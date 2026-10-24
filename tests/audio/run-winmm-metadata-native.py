#!/usr/bin/env python3
"""Compile/run exact WinMM metadata bodies with local deterministic doubles.

This is algorithm/ownership evidence only, not CoreAudio, PE ABI, InitOnce
concurrency, actual callback concurrency or worker-thread runtime acceptance.
Full component compilation and actual Wine checks are separate obligations.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--provider-source',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
source=a.source.resolve(strict=True);out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
source_bytes=source.read_bytes();text=source_bytes.decode('utf-8');functions=[]
pairs=[('static HRESULT WINMM_InitMMDevice(', 'static void WINMM_FreeMMDevices('),('static void WINMM_FreeMMDevices(', 'static HRESULT WINMM_GetDefaultDeviceID('),('static HRESULT WINMM_GetDefaultDeviceID(', 'static HRESULT WINMM_EnumDevices('),('static HRESULT WINMM_EnumDevices(', 'static HRESULT WINAPI notif_QueryInterface('),('static HRESULT update_mapping(', 'static HRESULT reroute_mapper_device('),('static HRESULT WINAPI notif_OnDefaultDeviceChanged(', 'static HRESULT WINAPI notif_OnPropertyValueChanged('),('static BOOL WINAPI WINMM_InitMMDevices(', 'static inline BOOL WINMM_IsMapper(')]
for start,end in pairs:
    assert text.count(start)==text.count(end)==1
    begin=text.index(start);finish=text.index(end,begin);body=text[begin:finish];functions.append(dict(name=start.split('(')[0].split()[-1],body=body,sha256=hashlib.sha256(body.encode()).hexdigest()))
for start,end in [('static UINT WINMM_QueryInstanceIDSize(', 'static UINT WINMM_QueryInstanceID('), ('static UINT WINMM_QueryInstanceID(', 'static UINT get_device_interface(')]:
    assert text.count(start)==text.count(end)==1
    begin=text.index(start);finish=text.index(end,begin);body=text[begin:finish];functions.append(dict(name=start.split('(')[0].split()[-1],body=body,sha256=hashlib.sha256(body.encode()).hexdigest()))
formats=list(dict.fromkeys(re.findall(r'\bWAVE_FORMAT_[1249][A-Z0-9]+\b',functions[0]['body'])))
inc='\n'.join('#define %s (1u << %u)'%(name,index)for index,name in enumerate(formats))+'\n'+''.join(f['body']for f in functions)
(out/'metadata-under-test.inc').write_text(inc)
(out/'source-snapshot.c').write_bytes(source_bytes)
live_harness=Path(__file__).with_name('winmm-metadata-harness.c')
harness_bytes=live_harness.read_bytes();harness=out/'harness.c';harness.write_bytes(harness_bytes)
runner_bytes=Path(__file__).read_bytes()
assert not (out/'result.json').exists()
result=dict(scope=__doc__,source=str(source),sourceSHA256=hashlib.sha256(source_bytes).hexdigest(),harnessSHA256=hashlib.sha256(harness_bytes).hexdigest(),runnerSHA256=hashlib.sha256(runner_bytes).hexdigest(),includeSHA256=hashlib.sha256((out/'metadata-under-test.inc').read_bytes()).hexdigest(),functions=[{k:v for k,v in f.items()if k!='body'}for f in functions],formatConstants=formats,runs=[])
provider_bytes=None
if a.provider_source:
    provider=a.provider_source.resolve(strict=True);provider_bytes=provider.read_bytes();provider_text=provider_bytes.decode('utf-8');provider_functions=[]
    for start,end in [('static HRESULT read_default_role_ids(', 'static void free_default_role_ids('), ('static void free_default_role_ids(', '/* A failed partial sample'), ('static HRESULT sample_default_role_ids(', 'static DWORD WINAPI notif_thread_proc(')]:
        assert provider_text.count(start)==provider_text.count(end)==1
        begin=provider_text.index(start);finish=provider_text.index(end,begin);body=provider_text[begin:finish];provider_functions.append(body)
    (out/'provider-under-test.inc').write_text(''.join(provider_functions))
    (out/'provider-snapshot.c').write_bytes(provider_bytes)
    result['providerSourceSHA256']=hashlib.sha256(provider_bytes).hexdigest();result['providerIncludeSHA256']=hashlib.sha256((out/'provider-under-test.inc').read_bytes()).hexdigest()
    result['providerFunctionsSHA256']=[hashlib.sha256(f.encode()).hexdigest()for f in provider_functions]
for arch in ['arm64','x86_64']:
    exe=out/('fixture-'+arch)
    cmd=['/usr/bin/clang','-arch',arch,'-isysroot','/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk','-mmacosx-version-min=14.0','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-misleading-indentation','-I',str(out),str(harness),'-o',str(exe)]
    if provider_bytes is not None:cmd.append('-DPROVIDER_HELPERS')
    c=subprocess.run(cmd,capture_output=True,text=True,timeout=40);row=dict(arch=arch,compileArgs=cmd,compileExit=c.returncode,compileDiagnostics=c.stderr);result['runs'].append(row)
    if c.returncode==0:
        r=subprocess.run(['/usr/bin/arch','-'+arch,str(exe)],capture_output=True,text=True,timeout=10);row.update(exeSHA256=hashlib.sha256(exe.read_bytes()).hexdigest(),runExit=r.returncode,stdout=r.stdout,stderr=r.stderr)
        if r.returncode==0:row['complete']=json.loads(r.stdout)
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
result['inputsUnchanged']=(source.read_bytes()==source_bytes and live_harness.read_bytes()==harness_bytes and Path(__file__).read_bytes()==runner_bytes and harness.read_bytes()==harness_bytes and hashlib.sha256((out/'metadata-under-test.inc').read_bytes()).hexdigest()==result['includeSHA256'])
if provider_bytes is not None:
    result['inputsUnchanged']=result['inputsUnchanged']and provider.read_bytes()==provider_bytes and hashlib.sha256((out/'provider-under-test.inc').read_bytes()).hexdigest()==result['providerIncludeSHA256']
result['passed']=result['inputsUnchanged']and all(r.get('runExit')==0 and r.get('complete',{}).get('ok')is True for r in result['runs'])
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));sys.exit(0 if result['passed']else 1)
