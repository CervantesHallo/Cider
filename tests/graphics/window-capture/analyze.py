from pathlib import Path
import struct,zlib,binascii,json,hashlib

def analyze(path,phase):
 p=Path(path);data=p.read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and len(data)<20*1024**2
 pos=8;payload=[];header=None;done=False
 while pos<len(data):
  assert pos+12<=len(data);n=struct.unpack_from('>I',data,pos)[0];kind=data[pos+4:pos+8];end=pos+12+n;assert end<=len(data)
  body=data[pos+8:pos+8+n];crc=struct.unpack_from('>I',data,pos+8+n)[0];assert binascii.crc32(kind+body)&0xffffffff==crc
  if kind==b'IHDR':assert header is None and len(body)==13;header=struct.unpack('>IIBBBBB',body)
  if kind==b'IDAT':payload.append(body)
  if kind==b'IEND':assert n==0 and end==len(data);done=True;break
  pos=end
 assert done and header and payload
 w,h,depth,color,compression,filtration,interlace=header;assert 100<=w<=8192 and 100<=h<=8192 and depth==8 and color in [2,6] and compression==filtration==interlace==0
 bpp=3 if color==2 else 4;stride=w*bpp;expected=(stride+1)*h;assert expected<=256*1024**2
 dec=zlib.decompressobj();raw=dec.decompress(b''.join(payload),expected+1);assert len(raw)==expected and dec.eof and not dec.unused_data and not dec.unconsumed_tail
 def paeth(a,b,c):
  p=a+b-c;x,y,z=abs(p-a),abs(p-b),abs(p-c);return a if x<=y and x<=z else b if y<=z else c
 prev=bytearray(stride);counts=[0,0];bbox=[w,h,-1,-1];tile_rows=0;longest_rows=0;wanted=phase-1
 for y in range(h):
  start=y*(stride+1);f=raw[start];row=bytearray(raw[start+1:start+1+stride]);assert 0<=f<=4
  for i in range(stride):
   a=row[i-bpp] if i>=bpp else 0;b=prev[i];c=prev[i-bpp] if i>=bpp else 0
   row[i]=(row[i]+(0 if f==0 else a if f==1 else b if f==2 else (a+b)//2 if f==3 else paeth(a,b,c)))&255
  longest=run=0
  for x in range(w):
   i=x*bpp;r,g,b=row[i:i+3];a=row[i+3] if bpp==4 else 255
   red=r>=240 and g<=16 and b<=16 and a>=240;blue=b>=240 and r<=16 and g<=16 and a>=240
   counts[0]+=red;counts[1]+=blue
   match=red if wanted==0 else blue
   run=run+1 if match else 0;longest=max(longest,run)
   if match:bbox=[min(bbox[0],x),min(bbox[1],y),max(bbox[2],x),max(bbox[3],y)]
  tile_rows=tile_rows+1 if longest>=100 else 0;longest_rows=max(longest_rows,tile_rows);prev=row
 accepted=counts[wanted]>=10000 and counts[1-wanted]<1000 and longest_rows>=60
 return dict(passed=accepted,phase=phase,width=w,height=h,pngSHA256=hashlib.sha256(data).hexdigest(),redPixels=counts[0],bluePixels=counts[1],dominantBounds=bbox,consecutiveRowsWith100PixelRun=longest_rows,scope='actual captured test-window PNG dominant color rectangle; no exact color-management or texture/readback identity claim')
if __name__=='__main__':
 import sys
 d=analyze(sys.argv[1],int(sys.argv[2]));print(json.dumps(d));raise SystemExit(0 if d['passed'] else 1)
