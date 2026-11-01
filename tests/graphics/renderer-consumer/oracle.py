import json,re

def qualify(text,arch,peer,nonce,exit_code):
 decoder=json.JSONDecoder();events=[]
 for line in text.splitlines():
  if not line.startswith('{"type":"renderer-consumer"'):continue
  rest=line
  while rest:
   value,end=decoder.raw_decode(rest);assert value['type']=='renderer-consumer';events.append(value);rest=rest[end:].lstrip()
 summaries={}
 for role in ['host','renderer']:
  data=[v for v in events if v['role']==role];sm=[v for v in data if v['event']=='summary'];assert len(sm)==1,sm;sm=sm[0];summaries[role]=sm
  assert sm['ok'] and sm['failures']==0 and all(v['ok'] for v in data),[v for v in data if not v['ok']]
  assert [v['check'] for v in data]==list(range(1,len(data)+1)) and sm['checks']==len(data)
  assert all(v['nonce']==nonce and v['pid']==sm['pid'] for v in data)
  assert sm['bits']==(64 if (arch if role=='host' else peer)=='x86_64' else 32)
  assert sm['phases']==sm['pipe_sends']==sm['pipe_receives']==3 and not any(sm[k] for k in ['io_error','clock_error','timeout','force_attempted','force_succeeded']) and sm['elapsed_ms']<180000
 h,p=summaries['host'],summaries['renderer'];assert h['host_pid']==h['pid']==p['host_pid'] and p['pid']==p['renderer_pid']==h['renderer_pid'] and h['pid']!=p['pid']
 assert h['self_birth']==p['peer_birth'] and p['self_birth']==h['peer_birth'] and h['self_birth'] and p['self_birth']
 assert h['root']==p['root'] and h['child_a']==p['child_a'] and h['child_b']==p['child_b'] and len({h['root'],h['child_a'],h['child_b']})==3
 assert h['peer_phase12_reports']==2 and all(h[k]==4 for k in ['peer_frames_reported','peer_readbacks_reported','peer_presents_reported']) and h['peer_local_release_reported'] and h['kernel_signaled'] and h['joined_zero'] and h['exit_code']==0
 assert p['swapchains']==2 and all(p[k]==4 for k in ['frames','readbacks','presents']) and p['local_com_released'] and p['gpu_drained']
 active=[(int(a,16),int(b)) for a,b in re.findall(r'renderer client active hwnd (0x[0-9a-fA-F]+) context (\d+)',text)]
 freed=[int(a,16) for a in re.findall(r'renderer client freed hwnd (0x[0-9a-fA-F]+) context0 routes0',text)]
 assert {h['child_a'],h['child_b']}=={a for a,b in active} and len({b for a,b in active})==2 and len(active)==2,active
 assert {h['child_a'],h['child_b']}==set(freed) and len(freed)==2,freed
 assert exit_code==0
 return dict(events=events,summaries=summaries,nativeActive=active,nativeLocalFreed=freed,physicalDisplayAccepted=False)
