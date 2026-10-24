"""Bounded diagnostic through existing GUI/XPC; never invoke while hour owns lab.
No direct Wine spawn, native counter writes, or inferred kernel-park evidence.
"""
from pathlib import Path
import argparse,fcntl,hashlib,json,os,signal,stat,subprocess,time,uuid

repo=Path(__file__).resolve().parents[2]
root=repo/'out/waitall-live-hint-reap'
lab=repo/'out/waitall-mac-20261018-admission'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

class ObservationError(RuntimeError): pass

def host_clock_gap(start,wall):
    return (time.time()-wall)-(time.monotonic()-start)

def acceptance_window_valid(start,wall):
    return time.monotonic()-start<=150 and abs(host_clock_gap(start,wall))<=2

def drain_client(cli):
    """Collect exited clients too, including retained partial pipe output."""
    result={}
    def partial(value):return value.decode(errors='replace')if isinstance(value,bytes)else value or''
    try:out,err=cli.communicate(timeout=2)
    except subprocess.TimeoutExpired:
        if cli.poll()is None:cli.terminate()
        try:out,err=cli.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            if cli.poll()is None:cli.kill()
            try:out,err=cli.communicate(timeout=5)
            except subprocess.TimeoutExpired as error:
                result.update(clientDrainIncomplete=True,stdout=partial(error.output),stderr=partial(error.stderr))
                for stream in [cli.stdout,cli.stderr]:stream.close()
                cli.wait(timeout=1)
                return result
    result.update(stdout=out,stderr=err)
    return result

def decoded(log):
    raw=log.read_bytes()
    if len(raw)>4*1024*1024:raise ObservationError('log limit exceeded')
    rows=[];position=0
    for line in raw.splitlines(keepends=True):
        start=position;position+=len(line)
        if not line.endswith(b'\n'):break
        if not line.startswith(b'{'):continue
        try:value=json.loads(line)
        except ValueError:raise ObservationError('malformed complete JSON log line')
        rows.append((start,value))
    return raw,rows

class Observer:
    def __init__(self,prefix,control,nonce,rounds,bits):
        self.prefix,self.control,self.nonce,self.rounds,self.bits=prefix,control,nonce,rounds,bits
        self.round=0;self.stage='mapped';self.floor=0;self.deadline=time.monotonic()+15
        self.begin=None;self.target=None;self.identity=None;self.producer=None;self.index=None
        self.current={};self.evidence=[];self.zeros=[];self.complete=None

    def check_deadline(self):
        now=time.monotonic()
        if now>self.deadline:
            if self.stage=='cleared' and self.current.get('afterJoin') and all(x['value']['eventHintValue']==1 for x in self.current['afterJoin']):raise ObservationError('hint_still_positive_after_join')
            raise ObservationError('observer phase timeout: '+self.stage)
        return now

    def command(self,kind):
        temporary=self.control.with_name(self.control.name+'.pending')
        fd=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
        try:
            with os.fdopen(fd,'w')as stream:
                stream.write(f'{self.nonce}|{kind}|{self.round}\n');stream.flush();os.fsync(stream.fileno())
            os.replace(temporary,self.control)
        finally:temporary.unlink(missing_ok=True)

    def snapshot(self,trace):
        wanted=(trace.get('coord_dev'),trace.get('coord_ino'))
        if any(type(x)is not int or x<0 for x in wanted):raise ObservationError('missing native file identity')
        matches=[]
        for path in self.prefix.glob('.cider-msync-coord-*'):
            item=path.lstat()
            if stat.S_ISREG(item.st_mode) and not path.is_symlink() and (item.st_dev,item.st_ino)==wanted:matches.append(path)
        if len(matches)!=1:raise ObservationError('cannot uniquely bind actual coordinator inode')
        path=matches[0]
        if self.identity is not None and wanted!=self.identity:raise ObservationError('coordinator identity changed')
        if type(trace.get('idx'))is not int or not 0<trace['idx']<=0x0fffffff:raise ObservationError('invalid native index')
        run=subprocess.run([str(root/'hint-snapshot'),str(path),'--hint',str(trace['idx'])],capture_output=True,text=True,timeout=2)
        if run.returncode:raise ObservationError('reader unavailable: '+str(run.returncode))
        value=json.loads(run.stdout)
        if value.get('eventHintsAvailable')is not True or value.get('eventHintReadAtomic')is not True or value.get('eventHintIndex')!=trace['idx'] or type(value.get('eventHintValue'))is not int:raise ObservationError('hint value unavailable, not zero')
        if value.get('producerResourcesAvailable')is not True or not value.get('producerStartAbstime'):raise ObservationError('producer identity unavailable')
        producer=(value['pid'],value['producerStartAbstime'])
        if self.producer is not None and producer!=self.producer:raise ObservationError('producer incarnation changed')
        self.identity=wanted;self.producer=producer
        return {'time':time.monotonic(),'file':str(path),'identity':wanted,'value':value}

    def tick(self,log):
        raw,rows=decoded(log)
        now=time.monotonic()
        if self.begin is not None and now>self.deadline:
            if self.stage=='cleared' and self.current.get('afterJoin') and all(x['value']['eventHintValue']==1 for x in self.current['afterJoin']):raise ObservationError('hint_still_positive_after_join')
            raise ObservationError('observer phase timeout: '+self.stage)
        begins=[v for _,v in rows if v.get('type')=='begin' and v.get('nonce')==self.nonce]
        if len(begins)>1:raise ObservationError('duplicate begin')
        if not begins:return
        begin=begins[0]
        if begin.get('mode')!='live-hint-reap' or begin.get('pointer_bits')!=self.bits or begin.get('rounds')!=self.rounds or type(begin.get('pid'))is not int or type(begin.get('main_tid'))is not int:raise ObservationError('invalid begin')
        self.begin=begin
        objects=[(p,v)for p,v in rows if v.get('type')=='object' and v.get('nonce')==self.nonce]
        if len(objects)>1:raise ObservationError('duplicate target object')
        if not objects:return
        object_position,object_row=objects[0]
        handle=object_row.get('target_handle')
        if type(handle)is not int or handle<=0:raise ObservationError('invalid target handle')
        self.target=handle
        failures=[v for _,v in rows if v.get('type')=='failure' and v.get('nonce')==self.nonce]
        if failures:raise ObservationError('guest failure: '+json.dumps(failures[-1]))
        if self.round==self.rounds:
            completed=[v for _,v in rows if v.get('type')=='complete' and v.get('nonce')==self.nonce]
            if completed:
                if len(completed)!=1:raise ObservationError('duplicate complete')
                value=completed[0]
                if value.get('mode')!='live-hint-reap' or value.get('ok')is not True or any(value.get(k)!=self.rounds for k in ['rounds','created','terminated','joined']) or value.get('unexpected_returns')!=0 or value.get('output_error')is not False:raise ObservationError('invalid complete')
                self.complete=value
            return
        phases=[(p,v)for p,v in rows if p>=self.floor and v.get('type')=='phase' and v.get('nonce')==self.nonce and v.get('round')==self.round]
        def phase(name):
            found=[(p,v)for p,v in phases if v.get('stage')==name]
            if len(found)>1:raise ObservationError('duplicate phase '+name)
            return found[0]if found else None
        if self.stage=='mapped':
            mapped=phase('mapped')
            if not mapped:return
            candidates=[(p,v)for p,v in rows if self.floor<=p<mapped[0] and p>object_position and v.get('type')=='cider-live-hint' and v.get('phase')=='reset' and v.get('windows_pid')==begin['pid'] and v.get('windows_tid')==begin['main_tid'] and v.get('windows_handle')==handle]
            if len(candidates)!=1:raise ObservationError('reset mapping not unique in fresh round')
            _,trace=candidates[0];snapshot=self.snapshot(trace)
            if snapshot['value']['eventHintValue']!=0:raise ObservationError('baseline hint not zero; no worker started')
            if self.index is not None and trace['idx']!=self.index:raise ObservationError('persistent target index changed')
            self.index=trace['idx'];self.current={'round':self.round,'mapping':trace,'mappedPosition':mapped[0],'baseline':snapshot}
            now=self.check_deadline()
            self.command('START');self.stage='registered';self.deadline=now+5
        elif self.stage=='registered':
            created,ready=phase('created'),phase('ready')
            if not created or not ready:return
            if type(created[1].get('tid'))is not int or created[1]['tid']!=ready[1].get('tid') or created[0]>=ready[0]:raise ObservationError('invalid worker identity/order')
            tid=created[1]['tid']
            candidates=[(p,v)for p,v in rows if p>self.current['mappedPosition'] and v.get('type')=='cider-live-hint' and v.get('phase')=='registered' and v.get('windows_pid')==begin['pid'] and v.get('windows_tid')==tid and v.get('idx')==self.index and (v.get('coord_dev'),v.get('coord_ino'))==self.identity]
            if not candidates:return
            _,trace=candidates[-1]
            if type(trace.get('hint'))is not int or trace['hint']<=0:raise ObservationError('native registration not positive')
            snapshot=self.snapshot(trace)
            if snapshot['value']['eventHintValue']!=1:raise ObservationError('live target bucket not exactly one')
            self.current.update(tid=tid,registration=trace,registeredSnapshot=snapshot)
            now=self.check_deadline()
            self.command('TERMINATE');self.stage='joined';self.deadline=now+5
        elif self.stage=='joined':
            joined=phase('joined')
            if not joined:return
            if joined[1].get('tid')!=self.current['tid'] or joined[1].get('exit_code')!=42:raise ObservationError('wrong joined worker/exit')
            self.current['joined']=joined[1];self.stage='cleared';self.deadline=now+3;self.zeros=[]
        elif self.stage=='cleared':
            snapshot=self.snapshot(self.current['mapping']);self.current.setdefault('afterJoin',[]).append(snapshot)
            now=self.check_deadline()
            value=snapshot['value']['eventHintValue']
            if value==0:
                if not self.zeros or snapshot['time']-self.zeros[-1]['time']>=.02:self.zeros.append(snapshot)
                if len(self.zeros)>=2:
                    self.command('CLEARED');self.evidence.append(self.current);self.floor=len(raw);self.round+=1;self.stage='mapped';self.deadline=now+5
            else:self.zeros=[]
        if now>self.deadline:
            if self.stage=='cleared' and self.current.get('afterJoin') and all(x['value']['eventHintValue']==1 for x in self.current['afterJoin']):raise ObservationError('hint_still_positive_after_join')
            raise ObservationError('observer phase timeout: '+self.stage)

def main():
    global root,lab
    parser=argparse.ArgumentParser();parser.add_argument('--bottle',required=True);parser.add_argument('--arch',choices=['i686','x86_64'],required=True);parser.add_argument('--rounds',type=int,default=64);parser.add_argument('--label',required=True);parser.add_argument('--expected-leak',action='store_true');parser.add_argument('--root',type=Path,default=lab);parser.add_argument('--artifacts',type=Path,default=root);args=parser.parse_args();lab=args.root.resolve(strict=True);root=args.artifacts.resolve(strict=True)
    if not 1<=args.rounds<=64:raise SystemExit('invalid round bound')
    hour=json.loads((repo/'out/waitall-late-park/hour-state-v1.json').read_text())
    if hour['state']in ['starting','running'] or not hour.get('finalCleanup',{}).get('hostShutdown',{}).get('confirmed'):raise SystemExit('active hour or unresolved cleanup; no lab mutation')
    owner=json.loads((lab/'owner.json').read_text());assert owner['root']==str(lab)
    lock=os.open(lab/'wine-runtime.lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600);fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    configurations=[(p,json.loads(p.read_text()))for p in (lab/'private-home/AppSupport/Bottles').glob('*/cider-bottle.json')]
    chosen=[(p,c)for p,c in configurations if c['name']==args.bottle];assert len(chosen)==1;config_path,config=chosen[0];prefix=(config_path.parent/'prefix').resolve();assert prefix.is_relative_to(lab)and config['engine']['id'].startswith('cider-lab-')
    nonce=uuid.uuid4().hex;run_id=str(uuid.UUID(nonce));exe=root/('live-hint-reap-v4-'+args.arch+'.exe');destination=prefix/'drive_c/CiderTests'/exe.name;destination.parent.mkdir(exist_ok=True);assert destination.parent.resolve().is_relative_to(prefix/'drive_c');destination.write_bytes(exe.read_bytes());control=destination.parent/('live-hint-'+nonce+'.ctl')
    app=lab/'source/out/Cider.app';env={k:os.environ[k]for k in ['HOME','USER','LOGNAME','PATH','TMPDIR']if k in os.environ};env.update(CIDER_HOME=str(lab/'private-home'),CIDER_APP=str(app));engine=lab/'private-home/AppSupport/Engines'/config['engine']['id']/'engine';native={str(p.relative_to(engine)):sha(p)for p in [engine/'bin/wineserver',engine/'lib/wine/x86_64-unix/ntdll.so',engine/'lib/wine/i386-windows/ntdll.dll',engine/'lib/wine/x86_64-windows/ntdll.dll']}
    expected_build=root/('negative-build-result-v2.json'if args.expected_leak else'build-result-v3.json')
    expected=json.loads(expected_build.read_text())
    if not expected['passed'] or native['bin/wineserver']!=expected['nativeSHA256']['wineserver'] or native['lib/wine/x86_64-unix/ntdll.so']!=expected['nativeSHA256']['ntdll.so']:
        raise ObservationError('installed diagnostic pair does not match expected build')
    if native['lib/wine/i386-windows/ntdll.dll']!='8ccc2454f3afcdc0d6114b3f727a24acf5736c794f5be6d1180a46ff8cab8650' or native['lib/wine/x86_64-windows/ntdll.dll']!='c2203764cfddd2194e9fe2c80dbd1cbc852ce651acdfeec2ef03e6c6f623c8b0':
        raise ObservationError('unexpected diagnostic PE ntdll')
    built=json.loads((root/'client-build-v4.json').read_text())
    binary_record=next(x for x in built['builds']if x['arch']==args.arch)
    if not built['passed'] or sha(exe)!=binary_record['exeSHA256'] or sha(destination)!=sha(exe):
        raise ObservationError('unexpected diagnostic executable')
    result=root/(args.label+'-'+nonce+'.json')
    record={'scope':'live hint registration before actual owned Win32 termination, not raw-kernel park/Windows/performance/hour proof',
            'state':'running','passed':False,'nativeSHA256':native,'exeSHA256':sha(exe),
            'readerSHA256':sha(root/'hint-snapshot'),'runID':run_id,'nonce':nonce,
            'observerSHA256':sha(Path(__file__)),'args':{k:str(v)if isinstance(v,Path)else v for k,v in vars(args).items()},'windowsOperations':0}
    result.write_text(json.dumps(record,indent=2)+'\n')
    observer=Observer(prefix,control,nonce,args.rounds,32 if args.arch=='i686' else 64)
    cli=None;log=None;start=time.monotonic();wall=time.time();stdout=stderr='';client_drained=False
    try:
        command=[str(app/'Contents/MacOS/ciderctl'),'run','-b',args.bottle,'--wait','--debug','quiet','--env','WAITALL_RUN_ID='+run_id,'--env','WINEMSYNC_STATS=1','--env','WINEMSYNC_TRACE_WAIT=1','C:\\CiderTests\\'+exe.name,'C:\\CiderTests\\'+control.name,nonce,str(args.rounds)]
        record['command']=command;cli=subprocess.Popen(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        while True:
            active=time.monotonic()-start;elapsed=time.time()-wall
            if active>150 or abs(elapsed-active)>2:raise ObservationError('outer timeout or host clock gap')
            if log is None:
                found=[]
                for manifest in (lab/'private-home/Logs/sessions'/config['id']).glob('*/session.json'):
                    if manifest.stat().st_size>1024*1024:continue
                    plan=json.loads(manifest.read_text())
                    if plan.get('env',{}).get('WAITALL_RUN_ID')==run_id and plan.get('bottleID')==config['id'] and plan.get('engineID')==config['engine']['id']:found.append(manifest.with_name('wine.log'))
                if len(found)>1:raise ObservationError('ambiguous session binding')
                if found:log=found[0]
            if log is not None:observer.tick(log)
            if abs(host_clock_gap(start,wall))>2:raise ObservationError('host clock gap after observation')
            try:stdout,stderr=cli.communicate(timeout=.05);client_drained=True
            except subprocess.TimeoutExpired:continue
            if log is not None:observer.tick(log)
            if abs(host_clock_gap(start,wall))>2:raise ObservationError('host clock gap at final completion')
            break
        if cli.returncode or not observer.complete or len(observer.evidence)!=args.rounds:raise ObservationError('incomplete guest/registration observations')
        if args.expected_leak:raise ObservationError('negative unexpectedly cleared registrations')
        if not acceptance_window_valid(start,wall):raise ObservationError('outer timeout or host clock gap before acceptance')
        record.update(state='complete',passed=True,guestComplete=observer.complete,acceptedAtActiveSeconds=time.monotonic()-start)
    except BaseException as error:
        reason=str(error);record.update(state='failed',passed=False,error=repr(error),negativeControlMet=args.expected_leak and reason=='hint_still_positive_after_join' and acceptance_window_valid(start,wall),observedFailureAtActiveSeconds=time.monotonic()-start)
        try:observer.command('STOP')
        except Exception as stop_error:record['stopCommandError']=repr(stop_error)
    finally:
        try:
            if cli is not None and not client_drained:
                drained=drain_client(cli)
                stdout,stderr=drained.pop('stdout'),drained.pop('stderr')
                record.update(drained)
        except Exception as drain_error:
            record.update(passed=False,negativeControlMet=False,clientDrainError=repr(drain_error))
        try:
            cleanup=subprocess.run([str(lab/'owned-processes'),'stop-runtime',str(lab)],capture_output=True,text=True,timeout=30)
            receipt=json.loads(cleanup.stdout)
            record['cleanup']={'exitCode':cleanup.returncode,'receipt':receipt}
            if cleanup.returncode or receipt['pending'] or not receipt.get('hostShutdown',{}).get('confirmed') or receipt.get('emptyObservations',0)<2:
                record.update(passed=False,negativeControlMet=False,cleanupUnconfirmed=True)
        except Exception as cleanup_error:
            record.update(passed=False,negativeControlMet=False,cleanupUnconfirmed=True,cleanupError=repr(cleanup_error))
        gap=host_clock_gap(start,wall)
        if abs(gap)>2:record.update(passed=False,negativeControlMet=False,timingDiscontinuity=True)
        record['hostClockGapSeconds']=gap
        record.update(cliExitCode=cli.returncode if cli else None,stdout=stdout,stderr=stderr,observations=observer.evidence,currentRound=observer.current,activeSeconds=time.monotonic()-start,wallSeconds=time.time()-wall)
        if log is not None:record.update(log=str(log),logSHA256=sha(log))
        for path in [control,control.with_name(control.name+'.pending')]:path.unlink(missing_ok=True)
        result.write_text(json.dumps(record,indent=2)+'\n');os.close(lock)
    print(json.dumps({'result':str(result),'passed':record['passed'],'negativeControlMet':record.get('negativeControlMet',False),'error':record.get('error')}))
    raise SystemExit(0 if record['passed']or record.get('negativeControlMet')else 1)

if __name__=='__main__':
    def requested_stop(signum,frame):raise KeyboardInterrupt('requested diagnostic stop')
    signal.signal(signal.SIGTERM,requested_stop)
    main()
