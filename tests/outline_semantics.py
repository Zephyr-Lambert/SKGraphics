"""Outline transformations: optional test with user's GOG 01.06 initLoad.cat."""
import ctypes,struct,sys,random
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lib=ctypes.CDLL(str(root/'build/outline-test.so'));U=ctypes.c_uint32
f=lib.outline_transform_test;f.argtypes=[ctypes.POINTER(U),U,U,U,ctypes.POINTER(U)];f.restype=U
creation=lib.outline_creation_test;creation.argtypes=[ctypes.POINTER(U),U,U,U,U,U];creation.restype=U
b=Path(sys.argv[1]).read_bytes();out=(U*4096)()
def extract(st):
 p=st+4
 while True:
  t=struct.unpack_from('<I',b,p)[0];op=t&65535;n=(t>>16)&32767 if op==65534 else(t>>24)&15;p+=4*(n+1)
  if op==65535:return list(struct.unpack('<'+'I'*((p-st)//4),b[st:p]))
def instructions(w):
 p=1;result=[]
 while p<len(w):
  t=w[p];op=t&65535;n=(t>>16)&32767 if op==65534 else(t>>24)&15
  if op!=65534:result.append(w[p:p+n+1])
  p+=n+1
 return result
pixels=[0x3e700,0x3e7c0,0x3e8c0,0x3ea80,0x41ec0,0x41f80,0x42080,0x421c0]
vertices=[0x15500,0x157c0,0x15a80,0x16380,0x16c80,0x17000,0x17380,0x17d40,0x2b940,0x2bc40,0x2bf40,0x2c840]
random.seed(83);checks=0
for vertex,offsets in [(0,pixels),(1,vertices)]:
 for st in offsets:
  w=extract(st);a=(U*len(w))(*w)
  for thickness in [25,50,100,150,200,400]:
   n=f(a,len(w)*4,vertex,thickness,out);assert n
   transformed=list(out[:n]);seq=instructions(transformed);original=instructions(w)
   if not vertex:
    # Program sets all components negative then discards; contains no inputs,
    # texture reads, depth output or branches that could bypass TEXKILL.
    assert [x[0]&65535 for x in seq]==[81,1,65,1,65535]
    assert seq[0][2:]==[0xbf800000]*4
    assert seq[1]==[0x02000001,0x800f0000,0xa0e40000]
    assert seq[2]==[0x01000041,0x800f0000]
   else:
    const=seq.pop(0);assert const[:2]==[0x05000051,0xa00f00ff]
    multiplier=struct.unpack('<f',struct.pack('<I',const[2]))[0];assert multiplier==thickness/100
    injected=[i for i,x in enumerate(seq) if x==[0x03000005,0x8003001f,0x80e40000,0xa00000ff]]
    assert len(injected)==1;idx=injected[0];seq.pop(idx);target=seq[idx];assert target[2]==0x80e4001f;target[2]=0x80e40000
    # All original instructions preserved, including depth-bias calculation.
    assert seq==original
    for _ in range(50):
     normal=[random.uniform(-1,1) for _ in range(2)];width=random.uniform(0,.03);base=[random.uniform(-5,5) for _ in range(2)]
     for axis in range(2):
      before=normal[axis]*width+(base[axis] if target[0]&65535==4 else 0)
      after=(normal[axis]*multiplier)*width+(base[axis] if target[0]&65535==4 else 0)
      expected=(before-base[axis])*multiplier+base[axis] if target[0]&65535==4 else before*multiplier
      assert abs(after-expected)<1e-12;checks+=1
  for enabled,thickness in [(0,100),(1,100),(1,200)]:
   for mode in [0,1,2]:assert creation(a,len(w)*4,vertex,enabled,thickness,mode),(hex(st),enabled,thickness,mode)
  bad=w.copy();bad[-2]^=1;assert not f((U*len(bad))(*bad),len(bad)*4,vertex,200,out)
  assert not f(a,len(w)*4-4,vertex,200,out)
# Entire archive: reject all shaders outside the explicit outline set.
matched=[]
for st in range(0,len(b)-4,64):
 if b[st:st+4] not in (b'\x00\x03\xff\xff',b'\x00\x03\xfe\xff'):continue
 w=extract(st);v=int(w[0]==0xfffe0300)
 if f((U*len(w))(*w),len(w)*4,v,200,out):matched.append(st)
assert sorted(matched)==sorted(pixels+vertices)
print(f'Outline tests passed: 8 pixel programs, 12 vertex programs, {checks} extrusion comparisons, default/off/failure handling, and full-archive rejection of unrelated shaders. Not a GPU test.')
