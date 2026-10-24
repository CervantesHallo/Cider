"""Pure observer protocol doubles. No Wine, GUI, native counter or remote work."""
from pathlib import Path
import hashlib,importlib.util,json,tempfile,time,types,unittest.mock
r=Path(__file__).resolve().parents[2]/'out/waitall-live-hint-reap'
spec=importlib.util.spec_from_file_location('live_observer',Path(__file__).with_name('run-live-hint-reap.py'));module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
nonce='0123456789abcdef0123456789abcdef';checks=[]
def check(name,value):
    checks.append({'name':name,'passed':bool(value)})
    assert value,name
with tempfile.TemporaryDirectory(prefix='cider-observer-protocol-')as directory:
    root=Path(directory);log=root/'observations.log';file=root/'.cider-msync-coord-fixture';file.write_bytes(b'not-a-native-coordinator');file.chmod(0o600);info=file.stat();identity={'coord_dev':info.st_dev,'coord_ino':info.st_ino}
    rows=[]
    def append(**row):
        rows.append(row);log.write_text(''.join(json.dumps(x)+'\n'for x in rows))
    class Fake(module.Observer):
        def __init__(self):
            super().__init__(root,root/'unused-control',nonce,2,64);self.commands=[];self.values=[];self.fake_time=time.monotonic()
        def command(self,kind):self.commands.append((self.round,kind))
        def snapshot(self,trace):
            value=self.values.pop(0);self.identity=(trace['coord_dev'],trace['coord_ino']);self.fake_time+=.03
            return {'time':self.fake_time,'value':{'eventHintValue':value}}
    obs=Fake()
    append(type='begin',mode='live-hint-reap',pointer_bits=64,pid=32,main_tid=36,rounds=2,nonce=nonce)
    append(type='object',target_handle=128,nonce=nonce)
    append(type='cider-live-hint',phase='reset',windows_pid=32,windows_tid=36,windows_handle=128,idx=7,**identity)
    append(type='phase',stage='mapped',round=0,nonce=nonce)
    obs.values=[0];obs.tick(log);check('start_only_after_zero_mapping',obs.commands==[(0,'START')])
    append(type='phase',stage='created',round=0,tid=40,nonce=nonce)
    append(type='phase',stage='ready',round=0,tid=40,nonce=nonce)
    obs.tick(log);check('ready_is_not_registration',obs.commands==[(0,'START')])
    append(type='cider-live-hint',phase='registered',windows_pid=32,windows_tid=41,idx=7,hint=1,**identity)
    obs.tick(log);check('wrong_tid_cannot_authorize',obs.commands==[(0,'START')])
    append(type='cider-live-hint',phase='registered',windows_pid=32,windows_tid=40,idx=7,hint=1,**identity)
    obs.values=[1];obs.tick(log);check('one_live_registration_authorizes',obs.commands[-1]==(0,'TERMINATE'))
    append(type='phase',stage='joined',round=0,tid=40,exit_code=42,nonce=nonce)
    obs.tick(log);obs.values=[0,0];obs.tick(log);check('one_zero_does_not_ack',obs.commands[-1]==(0,'TERMINATE'));obs.tick(log);check('two_zeros_ack',obs.commands[-1]==(0,'CLEARED')and obs.round==1)
    append(type='cider-live-hint',phase='reset',windows_pid=32,windows_tid=36,windows_handle=128,idx=7,**identity)
    append(type='phase',stage='mapped',round=1,nonce=nonce)
    obs.values=[0];obs.tick(log)
    append(type='phase',stage='created',round=1,tid=40,nonce=nonce)
    append(type='phase',stage='ready',round=1,tid=40,nonce=nonce)
    obs.tick(log);check('reused_tid_requires_fresh_registration',obs.commands[-1]==(1,'START'))
    # Exercise the real availability checks, with subprocess output mocked only.
    real=module.Observer(root,root/'unused',nonce,1,64)
    response={'eventHintsAvailable':False,'eventHintIndex':7,'eventHintValue':0,'eventHintReadAtomic':True}
    with unittest.mock.patch.object(module.subprocess,'run',return_value=types.SimpleNamespace(returncode=0,stdout=json.dumps(response),stderr='')):
        try:real.snapshot({'idx':7,**identity});raised=False
        except module.ObservationError:raised=True
    check('unavailable_never_counts_as_zero',raised)
    obs.deadline=time.monotonic()-1
    try:obs.tick(log);raised=False
    except module.ObservationError:raised=True
    check('missing_phase_deadline_enforced',raised)
    # A second zero returned after the old deadline must never authorize ACK.
    obs.stage='cleared';obs.deadline=101;obs.current={'mapping':{'idx':7,**identity},'afterJoin':[]};obs.zeros=[{'time':99}]
    clock=[100.0]
    def late_read(trace):
        clock[0]=102.0
        return {'time':102.0,'value':{'eventHintValue':0}}
    obs.snapshot=late_read
    previous_commands=list(obs.commands)
    with unittest.mock.patch.object(module.time,'monotonic',side_effect=lambda:clock[0]):
        try:obs.tick(log);raised=False
        except module.ObservationError:raised=True
    check('late_second_zero_never_acks',raised and obs.commands==previous_commands)
    with unittest.mock.patch.object(module.time,'monotonic',return_value=20),unittest.mock.patch.object(module.time,'time',return_value=123):
        check('final_clock_gap_detectable',abs(module.host_clock_gap(10,100))>2)
    class Exited:
        def __init__(self):self.calls=0
        def poll(self):return 0
        def communicate(self,timeout):self.calls+=1;return 'retained stdout','retained stderr'
    exited=Exited();drained=module.drain_client(exited)
    check('exited_cli_still_drained',exited.calls==1 and drained['stdout']=='retained stdout')
    class Held(Exited):
        def __init__(self):
            super().__init__();self.closed=0
            self.stdout=self.stderr=types.SimpleNamespace(close=self.close)
        def close(self):self.closed+=1
        def communicate(self,timeout):
            self.calls+=1
            raise module.subprocess.TimeoutExpired('owned-client',timeout,output=b'partial stdout',stderr=b'partial stderr')
        def wait(self,timeout):return 0
    held=Held();drained=module.drain_client(held)
    check('descendant_pipe_preserves_partial_without_eof',drained.get('clientDrainIncomplete')is True and drained['stdout']=='partial stdout' and held.closed==2)
    with unittest.mock.patch.object(module.time,'monotonic',return_value=151),unittest.mock.patch.object(module.time,'time',return_value=251):
        check('late_outer_completion_not_accepted',not module.acceptance_window_valid(0,100))
result={'scope':'pure protocol/availability fault doubles only, not native or Wine validation','checks':checks,'passed':all(x['passed']for x in checks)}
result['observerSHA256']=hashlib.sha256((Path(__file__).with_name('run-live-hint-reap.py')).read_bytes()).hexdigest()
result['fixtureSHA256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
(r/'public-observer-protocol-validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
