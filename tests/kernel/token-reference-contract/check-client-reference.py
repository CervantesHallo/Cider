#!/usr/bin/env python3
"""Check exact client ownership bodies with bounded RPC/object/SEH dependencies."""
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
args=parser.parse_args();text=args.source.read_text()
parts=[];hashes={}
context=re.search(r'^struct primary_token_reference_context\n\{.*?^\};',text,re.M|re.S)
header=re.search(r'^struct DECLSPEC_ALIGN\(MEMORY_ALLOCATION_ALIGNMENT\) object_header\n\{.*?^\};',text,re.M|re.S)
if not context or not header:raise RuntimeError('missing exact ownership structures')
parts.append(context.group())
for name in ['finish_primary_token_reference','unlock_obref','PsReferencePrimaryToken']:
    match=re.search(r'^[\w *]+\b'+name+r'\([^;]*?\n\{\n.*?^\}',text,re.M|re.S)
    if not match:raise RuntimeError('missing exact body '+name)
    body=match.group();parts.append(body);hashes[name]=hashlib.sha256(body.encode()).hexdigest()
with tempfile.TemporaryDirectory(prefix='cider-primary-client-model-') as tmp:
    tmp=Path(tmp);(tmp/'client-extracted.h').write_text('\n\n'.join(parts)+'\n');(tmp/'object-header.h').write_text(header.group()+'\n');binary=tmp/'checks'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I',str(tmp),str(Path(__file__).with_name('client-reference-model.c')),'-o',str(binary)],check=True)
    result=subprocess.run([str(binary)],text=True,capture_output=True,timeout=15,check=True)
report={'scope':'exact client C bodies; RPC/object dependencies and SEH control flow modeled, not native parity',
        'functionSHA256':hashes,'exitCode':result.returncode,'output':result.stdout.strip()}
args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
