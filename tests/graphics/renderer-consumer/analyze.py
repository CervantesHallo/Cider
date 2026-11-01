from pathlib import Path
import struct,zlib,binascii,json,hashlib

MAX_FILE=20*1024**2
MAX_RUNS=100000

def need(value,why):
 if not value:raise ValueError(why)

def _analyze(path,phase,component_count=1,allow_opposite=False):
 need(phase in (1,2) and component_count in (1,2),'phase/component contract')
 with Path(path).open('rb') as file:data=file.read(MAX_FILE+1)
 need(len(data)<=MAX_FILE and data[:8]==b'\x89PNG\r\n\x1a\n','bounded PNG signature')
 pos=8;payload=[];header=None;done=False;idat_started=False;idat_ended=False;palette=False
 while pos<len(data):
  need(pos+12<=len(data),'chunk header bounds');n=struct.unpack_from('>I',data,pos)[0];kind=data[pos+4:pos+8];end=pos+12+n;need(end<=len(data),'chunk bounds')
  need(len(kind)==4 and all(65<=v<=90 or 97<=v<=122 for v in kind) and not kind[2]&32,'chunk type')
  body=data[pos+8:pos+8+n];crc=struct.unpack_from('>I',data,pos+8+n)[0];need(binascii.crc32(kind+body)&0xffffffff==crc,'chunk CRC')
  if header is None:need(kind==b'IHDR' and pos==8 and n==13,'IHDR first')
  if kind==b'IHDR':
   need(header is None and n==13,'one IHDR');header=struct.unpack('>IIBBBBB',body)
  elif kind==b'IDAT':
   need(not idat_ended,'contiguous IDAT');idat_started=True;payload.append(body)
  else:
   if idat_started:idat_ended=True
   if kind==b'PLTE':need(not palette and not idat_started and 0<n<=768 and n%3==0,'PLTE position/size');palette=True
   elif kind==b'IEND':need(n==0 and end==len(data) and idat_started,'terminal IEND');done=True;break
   else:
    need(kind[0]&32,'unknown critical PNG chunk')
    need(kind not in (b'tRNS',b'acTL',b'fcTL',b'fdAT'),'unsupported transparency/animated PNG')
  pos=end
 need(done and header and payload,'complete PNG')
 w,h,depth,color,compression,filtration,interlace=header
 need(100<=w<=2048 and 100<=h<=2048 and depth==8 and color in (2,6) and compression==filtration==interlace==0,'supported capture PNG subset/dimensions')
 bpp=3 if color==2 else 4;stride=w*bpp;expected=(stride+1)*h
 dec=zlib.decompressobj();raw=dec.decompress(b''.join(payload),expected+1)
 need(len(raw)==expected and dec.eof and not dec.unused_data and not dec.unconsumed_tail,'bounded complete inflate')
 def paeth(a,b,c):
  p=a+b-c;x,y,z=abs(p-a),abs(p-b),abs(p-c);return a if x<=y and x<=z else b if y<=z else c
 parent=[];boxes=[];areas=[]
 def root(i):
  while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
  return i
 def union(a,b):
  a,b=root(a),root(b)
  if a==b:return
  if a>b:a,b=b,a
  parent[b]=a;areas[a]+=areas[b];areas[b]=0
  x,y,z,t=boxes[a];u,v,q,s=boxes[b];boxes[a]=[min(x,u),min(y,v),max(z,q),max(t,s)]
 prev=bytearray(stride);counts=[0,0];bbox=[w,h,-1,-1];previous=[];white_rows=[];wanted=phase-1
 for y in range(h):
  start=y*(stride+1);f=raw[start];row=bytearray(raw[start+1:start+1+stride]);need(0<=f<=4,'PNG filter')
  for i in range(stride):
   a=row[i-bpp] if i>=bpp else 0;b=prev[i];c=prev[i-bpp] if i>=bpp else 0
   row[i]=(row[i]+(0 if f==0 else a if f==1 else b if f==2 else (a+b)//2 if f==3 else paeth(a,b,c)))&255
  segments=[];begin=None;white=bytearray((w+7)//8)
  for x in range(w):
   i=x*bpp;r,g,b=row[i:i+3];alpha=row[i+3] if bpp==4 else 255
   red=r>=240 and g<=16 and b<=16 and alpha>=240;blue=b>=240 and r<=16 and g<=16 and alpha>=240
   counts[0]+=red;counts[1]+=blue;match=red if wanted==0 else blue
   if r>=240 and g>=240 and b>=240 and alpha>=240:white[x//8]|=1<<(x%8)
   if match:
    if begin is None:begin=x
    bbox=[min(bbox[0],x),min(bbox[1],y),max(bbox[2],x),max(bbox[3],y)]
   elif begin is not None:segments.append((begin,x-1));begin=None
  if begin is not None:segments.append((begin,w-1))
  current=[];j=0
  for left,right in segments:
   need(len(parent)<MAX_RUNS,'component label cap')
   label=len(parent);parent.append(label);boxes.append([left,y,right,y]);areas.append(right-left+1)
   # Full eight-neighbor connectivity, including every one-pixel run. The
   # sorted interval sweep does not discard narrow bridges or stems.
   while j<len(previous) and previous[j][1]<left-1:j+=1
   k=j
   while k<len(previous) and previous[k][0]<=right+1:union(label,previous[k][2]);k+=1
   current.append((left,right,label))
  previous=current;white_rows.append(int.from_bytes(white,'little'));prev=row
 components=[dict(bounds=boxes[i],pixels=areas[i]) for i in range(len(parent)) if root(i)==i]
 large=sorted([c for c in components if c['pixels']>10000 and c['bounds'][3]-c['bounds'][1]+1>60],key=lambda c:c['bounds'][0])
 rectangles=all(c['pixels']==(c['bounds'][2]-c['bounds'][0]+1)*(c['bounds'][3]-c['bounds'][1]+1) for c in large)
 gap_width=0;geometry=True
 if component_count==2 and len(large)==2:
  a,b=(c['bounds'] for c in large);geometry=a[2]+20<b[0] and abs(a[1]-b[1])<=2 and abs(a[3]-b[3])<=2
  if geometry:
   left,right=a[2]+1,b[0]-1;common=(1<<(right-left+1))-1
   for y in range(max(a[1],b[1]),min(a[3],b[3])+1):common&=white_rows[y]>>left
   run=0
   for x in range(right-left+1):
    run=run+1 if common>>x&1 else 0;gap_width=max(gap_width,run)
 accepted=counts[wanted]>10000*component_count and (allow_opposite or counts[1-wanted]<1000) and len(large)==component_count and rectangles and geometry and (component_count==1 or gap_width>=20)
 return dict(passed=accepted,phase=phase,width=w,height=h,pngSHA256=hashlib.sha256(data).hexdigest(),redPixels=counts[0],bluePixels=counts[1],dominantBounds=bbox,regions=large,expectedComponents=component_count,fullRectangleOccupancy=rectangles,opaqueWhiteGapWidth=gap_width,connectivity='all matching runs, eight-neighbor',scope='actual captured WindowServer PNG rectangles; no physical scanout/color-management/exact source texture identity claim'),white_rows

def analyze(path,phase,component_count=1):
 return _analyze(path,phase,component_count)[0]

def analyze_mixed(path,phase):
 need(phase in (1,2),'mixed phase')
 red,white=_analyze(path,1,1,True);blue,white_b=_analyze(path,2,1,True)
 need(red['pngSHA256']==blue['pngSHA256'] and white==white_b,'same immutable captured PNG')
 accepted=red['passed'] and blue['passed'];ordered=[];gap_width=0
 if accepted:
  a,b=(red['regions'][0],blue['regions'][0]) if phase==1 else (blue['regions'][0],red['regions'][0])
  x,y=a['bounds'],b['bounds']
  accepted=x[2]+20<y[0] and x[1]==y[1] and x[3]==y[3]
  if accepted:
   left,right=x[2]+1,y[0]-1;common=(1<<(right-left+1))-1
   for row in range(max(x[1],y[1]),min(x[3],y[3])+1):common&=white[row]>>left
   run=0
   for column in range(right-left+1):
    run=run+1 if common>>column&1 else 0;gap_width=max(gap_width,run)
   accepted=gap_width>=20
  ordered=[dict(color='red' if phase==1 else 'blue',**a),dict(color='blue' if phase==1 else 'red',**b)]
 return dict(passed=accepted,phase=phase,width=red['width'],height=red['height'],pngSHA256=red['pngSHA256'],redPixels=red['redPixels'],bluePixels=red['bluePixels'],orderedRegions=ordered,opaqueWhiteGapWidth=gap_width,scope='actual consumer window red/blue left-right correspondence, no physical scanout or performance claim')

if __name__=='__main__':
 import sys
 d=analyze_mixed(sys.argv[1],int(sys.argv[2]));print(json.dumps(d));raise SystemExit(0 if d['passed'] else 1)
