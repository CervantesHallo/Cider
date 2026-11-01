import re

def qualify_geometry_wakes(text):
 stacks={};pairs=[];bounds=[]
 for line_no,line in enumerate(text.splitlines(),1):
  start=re.search(r'^([0-9a-f]+):trace:macdrv:macdrv_remote_client_event renderer geometry wake begin thread (\d+)$',line)
  end=re.search(r'^([0-9a-f]+):trace:macdrv:macdrv_remote_client_event renderer geometry wake end thread (\d+)$',line)
  if start:
   thread=int(start[1],16);assert thread==int(start[2]);stacks.setdefault(thread,[]).append(dict(thread=thread,startLine=line_no,bounds=[]));continue
  if end:
   thread=int(end[1],16);assert thread==int(end[2]) and stacks.get(thread);item=stacks[thread].pop();item['endLine']=line_no;pairs.append(item);continue
  match=re.search(r'^([0-9a-f]+):trace:macdrv:update_client_bounds renderer client bounds confirmed hwnd (0x[0-9a-f]+) context (\d+) rect (-?\d+),(-?\d+),(-?\d+),(-?\d+)$',line)
  if match:
   item=dict(thread=int(match[1],16),line=line_no,hwnd=int(match[2],16),context=int(match[3]),rect=list(map(int,match.groups()[3:])));bounds.append(item)
   if stacks.get(item['thread']):stacks[item['thread']][-1]['bounds'].append(item)
 assert pairs,'no completed actual geometry-message callback observed'
 return dict(passed=True,completedCallbacks=len(pairs),boundsInsideCompletedCallbacks=sum(len(v['bounds']) for v in pairs),callbacks=pairs,observedBounds=bounds,unmatchedProcessExitTails=[v for s in stacks.values() for v in s],scope='Actual existing PE message route dispatched geometry wake; not an all-callback-tail or latency/performance claim')
