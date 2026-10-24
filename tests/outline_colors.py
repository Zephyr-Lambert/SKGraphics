"""Compare transformed outline output to the original with adjusted uniform."""
import ctypes,struct,sys,itertools
from pathlib import Path
U=ctypes.c_uint32;root=Path(__file__).resolve().parent.parent
lib=ctypes.CDLL(str(root/'build/outline-test.so'));f=lib.outline_color_test
f.argtypes=[ctypes.POINTER(U)]+[U]*7+[ctypes.POINTER(U)];f.restype=U
b=Path(sys.argv[1]).read_bytes()
def extract(st):
 p=st+4
 while True:
  t=struct.unpack_from('<I',b,p)[0];op=t&65535;n=(t>>16)&32767 if op==65534 else(t>>24)&15;p+=4*(n+1)
  if op==65535:return list(struct.unpack('<'+'I'*((p-st)//4),b[st:p]))
def run(w,color,fog,alpha):
 regs={(2,165):color,(2,177):[.6,.7,.8,1],(2,178):[0,0,fog,0],(1,0):[.2,.4,0,0]}
 def kind(t):return((t>>28)&7)|((t>>8)&24)
 def read(t):
  v=regs.get((kind(t),t&2047),[0]*4);s=(t>>16)&255;out=[v[(s>>(2*i))&3] for i in range(4)]
  mod=(t>>24)&15;assert mod in(0,1)
  return [-x for x in out] if mod else out
 p=1
 while p<len(w):
  t=w[p];op=t&65535;n=(t>>16)&32767 if op==65534 else(t>>24)&15;a=w[p+1:p+1+n];p+=n+1
  if op==65535:break
  if op in(65534,31):continue
  if op==81:regs[(2,a[0]&2047)]=[struct.unpack('<f',struct.pack('<I',x))[0] for x in a[1:]];continue
  if op==65:
   if any(x<0 for x in regs[(kind(a[0]),a[0]&2047)][:3]):return None
   continue
  v=[read(x) for x in a[1:]]
  if op==1:z=v[0]
  elif op==2:z=[x+y for x,y in zip(*v)]
  elif op==5:z=[x*y for x,y in zip(*v)]
  elif op==4:z=[x*y+k for x,y,k in zip(*v)]
  elif op==88:z=[y if x>=0 else k for x,y,k in zip(*v)]
  elif op==66:z=[.9,.7,.5,alpha]
  else:raise AssertionError(op)
  d=a[0];dest=regs.setdefault((kind(d),d&2047),[0]*4)
  for i in range(4):
   if d&(1<<(16+i)):dest[i]=z[i]
 return regs[(8,0)]
checks=0
for st in [0x3e700,0x3e7c0,0x3e8c0,0x3ea80,0x41ec0,0x41f80,0x42080,0x421c0]:
 original=extract(st);src=(U*len(original))(*original);out=(U*4096)()
 assert not f(src,len(original)*4,1,0,0,0,0,100,out)
 for enabled,custom,opacity,rgb in itertools.product([0,1],[0,1],[0,1,50,100],[(0,0,0),(255,0,0),(32,64,128)]):
  count=f(src,len(original)*4,enabled,custom,*rgb,opacity,out)
  if enabled and not custom and opacity==100:assert count==0;continue
  assert count;changed=list(out[:count])
  for gamealpha,fog,texalpha in itertools.product([0,.4,1],[0,.3,1],[0,1]):
   color=[.1,.2,.3,gamealpha];actual=run(changed,color,fog,texalpha)
   if not enabled or not opacity:assert actual is None
   else:
    expected=run(original,([x/255 for x in rgb] if custom else color[:3])+[gamealpha*opacity/100],fog,texalpha)
    if expected is None:assert actual is None
    else:assert actual is not None and max(abs(x-y) for x,y in zip(actual,expected))<1e-6
   checks+=1
print(f'Outline color/opacity passed {checks} output comparisons: original color, RGB override, fades, texture masking, fog, off and zero opacity. Not a GPU test.')
