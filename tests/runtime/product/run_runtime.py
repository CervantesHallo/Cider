import ctypes, datetime, hashlib, json, os, plistlib, re, signal, subprocess, sys, time, fcntl, struct
from pathlib import Path
BASE=(Path(sys.argv[sys.argv.index('--root')+1]).resolve() if '--root' in sys.argv else Path(__file__).resolve().parent)
OWNER=json.loads((BASE/'owner.json').read_text())
assert BASE.name.startswith('runtime-acceptance-') and Path(OWNER['root']).resolve()==BASE
APP=BASE/'Cider.app';CLI=APP/'Contents/MacOS/ciderctl';TEST_HOME=Path(OWNER['home'])
assert TEST_HOME.resolve().parent==BASE and Path(OWNER['engine_copy']).resolve().is_relative_to(BASE)
assert not APP.is_symlink() and APP.resolve().is_relative_to(BASE)
CONTROLLER_LOCK=os.open(BASE/'controller.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
fcntl.flock(CONTROLLER_LOCK,fcntl.LOCK_EX|fcntl.LOCK_NB)
ENV={key:os.environ[key] for key in ['HOME','USER','LOGNAME','TMPDIR','PATH'] if key in os.environ};ENV['CIDER_HOME']=str(TEST_HOME)
RESULT={'schema':'cider.product-runtime-acceptance/v1','startedUTC':datetime.datetime.utcnow().isoformat()+'Z','owner':OWNER,'cases':[],'windowsOperations':0,'clients':[]}
CHILDREN=[];BOTTLE='Runtime Acceptance';DEADLINE=time.monotonic()+240
LIB=ctypes.CDLL('/usr/lib/libproc.dylib')
LIB.proc_listallpids.argtypes=[ctypes.c_void_p,ctypes.c_int];LIB.proc_listallpids.restype=ctypes.c_int
LIB.proc_pidpath.argtypes=[ctypes.c_int,ctypes.c_void_p,ctypes.c_uint];LIB.proc_pidpath.restype=ctypes.c_int

def run(args,timeout=20,env=None):
 return subprocess.run([str(a) for a in args],capture_output=True,text=True,timeout=timeout,env=env)
def cli(args,timeout=30):
 if time.monotonic()>DEADLINE:raise TimeoutError('owned runtime batch deadline')
 return run([CLI,*args],timeout=timeout,env=ENV)
def require(ok,message):
 if not ok:raise AssertionError(message)
def snapshot():
 count=LIB.proc_listallpids(None,0);pids=(ctypes.c_int*(count+128))();filled=LIB.proc_listallpids(pids,ctypes.sizeof(pids));rows=[]
 for pid in list(pids)[:filled]:
  if pid<2:continue
  buf=ctypes.create_string_buffer(4096)
  if LIB.proc_pidpath(pid,buf,len(buf))>0:
   path=buf.value.decode('utf-8','replace')
   if path.startswith(str(BASE)+'/'):rows.append({'pid':pid,'path':path})
 return rows
def host_info():
 p=run([BASE/'host-control','info',APP]);require(p.returncode==0,p.stderr);return json.loads(p.stdout)
def audits():
 p=TEST_HOME/'Logs/audit/spawn.jsonl'
 return [json.loads(line) for line in p.read_text().splitlines()] if p.exists() else []
def wait_for(predicate,seconds=10):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  value=predicate()
  if value:return value
  time.sleep(.05)
 raise TimeoutError('owned runtime observation timed out')
def probe_record():
 hits=[]
 for log in (TEST_HOME/'Logs/sessions').glob('*/*/wine.log'):
  for line in log.read_text(errors='replace').splitlines():
   if line.startswith('{"kind":"cider-owned-win32-probe"'):
    hits.append((log.stat().st_mtime_ns,log,json.loads(line)))
 return sorted(hits,key=lambda v:v[0])[-1] if hits else None
def case(name,body):
 start=time.monotonic()
 try:evidence=body();RESULT['cases'].append({'name':name,'passed':True,'seconds':time.monotonic()-start,'evidence':evidence})
 except Exception as error:
  RESULT['cases'].append({'name':name,'passed':False,'seconds':time.monotonic()-start,'error':str(error)});raise
 finally:(BASE/'runtime-result.json').write_text(json.dumps(RESULT,indent=2,ensure_ascii=False))

def engine_list():
 p=cli(['engine','list']);require(p.returncode==0,p.stderr);require(OWNER['engine_id'] in p.stdout,'engine not found')
 info=host_info();require(len(info)==1,'expected the exact private product GUI')
 RESULT['guiPID']=info[0]['pid'];return {'stdout':p.stdout,'gui':info}
def create_bottle():
 existing=list((TEST_HOME/'AppSupport/Bottles').glob('*/cider-bottle.json'))
 if '--resume' in sys.argv and existing:
  require(len(existing)==1,'resume requires exactly one owned test bottle')
  config=json.loads(existing[0].read_text());require(config['name']==BOTTLE,'foreign bottle in private root')
  RESULT['bottleID']=config['id'];RESULT['prefix']=str(existing[0].parent/'prefix')
  return {'reusedOnlyThisOwnedEmptyPrefix':True,'bottleID':config['id']}
 p=cli(['bottle','create',BOTTLE,'--locale','zh-Hans','--engine',OWNER['engine_id']],timeout=100)
 require(p.returncode==0,p.stdout+p.stderr)
 files=list((TEST_HOME/'AppSupport/Bottles').glob('*/cider-bottle.json'));require(len(files)==1,'expected one owned bottle')
 config=json.loads(files[0].read_text());RESULT['bottleID']=config['id'];RESULT['prefix']=str(files[0].parent/'prefix')
 return {'stdout':p.stdout,'stderr':p.stderr,'bottleID':config['id']}
def exact_arguments():
 cwd=BASE/'caller dir 中文';cwd.mkdir(exist_ok=True)
 args=['alpha beta','','中文','quote"tail','backslash\\','tab\tvalue','--exit','37']
 p=cli(['run','-b',BOTTLE,'--wait','--cwd',str(cwd),'--env','ACCEPTANCE_VALUE=round-trip',str(BASE/'probe.exe'),'--',*args],timeout=40)
 require(p.returncode==37,p.stdout+p.stderr)
 found=wait_for(probe_record);record=found[2]
 require(record['arguments']==args,'argument mismatch: '+repr(record))
 require(record['fixtureEnvironment']=='round-trip','explicit environment not propagated')
 require(record['cwd'].endswith('caller dir 中文'),'caller cwd not propagated')
 return {'cliExitCode':p.returncode,'stdout':p.stdout,'probe':record,'log':str(found[1])}
def server_lock_observation():
 try:
  st=os.stat(RESULT['prefix']);lock=Path('/tmp')/('.wine-'+str(os.getuid()))/('server-'+format(st.st_dev,'x')+'-'+format(st.st_ino,'x'))/'lock'
  fd=os.open(lock,os.O_RDWR)
  try:fields=struct.unpack('qqihh',fcntl.fcntl(fd,fcntl.F_GETLK,struct.pack('qqihh',0,0,0,fcntl.F_WRLCK,0)))
  finally:os.close(fd)
  pid=fields[2];buf=ctypes.create_string_buffer(4096);LIB.proc_pidpath(pid,buf,len(buf)) if pid>1 else None
  return {'lockPath':str(lock),'type':fields[3],'holderPID':pid,'holderExecutable':buf.value.decode('utf-8','replace')}
 except Exception as error:return {'error':str(error)}

def wait_stop():
 before=len(audits());started_ns=time.time_ns()
 handle=subprocess.Popen([str(CLI),'run','-b',BOTTLE,'--wait',str(BASE/'probe.exe'),'--','--hold','30000'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=ENV,start_new_session=True)
 CHILDREN.append(handle)
 rows=wait_for(lambda:audits()[before:] if len(audits())>before else None)
 pid=int(rows[0]['pid']);parent=run(['/bin/ps','-p',str(pid),'-o','ppid=,comm='])
 ppid=int(parent.stdout.strip().split(None,1)[0]);require(ppid==RESULT['guiPID'],'Wine was not spawned by product GUI')
 ready=None if '--early-stop' in sys.argv else wait_for(lambda:(probe_record() if probe_record() and probe_record()[2]['arguments']==['--hold','30000'] and probe_record()[0]>=started_ns else None),seconds=15)
 RESULT['spawnParentEvidence']={'unixWinePID':pid,'nativePPID':ppid,'guiPID':RESULT['guiPID'],'parentObservation':parent.stdout,'probeReady':ready[2] if ready else None,'stopDuringGuestStartup':'--early-stop' in sys.argv}
 owned=snapshot();require(any(r['pid']==pid and r['path'].startswith(OWNER['engine_copy']+'/') for r in owned),'spawn PID not in private engine')
 if '--early-stop' not in sys.argv:
  ps=cli(['ps','--all']);require(ps.returncode==0,ps.stderr)
  ps_rows=[line.split('\t',2) for line in ps.stdout.splitlines() if line]
  require(all(len(row)==3 and row[1]==RESULT['bottleID'] for row in ps_rows),'ps crossed this data root')
  require(any(int(row[0])==pid for row in ps_rows),'ps missed the current owned Windows process')
  RESULT['scopedPS']={'exitCode':ps.returncode,'rows':ps_rows}

 start=time.monotonic();stopped=cli(['kill','-b',BOTTLE],timeout=20);stop_returned=time.monotonic()
 RESULT['immediateStopReply']={'exitCode':stopped.returncode,'stdout':stopped.stdout,'stderr':stopped.stderr,'seconds':stop_returned-start,'ownProcesses':snapshot(),'serverLock':server_lock_observation()}
 out,err=handle.communicate(timeout=20)
 RESULT['stopAttempt']={'exitCode':stopped.returncode,'stdout':stopped.stdout,'stderr':stopped.stderr,'waitingCLIExitCode':handle.returncode,'waitingCLIStdout':out,'waitingCLIStderr':err,'liveOwnedProcessesAfterStop':snapshot()}
 require(stopped.returncode==0,stopped.stdout+stopped.stderr)
 return {'unixWinePID':pid,'nativePPID':ppid,'guiPID':RESULT['guiPID'],'parentObservation':parent.stdout,'stopRequestSeconds':stop_returned-start,'stopAndWaiterCompletionSeconds':time.monotonic()-start,'stopStdout':stopped.stdout,'waitingCLIExitCode':handle.returncode,'waitingCLIStdout':out,'waitingCLIStderr':err}
def detach_wait():
 before=len(audits());started_ns=time.time_ns()
 handle=subprocess.Popen([str(CLI),'run','-b',BOTTLE,'--wait',str(BASE/'probe.exe'),'--','--hold','30000'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=ENV,start_new_session=True)
 CHILDREN.append(handle)
 rows=wait_for(lambda:audits()[before:] if len(audits())>before else None)
 pid=int(rows[0]['pid']);parent=run(['/bin/ps','-p',str(pid),'-o','ppid=,comm='])
 ppid=int(parent.stdout.strip().split(None,1)[0]);require(ppid==RESULT['guiPID'],'Wine was not spawned by product GUI')
 ready=wait_for(lambda:(probe_record() if probe_record() and probe_record()[2]['arguments']==['--hold','30000'] and probe_record()[0]>=started_ns else None),seconds=15)
 require(handle.poll() is None,'CLI exited before observer detach')
 handle.terminate();out,err=handle.communicate(timeout=4)
 require(handle.returncode==-signal.SIGTERM,'observer CLI was not terminated as requested')
 live=lambda:any(row['pid']==pid and row['path'].startswith(OWNER['engine_copy']+'/') for row in snapshot())
 require(live(),'disconnect terminated the launched program')
 start=time.monotonic();fresh=cli(['engine','list'],timeout=8)
 require(fresh.returncode==0 and OWNER['engine_id'] in fresh.stdout,fresh.stdout+fresh.stderr)
 time.sleep(.3);require(live(),'program stopped after detached waiter cleanup')
 ps=cli(['ps','--all'],timeout=8);require(ps.returncode==0 and any(line.split('\t',1)[0]==str(pid) for line in ps.stdout.splitlines()),'detached program disappeared from product ps')
 return {'unixWinePID':pid,'nativePPID':ppid,'guiPID':RESULT['guiPID'],'probeReady':ready[2],
         'observerCLIExitCode':handle.returncode,'programStillAliveAfterDetach':True,'freshCommandExitCode':fresh.returncode,
         'freshCommandSeconds':time.monotonic()-start,'stdout':out,'stderr':err,'laterTermination':'explicit normal bottle cleanup only'}
def relaunch():
 p=cli(['run','-b',BOTTLE,'--wait',str(BASE/'probe.exe'),'--','--exit','0'],timeout=35)
 require(p.returncode==0,p.stdout+p.stderr);return {'stdout':p.stdout,'exitCode':p.returncode,'probe':probe_record()[2]}

def window_audio():
 before=len(audits());started_ns=time.time_ns();program=BASE/('window-audio-i386.exe' if '--32' in sys.argv else 'window-audio.exe');stdout=BASE/'window-audio-cli.stdout';stderr=BASE/'window-audio-cli.stderr'
 with stdout.open('w') as out,stderr.open('w') as err:
  handle=subprocess.Popen([str(CLI),'run','-b',BOTTLE,'--wait',str(program)],stdout=out,stderr=err,text=True,env=ENV,start_new_session=True)
  CHILDREN.append(handle)
  rows=wait_for(lambda:audits()[before:] if len(audits())>before else None)
  pid=int(rows[0]['pid'])
  def native_windows():
   pids=[str(row['pid']) for row in snapshot() if row['path'].startswith(OWNER['engine_copy']+'/')]
   if not pids:return []
   rows=json.loads(run([BASE/'host-control','windows',','.join(pids)],timeout=3).stdout or '[]')
   return [r for r in rows if 600<=float(r['bounds'].get('Width',0))<=680 and 300<=float(r['bounds'].get('Height',0))<=400]
  info=wait_for(native_windows,seconds=15)
  handle.wait(timeout=25)
 records=[]
 for log in (TEST_HOME/'Logs/sessions').glob('*/*/wine.log'):
  session=log.parent/'session.json'
  if log.stat().st_mtime_ns<started_ns or not session.exists() or json.loads(session.read_text())['argv'][0]!=str(program):continue
  for line in log.read_text(errors='replace').splitlines():
   if line.startswith('{"kind":"window-') or line.startswith('{"kind":"audio-') or line.startswith('{"kind":"winmm-module"'):records.append(json.loads(line))
 require(handle.returncode==0,stdout.read_text()+stderr.read_text())
 module=[r for r in records if r['kind']=='winmm-module'][-1];require(module['candidateModuleLoaded'],'new candidate module was not actually loaded')
 paint=[r for r in records if r['kind']=='window-result'][-1];audio=[r for r in records if r['kind']=='audio-result'][-1]
 require(paint['paintCount']>0 and paint['surfacePixelMatches']>0,'Win32 paint/readback failed')
 require(audio['bufferCompleted'] and not audio['openCode'] and not audio['writeCode'],'render buffer not completed')
 return {'program':str(program),'nativePID':pid,'nativeOwnedWindows':info,'win32Paint':paint,'loadedModule':module,'silentOutputBuffer':audio,'cliExitCode':handle.returncode,'noMicrophoneOrCameraRequested':True}

try:
 require(not host_info(),'private product GUI already exists')
 case('product.cli_launches_real_gui',engine_list)
 case('product.reuse_empty_prefix' if '--resume' in sys.argv else 'product.bottle_create_from_gui',create_bottle)
 if '--detach' in sys.argv:
  case('product.cli_detach_keeps_program_and_frees_wait',detach_wait)
 elif '--early-stop' in sys.argv:
  case('product.stop_during_guest_startup',wait_stop)
  case('product.relaunch_after_early_stop',relaunch)
 elif '--only-ui' in sys.argv:
  case('product.win32_window_and_audio_output',window_audio)
 else:
  case('product.arguments_cwd_environment_and_exit37',exact_arguments)
  case('product.wait_can_be_stopped_and_gui_is_parent',wait_stop)
  case('product.relaunch_after_stop',relaunch)
except Exception as error:RESULT['failure']=str(error)
finally:
 errors=[]
 try:
  if list((TEST_HOME/'AppSupport/Bottles').glob('*/cider-bottle.json')):
   p=cli(['kill','-b',BOTTLE],timeout=20);RESULT['cleanupKill']={'exitCode':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
   if p.returncode:errors.append('product cleanup kill did not complete')
 except Exception as error:errors.append(str(error))
 for child in CHILDREN:
  try:
   if child.poll() is None:child.terminate();child.communicate(timeout=3)
  except Exception as error:errors.append(str(error))
 try:
  p=run([BASE/'host-control','terminate',APP],timeout=10);RESULT['cleanupGUI']={'exitCode':p.returncode,'result':p.stdout,'stderr':p.stderr}
 except Exception as error:errors.append(str(error))
 jobs=[]
 for file in (TEST_HOME/'AppSupport/State/XPC').glob('org.cider.ipc.*.plist'):
  try:
   data=plistlib.loads(file.read_bytes());args=data['ProgramArguments'];label=data['Label']
   require(Path(args[0]).resolve()==(APP/'Contents/MacOS/CiderXPCBridge').resolve() and Path(args[2]).resolve()==(TEST_HOME/'AppSupport').resolve() and file.stem==label,'job cleanup ownership mismatch')
   p=run(['/bin/launchctl','bootout',f'gui/{os.getuid()}/{label}'],timeout=8)
   absent=run(['/bin/launchctl','print',f'gui/{os.getuid()}/{label}'],timeout=8).returncode!=0
   jobs.append({'label':label,'bootoutCode':p.returncode,'absent':absent})
   if not absent:errors.append('owned job remains registered')
  except Exception as error:errors.append(str(error))
 support=Path('/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister')
 p=run([support,'-u',APP],timeout=10);RESULT['launchServicesUnregister']={'exitCode':p.returncode,'stderr':p.stderr}
 engine_host=Path(OWNER['engine_copy'])/'CiderWineHost.app'
 if engine_host.is_dir():
  require(engine_host.resolve().is_relative_to(BASE),'engine host cleanup escaped private root')
  p=run([support,'-u',engine_host],timeout=10);RESULT['engineLaunchServicesUnregister']={'exitCode':p.returncode,'stderr':p.stderr}
  if p.returncode:
   # Console Wine may never register this App. -10814 means not found; verify
   # the exact private path is absent instead of treating it as a live registration.
   dump=run([support,'-dump'],timeout=30)
   absent=dump.returncode==0 and str(engine_host) not in dump.stdout and str(engine_host.resolve()) not in dump.stdout
   RESULT['engineLaunchServicesUnregister']['notFoundAndExactPathAbsent']='-10814' in p.stderr and absent
   if not RESULT['engineLaunchServicesUnregister']['notFoundAndExactPathAbsent']:errors.append('private engine LaunchServices cleanup failed')
 time.sleep(.5);live=snapshot()
 if live:errors.append('owned processes remain; preserve root for cleanup')
 protected={str(path):hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in OWNER['protected']}
 require(protected==OWNER['protected'],'protected data changed')
 RESULT['cleanup']={'jobs':jobs,'liveProcesses':live,'errors':errors,'passed':not errors,'protectedSHA256':protected}
 RESULT['passed']=not RESULT.get('failure') and all(c['passed'] for c in RESULT['cases']) and not errors
 RESULT['finishedUTC']=datetime.datetime.utcnow().isoformat()+'Z'
 (BASE/'runtime-result.json').write_text(json.dumps(RESULT,indent=2,ensure_ascii=False));print(json.dumps(RESULT,indent=2,ensure_ascii=False));os.close(CONTROLLER_LOCK)
