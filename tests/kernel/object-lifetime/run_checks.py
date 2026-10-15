#!/usr/bin/env python3
"""Exact object-manager function fixtures. Local host C only; no Wine/driver execution."""
import argparse, hashlib, json, pathlib, re, subprocess, tempfile
HERE=pathlib.Path(__file__).resolve().parent
FUNCTIONS=['free_kernel_object','ObDereferenceObject','reference_kernel_object_checked','ObReferenceObject',
           'unlock_handle_map','kernel_object_from_handle','close_reference_handle','ObReferenceObjectByHandle','dispatch_free']
def extract(s,name):
 m=re.search(r'^[\w *]+\b'+re.escape(name)+r'\([^;]*?\n\{\n.*?^\}',s,re.M|re.S)
 if not m:raise ValueError('Missing exact function '+name)
 return m.group()
def run(cmd,**kw):
 p=subprocess.run(cmd,capture_output=True,text=True,timeout=30,**kw)
 if p.returncode:raise RuntimeError(str(cmd)+':\n'+p.stdout+p.stderr)
 return p
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=pathlib.Path,required=True)
p.add_argument('--old-source',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path)
a=p.parse_args();path=a.source/'dlls/ntoskrnl.exe/ntoskrnl.c';s=path.read_text()
result={'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'scope':'exact C bodies with mocked handle/object backend, native pthreads; no Windows parity','checks':[]}
with tempfile.TemporaryDirectory(prefix='cider-lifetime-checks-') as tmp:
 tmp=pathlib.Path(tmp)
 header=re.search(r'^struct DECLSPEC_ALIGN\(MEMORY_ALLOCATION_ALIGNMENT\) object_header\n\{.*?^\};',s,re.M|re.S)
 if not header:raise ValueError('Missing aligned source header')
 (tmp/'header.h').write_text(header.group()+'\n')
 core='\n\n'.join(extract(s,n) for n in FUNCTIONS)
 (tmp/'core.h').write_text(core)
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-I',str(tmp),str(HERE/'check.c'),'-o',str(tmp/'checks')]
 run(cmd);x=run([str(tmp/'checks')]);result['checks'].append({'case':'ownership/negative/unwind/100 two-thread races','passed':True,'output':x.stdout.strip()})
 if a.old_source:
  old=(a.old_source/'dlls/ntoskrnl.exe/ntoskrnl.c').read_text()
  oldcore='\n\n'.join(extract(old,n) for n in FUNCTIONS if n not in ('close_reference_handle','unlock_handle_map','kernel_object_from_handle'))
  (tmp/'core.h').write_text(oldcore)
  # The old core does not call the new dependency mocks. Keep strict errors on
  # real semantic diagnostics while allowing those deliberate unused shims.
  oldcmd=cmd[:-1]+[str(tmp/'old-checks')]
  oldcmd.insert(-2,'-Wno-unused-function')
  oldcmd.insert(-2,'-DOLD_CORE')
  run(oldcmd)
  failure=subprocess.run([str(tmp/'old-checks'),'race'],capture_output=True,text=True,timeout=20)
  if failure.returncode==0 or 'heap-use-after-free' not in failure.stderr:raise RuntimeError('Old race no longer reproduces as UAF: '+failure.stderr)
  result['checks'].append({'case':'old core exact same valid close/lookup interleaving','old_reproduced_ASan_UAF':True,'returncode':failure.returncode})
  result['old_source_sha256']=hashlib.sha256(old.encode()).hexdigest()
 result['extracted_sha256']=hashlib.sha256(core.encode()).hexdigest()
if a.report:a.report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
