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
if '--installer-exit' in sys.argv or '--stop-install' in sys.argv or '--stop-running-install' in sys.argv:
 data=BASE/'exit-status/recipe-data';require_marker=data/'owned-fixture-data'
 assert data.resolve().is_relative_to(BASE) and require_marker.is_file()
 ENV['CIDER_DATA']=str(data)
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
def exit_status_batch():
 import shutil,uuid
 source=BASE/'exit-status';directory=source/('collector-'+uuid.uuid4().hex[:12]);directory.mkdir()
 packaged=BASE/'Cider.app/Contents/Resources/windows-tools'
 for arch in ['x86_64','i686']:
  shutil.copy2(packaged/('cider-exit-status-'+arch+'.exe'),directory/('collector-'+arch+'.exe'))
  shutil.copy2(source/('exit-probe-'+arch+'.exe'),directory/('exit-probe-'+arch+'.exe'))
 cwd=directory/'路径 with spaces';cwd.mkdir(exist_ok=True)
 windows=lambda p:'Z:'+str(p).replace('/','\\')
 codes=[0,37,259,1603,1641,3010,0x80000100,0xc0000005,0xffffffff]
 evidence=[]
 for arch in ['x86_64','i686']:
  collector=directory/('collector-'+arch+'.exe');probe=directory/('exit-probe-'+arch+'.exe')
  require(collector.is_file() and probe.is_file(),'owned PE fixture missing')
  for code in codes:
   output=directory/(arch+'-'+format(code,'x')+'.json');require(not output.exists(),'result must be fresh')
   tail=[hex(code),'','中文 space','quote"tail','slash\\','tab\tvalue']
   result=cli(['run','-b',BOTTLE,'--wait','--cwd',str(cwd),'--env','EXIT_CAPTURE_VALUE=继承正常',str(collector),'--',windows(output),'30000',windows(cwd),windows(probe),*tail],timeout=18)
   require(result.returncode==0,result.stdout+result.stderr)
   status=json.loads(output.read_text());require(status['phase']=='captured' and status['windows_exit_code']==code and status['child_created'] and status['primary_terminated'] and not status['win32_error'],'full DWORD capture mismatch')
   log=Path(result.stdout.strip().split(' · log ',1)[1]);require(log.resolve().is_relative_to(TEST_HOME),'probe log escaped')
   records=[json.loads(line) for line in log.read_text(errors='replace').splitlines() if line.startswith('{"kind":"owned-exit-probe"')]
   require(len(records)==1 and records[0]['arguments']==tail and records[0]['environment']=='继承正常' and records[0]['cwd'].casefold()==windows(cwd).casefold(),'argv/cwd/environment mismatch')
   evidence.append({'arch':arch,'requestedDWORD':code,'collectorCLIExitCode':result.returncode,'report':status,'probe':records[0]})
  output=directory/(arch+'-timeout.json')
  result=cli(['run','-b',BOTTLE,'--wait',str(collector),'--',windows(output),'50','-',windows(probe),'0','--sleep'],timeout=18)
  status=json.loads(output.read_text());require(result.returncode!=0 and status['phase']=='timeout' and status['win32_error'] and status['primary_terminated'],'timeout reported success or left own primary live')
  evidence.append({'arch':arch,'negative':'timeout','collectorCLIExitCode':result.returncode,'report':status})
  bad=directory/(arch+'-bad.exe');bad.write_bytes(b'not an executable')
  output=directory/(arch+'-create-failed.json')
  result=cli(['run','-b',BOTTLE,'--wait',str(collector),'--',windows(output),'1000','-',windows(bad)],timeout=18)
  status=json.loads(output.read_text());require(result.returncode!=0 and status['phase']=='create-failed' and not status['child_created'],'bad image accepted as success')
  evidence.append({'arch':arch,'negative':'invalid-image','collectorCLIExitCode':result.returncode,'report':status})
  original=directory/(arch+'-old.json');original.write_text('{"old":"must remain"}')
  result=cli(['run','-b',BOTTLE,'--wait',str(collector),'--',windows(original),'1000','-',windows(probe),'0'],timeout=18)
  require(result.returncode!=0 and original.read_text()=='{"old":"must remain"}','old result was replaced or accepted')
  evidence.append({'arch':arch,'negative':'existing-report','collectorCLIExitCode':result.returncode,'oldResultPreserved':True})
 return {'checks':evidence,'count':len(evidence),'exceptionValuesAreExplicitExitProcessNotFaultInjection':True,'nativeWindowsReference':False}
def installer_exit_batch():
 import shutil,uuid,zipfile
 directory=BASE/'exit-status';data=directory/'recipe-data';nonce=uuid.uuid4().hex[:12]
 recipes=data/'recipes';profiles=data/'profiles';recipes.mkdir(exist_ok=True);profiles.mkdir(exist_ok=True)
 windows=lambda p:'Z:'+str(p).replace('/','\\')
 marker_dir=Path(RESULT['prefix'])/'drive_c/cider-exit-fixture';marker_dir.mkdir(exist_ok=True)
 def publish(recipe_id,source,kind,args,detect,convention=None):
  sha=hashlib.sha256(source.read_bytes()).hexdigest();filename='owned 安装.'+kind
  cache=TEST_HOME/'Caches/downloads/cas'/sha;cache.mkdir(parents=True,exist_ok=True)
  target=cache/filename
  if not target.exists():shutil.copy2(source,target)
  require(hashlib.sha256(target.read_bytes()).hexdigest()==sha,'fixture CAS mismatch')
  run={'source':'owned','kind':kind,'args':args}
  if convention:run['exit_code_convention']=convention
  profile='profile.'+recipe_id
  value={'schema':'cider.recipe/v1','id':recipe_id,'kind':'app','revision':1,'name':{'en':recipe_id,'zh-Hans':recipe_id},
   'sources':{'owned':{'urls':['https://example.invalid/owned.'+kind],'sha256':[sha],'filename':filename}},
   'steps':[{'run_installer':run,'environment_profile':profile}], 'detect':detect}
  (recipes/(recipe_id+'.json')).write_text(json.dumps(value,ensure_ascii=False))
  profile_value={'schema':'cider.profile/v1','id':profile,'revision':1,'target':recipe_id,'match':{'exe':'never-auto-match-owned.exe'},'actions':{'env':{'EXIT_CAPTURE_VALUE':recipe_id+' 继承'}}}
  (profiles/(profile+'.json')).write_text(json.dumps(profile_value,ensure_ascii=False))
  return recipe_id
 cases=[]
 for name,code,convention in [('zero',0,None),('lowbytezero',256,None),('exceptionvalue',0xc0000000,None),('reboot',3010,'msi'),('not-msi',3010,None),('zero32',0,None),('lowbytezero32',256,None)]:
  marker=marker_dir/(nonce+'-'+name+'.flag');require(not marker.exists(),'fixture marker must be fresh')
  winmarker='C:\\cider-exit-fixture\\'+marker.name
  recipe_id='fixture.'+nonce+'.'+name
  probe_arch='i686' if name.endswith('32') else 'x86_64'
  publish(recipe_id,directory/('exit-probe-'+probe_arch+'.exe'),'exe',[str(code),'--touch',winmarker],{'files':[winmarker]},convention)
  cases.append({'id':recipe_id,'kind':'exe','code':code,'succeeds':code==0 or convention=='msi','restart':convention=='msi','marker':marker})
 for mode,code in [('success',0),('reboot',3010),('fail',1603)]:
  package=directory/(nonce+'-'+mode+'.msi');key='Software\\CiderExitFixture\\'+nonce+'-'+mode
  built=cli(['run','-b',BOTTLE,'--wait',str(directory/'make-msi.exe'),'--',windows(package),mode,key],timeout=25)
  require(built.returncode==0 and package.is_file(),'own MSI construction failed: '+built.stdout+built.stderr)
  recipe_id='fixture.'+nonce+'.msi-'+mode
  publish(recipe_id,package,'msi',['/qn','REBOOT=ReallySuppress'],{'files':[],'registry_values':[{'key':'HKCU\\'+key,'name':'Installed','at_least':1}]})
  cases.append({'id':recipe_id,'kind':'msi','code':code,'succeeds':mode!='fail','restart':mode=='reboot'})
 records=[];capture_root=TEST_HOME/'Logs/exit-captures'/RESULT['bottleID'];session_root=TEST_HOME/'Logs/sessions'/RESULT['bottleID']
 for item in cases:
  before=set(capture_root.glob('capture-*'));sessions_before=set(session_root.iterdir())
  invoked=cli(['install',item['id'],'-b',BOTTLE],timeout=45)
  require((invoked.returncode==0)==item['succeeds'],'recipe result mismatch '+item['id']+': '+invoked.stdout+invoked.stderr)
  captures=set(capture_root.glob('capture-*'))-before;require(len(captures)==1,'expected one new capture outside sessions')
  capture=captures.pop();status=json.loads((capture/'result.json').read_text())
  require(status['windows_exit_code']==item['code'] and status['phase']=='captured','recipe lost full Windows code')
  actual_sessions=[p for p in set(session_root.iterdir())-sessions_before if (p/'session.json').is_file()]
  require(len(actual_sessions)==1,'capture scratch displaced real session namespace')
  session=actual_sessions[0]
  require((session/'exit-capture-request.json').read_bytes()==(capture/'request.json').read_bytes() and (session/'exit-capture-result.json').read_bytes()==(capture/'result.json').read_bytes(),'exit evidence not attached to actual session')
  if item['kind']=='exe':
   require(item['marker'].is_file(),'failure-with-file fixture did not create its detection marker')
   logs=[json.loads(line) for line in (session/'wine.log').read_text(errors='replace').splitlines() if line.startswith('{"kind":"owned-exit-probe"')]
   require(len(logs)==1 and logs[0]['environment']==item['id']+' 继承','recipe profile environment was not inherited')
  if item['succeeds']:require(('重启瓶子' in invoked.stderr)==item['restart'],'restart result was dropped or invented')
  else:require(str(item['code']) in invoked.stderr,'full DWORD absent from failure message')
  records.append({'recipe':item['id'],'kind':item['kind'],'expectedWindowsDWORD':item['code'],'cliExitCode':invoked.returncode,
   'requiresRestart':item['restart'],'detectMarkerExistsEvenOnFailure':item.get('marker',Path('/nonexistent-owned-marker')).is_file(),
   'report':status,'attachedToRealSession':True,'stdout':invoked.stdout,'stderr':invoked.stderr})
  RESULT['installerExitPartial']=records
 for i in range(6):
  legacy=session_root/('capture-legacy-'+str(i));legacy.mkdir(exist_ok=True)
  (legacy/'request.json').write_text('{"legacy":true}')
 support=directory/'support';support.mkdir(exist_ok=True)
 diagnostic=cli(['diag',BOTTLE,'--output',str(support)],timeout=20);require(diagnostic.returncode==0,diagnostic.stderr)
 archive=Path(diagnostic.stdout.strip())
 with zipfile.ZipFile(archive) as zipped:
  files=zipped.namelist()
  results=[name for name in files if name.endswith('/exit-capture-result.json')]
  logs=[name for name in files if name.endswith('/wine.log')]
  require(len(results)==5 and len(logs)==5,'support bundle omitted real sessions or attached exit reports')
  require(all(b'legacy' not in zipped.read(name) and not zipped.read(name).startswith(b'[missing') for name in logs),'legacy capture scratch displaced real logs')
 return {'installCases':records,'count':len(records),'supportBundleRealSessions':5,'supportBundleAttachedResults':5,'legacyScratchDirectoriesIgnored':6,'nativeWindowsReference':False}
def diagnose_msi():
 import uuid
 directory=BASE/'exit-status';nonce=uuid.uuid4().hex[:12];windows=lambda p:'Z:'+str(p).replace('/','\\')
 package=directory/('diagnostic-'+nonce+'.msi');key='Software\\CiderExitFixture\\diagnostic-'+nonce
 built=cli(['run','-b',BOTTLE,'--wait',str(directory/'make-msi.exe'),'--',windows(package),'success',key],timeout=25)
 require(built.returncode==0 and package.is_file(),'owned MSI diagnostic build failed')
 msi_log=directory/('msi-'+nonce+'.log');wrapper_log=directory/('wrapper-'+nonce+'.json')
 arguments=['/i',windows(package),'/qn','/l*v',windows(msi_log),'REBOOT=ReallySuppress']
 program='C:\\windows\\system32\\msiexec.exe'
 if '--wrapped-msi' in sys.argv:
  program=str(BASE/'Cider.app/Contents/Resources/windows-tools/cider-exit-status-x86_64.exe')
  arguments=[windows(wrapper_log),'30000','-', 'C:\\windows\\system32\\msiexec.exe',*arguments]
 handle=subprocess.Popen([str(CLI),'run','-b',BOTTLE,'--wait','--env','WINEDEBUG=+msi,+seh,+tid',program,'--',*arguments],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=ENV,start_new_session=True)
 CHILDREN.append(handle)
 try:
  out,err=handle.communicate(timeout=12)
  return {'route':'collector' if '--wrapped-msi' in sys.argv else 'direct','completedWithin12Seconds':True,'exitCode':handle.returncode,'stdout':out,'stderr':err,'msiLog':str(msi_log),'functionalMSIExitZero':handle.returncode==0}
 except subprocess.TimeoutExpired:
  rows=snapshot();ps=cli(['ps','--all'],timeout=8);targets=[]
  for line in ps.stdout.splitlines():
   fields=line.split('\t',2)
   if len(fields)==3 and 'msiexec' in fields[2].lower():
    pid=int(fields[0]);require(any(r['pid']==pid and r['path'].startswith(OWNER['engine_copy']+'/') for r in rows),'diagnostic target escaped owned engine')
    output=directory/('msi-'+nonce+'-sample-'+str(pid)+'.txt')
    sample=run(['/usr/bin/sample',str(pid),'3','1','-file',output],timeout=8)
    targets.append({'pid':pid,'image':fields[2],'sampleExitCode':sample.returncode,'sample':str(output)})
  return {'route':'collector' if '--wrapped-msi' in sys.argv else 'direct','completedWithin12Seconds':False,'functionalMSIExitZero':False,'ownedMSIProcessSamples':targets,'ps':ps.stdout,'msiLog':str(msi_log),'purpose':'diagnostic capture only; not a successful installer acceptance'}
def stop_installation():
 import uuid,shutil
 nonce=uuid.uuid4().hex[:12];base=BASE/'exit-status';data=base/'recipe-data';recipes=data/'recipes';recipes.mkdir(exist_ok=True)
 marker_dir=Path(RESULT['prefix'])/'drive_c/cider-exit-fixture';marker_dir.mkdir(exist_ok=True)
 pending=marker_dir/(nonce+'-pending.flag');late=marker_dir/(nonce+'-late.flag')
 win=lambda p:'C:\\cider-exit-fixture\\'+p.name
 probe=base/'exit-probe-x86_64.exe';sha=hashlib.sha256(probe.read_bytes()).hexdigest();cache=TEST_HOME/'Caches/downloads/cas'/sha;cache.mkdir(parents=True,exist_ok=True)
 cached=cache/'owned-stop.exe';shutil.copy2(probe,cached)
 source={'urls':['https://example.invalid/owned-stop.exe'],'sha256':[sha],'filename':'owned-stop.exe'}
 dependency='fixture.'+nonce+'.waiting';target='fixture.'+nonce+'.after-stop'
 running='--stop-running-install' in sys.argv
 for id,marker,args,deps in [(dependency,pending,['0','--sleep'] if running else ['0'],[]),(target,late,['0','--touch',win(late)],[dependency])]:
  value={'schema':'cider.recipe/v1','id':id,'kind':'app','revision':1,'name':{'en':id},'dependencies':deps,'sources':{'owned':source},'steps':[{'run_installer':{'source':'owned','kind':'exe','args':args}}],'detect':{'files':[win(marker)]}}
  (recipes/(id+'.json')).write_text(json.dumps(value))
 captures=TEST_HOME/'Logs/exit-captures'/RESULT['bottleID'];before=set(captures.glob('capture-*'))
 outpath=base/(nonce+'-stop.stdout');errpath=base/(nonce+'-stop.stderr')
 started_ns=time.time_ns()
 with outpath.open('w') as out,errpath.open('w') as err:
  handle=subprocess.Popen([str(CLI),'install',target,'-b',BOTTLE],stdout=out,stderr=err,text=True,env=ENV,start_new_session=True);CHILDREN.append(handle)
  def dependency_finished():
   for directory in set(captures.glob('capture-*'))-before:
    try:
     status=json.loads((directory/'result.json').read_text())
     if status['phase']=='captured' and status['windows_exit_code']==0:return directory
    except (OSError,ValueError):pass
   return None
  if running:
   def sleeping_probe_started():
    for log in (TEST_HOME/'Logs/sessions').glob('*/*/wine.log'):
     if log.stat().st_mtime_ns<started_ns:continue
     for line in log.read_text(errors='replace').splitlines():
      if line.startswith('{"kind":"owned-exit-probe"'):
       record=json.loads(line)
       if record['arguments']==['0','--sleep']:return log
    return None
   captured=wait_for(sleeping_probe_started,seconds=18)
  else:captured=wait_for(dependency_finished,seconds=18)
  require(handle.poll() is None and not pending.exists(),'fixture not in controlled install wait')
  time.sleep(.3);audit_count=len(audits());start=time.monotonic();stopped=cli(['kill','-b',BOTTLE],timeout=20)
  evidence={'killExitCode':stopped.returncode,'killSeconds':time.monotonic()-start,'workerAliveAfterStop':handle.poll() is None,'dependencyCapture':str(captured),'phase':'running Windows installer' if running else 'waiting for detect'}
  require(stopped.returncode==0,stopped.stdout+stopped.stderr)
  pending.write_text('owned late detection event after Stop returned\n')
  deadline=time.monotonic()+8
  while handle.poll() is None and not late.exists() and time.monotonic()<deadline:time.sleep(.05)
  evidence.update({'newWindowsSpawnsAfterStop':audits()[audit_count:],'laterRecipeMarkerExists':late.exists(),'installerCLIExitCode':handle.poll()})
  RESULT['stopInstallEvidence']=evidence
  require(not late.exists() and len(audits())==audit_count,'Stop returned but the old installer launched its next dependency step')
  handle.wait(timeout=3);require(handle.returncode!=0,'cancelled install reported success')
 return evidence
def stop_creation():
 import uuid,threading
 nonce=uuid.uuid4().hex[:12];name='Owned Creation Stop '+nonce;directory=BASE/'exit-status'
 readfd,writefd=os.pipe();filled=1024*1024;flood=None
 RESULT['creationStage']='reserving'
 output=directory/(nonce+'-create.stdout');error=directory/(nonce+'-create.stderr');chunks=[];reader=None;handle=None;new_id=None
 def drain():
  try:
   while True:
    part=os.read(readfd,8192)
    if not part:break
    chunks.append(part)
    while sum(map(len,chunks))>128*1024:chunks.pop(0)
  except OSError:pass
 try:
  with output.open('w') as out:
   handle=subprocess.Popen([str(CLI),'bottle','create',name,'--locale','zh-Hans','--template','win7_64','--engine',OWNER['engine_id']],stdout=out,stderr=writefd,text=True,env=ENV,start_new_session=True);CHILDREN.append(handle)
  def reserved():
   for file in (TEST_HOME/'AppSupport/Bottles').glob('*/cider-bottle.json'):
    try:
     config=json.loads(file.read_text())
     if config['name']==name:return (config,file.parent)
    except (OSError,ValueError):pass
   return None
  config,bottle_dir=wait_for(reserved,seconds=12);new_id=config['id'];RESULT['creationStage']='waiting wineboot'
  first=wait_for(lambda:next((r for r in audits() if r['bottle']==new_id and r['program']=='wineboot'),None),seconds=12)
  boot_pid=int(first['pid']);RESULT['creationStage']='waiting boot exit before blocked progress'
  def fill_after_boot_started():
   try:os.write(writefd,b'P'*filled)
   except OSError:pass
  flood=threading.Thread(target=fill_after_boot_started,daemon=True);flood.start()
  def between_stages():
   if any(row['pid']==boot_pid for row in snapshot()):return False
   ps=cli(['ps','--all'],timeout=6)
   return ps.returncode==0 and not any(line.split('\t')[1]==new_id for line in ps.stdout.splitlines() if '\t' in line)
  wait_for(between_stages,seconds=45);RESULT['creationStage']='blocked between stages'
  require(handle.poll() is None,'creation did not pause between initialization stages')
  before=len(audits());stopped=cli(['kill','-b',new_id],timeout=20)
  require(stopped.returncode==0,stopped.stdout+stopped.stderr)
  reader=threading.Thread(target=drain,daemon=True);reader.start()
  handle.wait(timeout=30)
  if flood:flood.join(timeout=3)
  os.close(writefd);writefd=-1
  reader.join(timeout=3);error.write_bytes(b''.join(chunks))
  late=[r for r in audits()[before:] if r['bottle']==new_id]
  evidence={'bottleID':new_id,'filledStderrBytes':filled,'killExitCode':stopped.returncode,'creatorCLIExitCode':handle.returncode,'newWindowsSpawnsAfterStop':late,'stdout':output.read_text(),'stderrTail':error.read_text(errors='replace')[-1200:]}
  RESULT['creationStopEvidence']=evidence
  require(not late and handle.returncode!=0,'stopped initialization resumed and launched later Windows setup stages')
  return evidence
 finally:
  if reader is None:reader=threading.Thread(target=drain,daemon=True);reader.start()
  if handle and handle.poll() is None:handle.terminate();handle.wait(timeout=4)
  if flood:flood.join(timeout=3)
  if writefd>=0:os.close(writefd)
  if new_id:
   file=TEST_HOME/'AppSupport/Bottles'/new_id/'cider-bottle.json'
   if file.exists():
    stopped=cli(['kill','-b',new_id],timeout=20);require(stopped.returncode==0,'owned new-prefix cleanup failed')
    archive=directory/'creation-prefix-archive';archive.mkdir(exist_ok=True)
    file.parent.rename(archive/new_id)
  reader.join(timeout=3);os.close(readfd)
def manual_installer():
 p=run([BASE/'manual-acceptance',BASE],timeout=75,env=ENV)
 require(p.returncode==0,p.stdout+p.stderr)
 result=json.loads((BASE/'exit-status/manual-result.json').read_text())
 require(result['passed'] and len(result['cases'])==5,'manual service evidence incomplete')
 return result
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
 if '--stop-creation' in sys.argv:
  case('product.stop_prevents_creation_resurrection',stop_creation)
 elif '--manual-installer' in sys.argv:
  case('kit.manual_installer_full_status_and_stop',manual_installer)
 elif '--stop-install' in sys.argv or '--stop-running-install' in sys.argv:
  case('product.stop_prevents_installer_resurrection',stop_installation)
 elif '--msi-diagnose' in sys.argv:
  case('diagnostic.owned_msi_hang_observation',diagnose_msi)
 elif '--installer-exit' in sys.argv:
  case('product.recipe_installer_full_exit_chain',installer_exit_batch)
 elif '--exit-capture' in sys.argv:
  case('product.full_windows_exit_capture',exit_status_batch)
 elif '--detach' in sys.argv:
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
