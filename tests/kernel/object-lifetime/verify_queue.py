#!/usr/bin/env python3
"""Reconstruct the repaired queue in a temporary tree; input is read-only."""
import argparse, hashlib, json, pathlib, shutil, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference',type=pathlib.Path,required=True,help='Original quarantined source with 0009/0010 applied')
p.add_argument('--expected',type=pathlib.Path,required=True,help='Read-only final combined source to compare')
p.add_argument('--report',type=pathlib.Path)
a=p.parse_args();files=['dlls/ntoskrnl.exe/'+x for x in ('ntoskrnl.c','ntoskrnl_private.h','sync.c','pnp.c','ntoskrnl.exe.spec')]+['server/token.c']
result={'scope':'temporary reconstruction only; no Wine or driver execution','steps':[],'checks':[]}
with tempfile.TemporaryDirectory(prefix='cider-object-queue-') as tmp:
 tmp=pathlib.Path(tmp)
 for name in files:
  d=tmp/name;d.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(a.reference/name,d)
 for reverse,patch in [(True,'rejected/0010-ntoskrnl-primary-token-reference.patch.txt'),(True,'candidates/0009-token-kernel-object-mapping.patch'),(False,'candidates/0009-token-kernel-object-mapping.patch'),(False,'candidates/0012-reference-callers.patch'),(False,'candidates/0013-object-reference-lifetime.patch'),(False,'candidates/0015-close-borrowed-object-cleanup.patch'),(False,'candidates/0016-ioctl-allocation-buffer-ownership.patch'),(False,'candidates/0017-primary-token-release.patch')]:
  source=ROOT/'engine/patches'/patch
  command=['patch','-p1','--fuzz=0','--batch']+(['-R']if reverse else[])+['-i',str(source)]
  x=subprocess.run(command,cwd=tmp,capture_output=True,text=True,timeout=15)
  if x.returncode or 'offset' in x.stdout or 'fuzz' in x.stdout:raise RuntimeError(x.stdout+x.stderr)
  result['steps'].append({'patch':patch,'reverse':reverse,'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'fuzz':0,'offset':0})
 for name in files:
  b=(tmp/name).read_bytes()
  if b!=(a.expected/name).read_bytes():raise RuntimeError('Final source mismatch: '+name)
  result['checks'].append({'file':name,'sha256':hashlib.sha256(b).hexdigest(),'matches':True})
 if 'PsReferencePrimaryToken(' in (tmp/'dlls/ntoskrnl.exe/ntoskrnl.c').read_text():raise RuntimeError('Retired body reintroduced')
 result['retired_primary_body_absent']=True
if a.report:a.report.write_text(json.dumps(result,indent=2)+'\n')
print(f'PASS: {len(files)} files match; repaired queue applies with zero fuzz/offset; retired body absent')
