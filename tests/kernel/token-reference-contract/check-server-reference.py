#!/usr/bin/env python3
"""Extract new server ownership functions and check them with bounded dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--report',type=Path,required=True)
args=parser.parse_args()
parts=[];hashes={}
items=[('server/device.c','get_owned_device_manager',False),('server/process.c','get_process_primary_token_reference',False),
       ('server/device.c','grab_kernel_object',False),('server/device.c','reference_process_primary_token',True),
       ('server/device.c','release_kernel_object',True)]
for file,name,handler in items:
    text=(args.source/file).read_text()
    pattern=r'^DECL_HANDLER\('+name+r'\)\n\{\n.*?^\}' if handler else r'^[\w *]+\b'+name+r'\([^;]*?\n\{\n.*?^\}'
    match=re.search(pattern,text,re.M|re.S)
    if not match:raise RuntimeError('missing exact body '+name)
    body=match.group();parts.append(body);hashes[name]=hashlib.sha256(body.encode()).hexdigest()
with tempfile.TemporaryDirectory(prefix='cider-primary-server-model-') as temp:
    temp=Path(temp);(temp/'server-extracted.h').write_text('\n\n'.join(parts)+'\n');binary=temp/'checks'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I',str(temp),str(Path(__file__).with_name('server-reference-model.c')),'-o',str(binary)],check=True)
    result=subprocess.run([str(binary)],text=True,capture_output=True,timeout=15,check=True)
report={'scope':'exact server C bodies, modeled handle/map storage; no live token or native Windows evidence',
        'functionSHA256':hashes,'exitCode':result.returncode,'output':result.stdout.strip()}
args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
