"""Independent interpreter comparison against user-supplied original shaders.
Usage: python3 tests/filter_semantics.py /path/to/shader/initLoad.cat
No game bytecode is included with the mod.
"""
import ctypes, math, random, struct, sys
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lib=ctypes.CDLL(str(root/'build/filter-test.so'))
fn=lib.filter_transform_test
U=ctypes.c_uint32
fn.argtypes=[ctypes.POINTER(U),U,U,U,U,ctypes.POINTER(U)];fn.restype=U
blob=Path(sys.argv[1]).read_bytes()
def extract(start):
 p=start+4
 while True:
  t=struct.unpack_from('<I',blob,p)[0];op=t&65535
  p+=4*(1+((t>>16)&32767 if op==65534 else (t>>24)&15))
  if op==65535:break
 return list(struct.unpack('<'+'I'*((p-start)//4),blob[start:p]))
def instructions(w):
 p=1
 while p<len(w):
  t=w[p];op=t&65535;n=(t>>16)&32767 if op==65534 else(t>>24)&15
  if op==65535:return
  if op!=65534:yield op,t,w[p+1:p+n+1]
  p+=n+1
 raise AssertionError('missing END')
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def run(w,case,offset=(0,0)):
 regs={(1,0):[.3,.6,0,case['fog']],(1,1):[.8,.7,.6,case['alpha']],(1,2):case['position'],(2,160):[.35,.45,.55,.7],(2,177):[.1,.2,.3,.4],(2,176):[.4]*4}
 def kind(t):return ((t>>28)&7)|((t>>8)&24)
 def read(t):
  v=regs.get((kind(t),t&2047),[0.]*4);s=(t>>16)&255
  v=[v[(s>>(i*2))&3] for i in range(4)];mod=(t>>24)&15
  if mod==1:v=[-x for x in v]
  elif mod:raise AssertionError(('modifier',mod))
  return v
 for op,t,a in instructions(w):
  if op==31:continue
  if op==81:
   regs[(2,a[0]&2047)]=[struct.unpack('<f',struct.pack('<I',x))[0] for x in a[1:]];continue
  if op==65:
   if any(x<0 for x in regs.get((kind(a[0]),a[0]&2047),[0]*4)[:3]):return None
   continue
  v=[read(x) for x in a[1:]]
  if op==1:z=v[0]
  elif op==2:z=[x+y for x,y in zip(*v)]
  elif op==5:z=[x*y for x,y in zip(*v)]
  elif op==4:z=[x*y+k for x,y,k in zip(*v)]
  elif op==6:z=[1/v[0][0]]*4
  elif op==88:z=[y if x>=0 else k for x,y,k in zip(*v)]
  elif op==18:z=[x*y+(1-x)*k for x,y,k in zip(*v)]
  elif op==66:
   stage=a[-1]&2047
   if stage==0:z=case['texture']
   elif stage==5:
    x=min(1,max(0,v[0][0]+offset[0]));y=min(1,max(0,v[0][1]+offset[1]))
    mode=case['mode']
    depth= .25 if (x+y<case['edge']) else .75
    if mode==1:depth=.45+.2*x-.1*y
    if mode==2:depth=.4
    z=[depth,0,0,1]
   else:raise AssertionError(('sampler',stage))
  else:raise AssertionError(('opcode',op))
  d=a[0];key=(kind(d),d&2047);dest=regs.setdefault(key,[0.]*4)
  for i in range(4):
   if d&(1<<(16+i)):dest[i]=f32(min(1,max(0,z[i])) if d&0x100000 else z[i])
 return regs[(8,0)]
random.seed(214)
cases=[]
for i in range(160):
 cases.append(dict(position=[random.uniform(-1.05,1.05),random.uniform(-1.05,1.05),random.uniform(.25,.75),1],fog=random.random(),alpha=random.choice([.1,.8,1]),texture=[.9,.7,.5,.9],edge=random.uniform(.4,1.6),mode=i%3))
# Explicit edge and bounds cases ensure mixed taps, lit/dark and outside-map behavior.
for x in [-.81,-.8,-.79,-.001,0,.001,.79,.8,.81]:
 for y in [-.81,0,.81]:
  cases.append(dict(position=[x,y,.5,1],fog=.75,alpha=1,texture=[.9,.7,.5,.9],edge=.5*x-.5*y+1,mode=0))
checks=0;mixed=0
for start in [0x40840,0x40b40]:
 original=extract(start);src=(U*len(original))(*original);out=(U*4096)()
 for quality in [1,2]:
  for size in [2048,4096,8192]:
   for radius in [25,200,800]:
    count=fn(src,len(original)*4,quality,radius,size,out);assert count>0
    filtered=list(out[:count]);assert sum(1 for _ in instructions(filtered))<512
    r=quality;step=radius/(100*r*size)
    for case in cases:
     actual=run(filtered,case)
     samples=[run(original,case,(x*step,y*step)) for y in range(-r,r+1) for x in range(-r,r+1)]
     if samples[0] is None:assert actual is None
     else:
      expected=[sum(s[i] for s in samples)/len(samples) for i in range(4)]
      assert actual is not None
      error=max(abs(a-b) for a,b in zip(actual,expected))
      assert error<2e-6,(hex(start),quality,size,radius,case,actual,expected,error)
      mixed+=len({tuple(s) for s in samples})>1
     checks+=1
 bad=original.copy();bad[-3]^=1
 assert not fn((U*len(bad))(*bad),len(bad)*4,1,200,4096,out)
 assert not fn(src,len(original)*4-4,1,200,4096,out)
fallback=lib.filter_fallback_test
fallback.argtypes=[ctypes.POINTER(U),U,U];fallback.restype=U
for mode in range(4):assert fallback(src,len(original)*4,mode),mode
assert mixed>0
print(f'Passed {checks} shader comparisons, including {mixed} mixed-shadow cases; both shader variants, 3 map sizes, 3 radii, 2 qualities. Not a GPU test.')
