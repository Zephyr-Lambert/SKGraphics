/* Forwarding/rehook smoke test with mock Win32 and D3D9; not an in-game test. */
#include "../src/diagnostic.c"
static P dv[119],av[17],sv[13];
static P dobj=dv,aobj=av,sobj=sv;
static U writes,calls,fail,file_creates,protect_calls,protect_fail_at,library_loads;
static U (*profile_int)(const W*,const W*,I,const W*);
static U tw=1280,th=720,tu=1;
static I equal(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
P API GetModuleHandleW(const W*p){(void)p;return 0;}
P API GetCurrentProcess(void){return (P)-1;}
I API FlushInstructionCache(P p,const void*a,U n){(void)p;(void)a;(void)n;return 1;}
U API GetSystemDirectoryW(W*p,U c){(void)c;p[0]='C';p[1]=0;return 1;}
P API LoadLibraryW(const W*p){
 library_loads++;
#ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
 const char*expected="C\\ReShade\\d3d9.dll";U i=0;
 while(expected[i]&&p[i]==(W)expected[i])i++;
 if(expected[i]||p[i])fail=32;
#else
 (void)p;
#endif
 return (P)2;
}
U API GetModuleFileNameW(P m,W*p,U c){(void)m;(void)c;p[0]='C';p[1]='\\';p[2]='x';p[3]=0;return 3;}
P API CreateFileW(const W*p,U a,U b,P c,U d,U e,P f){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;file_creates++;return (P)3;}
I API WriteFile(P a,const void*b,U c,U*d,P e){(void)a;(void)b;(void)e;*d=c;writes++;return 1;}
I API VirtualProtect(P a,U b,U c,U*d){(void)a;(void)b;(void)c;*d=4;return ++protect_calls!=protect_fail_at;}
I API CloseHandle(P a){(void)a;return 1;}
U API GetPrivateProfileIntW(const W*a,const W*b,I c,const W*d){
 if(profile_int)return profile_int(a,b,c,d);
#ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
 if(a[0]=='C'&&b[0]=='L')return 1;
#endif
 (void)a;(void)b;(void)d;return c;
}
static U explicit_legacy(const W*a,const W*b,I c,const W*d){
 (void)d;if(a[0]=='R'&&b[0]=='E')return 1;
 if(a[0]=='D'&&b[0]=='L')return 1;
 #ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
 if(a[0]=='C'&&b[0]=='L')return 1;
 #endif
 return c;
}
static P API make(U sdk){if(sdk!=32)fail=1;return &aobj;}
P API GetProcAddress(P m,const char*n){(void)m;return equal(n,"Direct3DCreate9")?(P)make:0;}
static I API mkdev(P o,U a,U t,P w,U b,PP*p,P*out){if(o!=&aobj||a!=0||t!=1||w!=(P)123||b!=64||p->w!=3840||p->h!=2160)fail=2;*out=&dobj;calls++;return 0;}
static I API getback(P o,U a,U b,U c,P*out){(void)o;(void)a;(void)b;(void)c;*out=&sobj;return 0;}
static I API getdesc(P o,Desc*d){(void)o;d->w=3840;d->h=2160;d->format=21;d->ms=0;return 0;}
static U API release(P o){(void)o;return 1;}
static I API reset0(P o,PP*p){if(o!=&dobj||p->w!=3840)fail=3;calls++;return 0;}
static I API present0(P o,P a,P b,P w,P d){if(o!=&dobj||a||b||w||d)fail=4;calls++;return 0;}
static I API rt0(P o,U slot,P target){if(o!=&dobj||slot||target!=&sobj)fail=5;calls++;return 0;}
static I API view0(P o,const VP*v){if(o!=&dobj||v->w!=3840||v->h!=2160)fail=6;calls++;return 0;}
static I API tex0(P o,U w,U h,U l,U u,U f,U pool,P*out,P shared){if(o!=&dobj||w!=tw||h!=th||l!=1||u!=tu||f!=21||pool||shared)fail=7;*out=(P)77;calls++;return 0;}
static I API getview0(P o,VP*v){(void)o;v->x=0;v->y=0;v->w=3840;v->h=2160;return 0;}
static I API draw0(P o,U type,U start,U count){if(o!=&dobj||type!=4||start!=0||count!=3)fail=17;return 0;}
static P outer_next;
static U outer_calls;
static I API outer_create(P o,U a,U t,P w,U b,PP*p,P*out){
 outer_calls++;
 return ((I(API*)(P,U,U,P,U,PP*,P*))outer_next)(o,a,t,w,b,p,out);
}
void _start(void){
 av[16]=mkdev;dv[16]=reset0;dv[17]=present0;dv[18]=getback;dv[23]=tex0;dv[37]=rt0;dv[47]=view0;dv[48]=getview0;dv[81]=draw0;sv[12]=getdesc;sv[2]=release;
 profile_int=explicit_legacy;
 DllMain((P)1,1,0);
 for(U i=0;i<2;i++){
  P a=Direct3DCreate9(32),d=0;
  /* ReShade wraps the shared factory vtable after our first export returns.
   * The second export must leave this wrapper in place. */
  if(i==0){outer_next=av[16];av[16]=outer_create;}
  if(av[16]!=(P)outer_create)fail=30;
  aa_method=0;PP p={0};p.w=2560;p.h=1440;p.windowed=1;
  ((I(API*)(P,U,U,P,U,PP*,P*))(*(P**)a)[16])(a,0,1,(P)123,64,&p,&d);
  if(p.w!=2560||p.h!=1440)fail=10;
  ((I(API*)(P,PP*))(*(P**)d)[16])(d,&p);
  if(p.w!=2560||p.h!=1440)fail=11;
  ((I(API*)(P,P,P,P,P))(*(P**)d)[17])(d,0,0,0,0);
  ((I(API*)(P,U,P))(*(P**)d)[37])(d,0,&sobj);
  VP v={0};v.w=2560;v.h=1440;
  ((I(API*)(P,const VP*))(*(P**)d)[47])(d,&v);
  P tex=0;((I(API*)(P,U,U,U,U,U,U,P*,P))(*(P**)d)[23])(d,1280,720,1,1,21,0,&tex,0);
  if(tex!=(P)77)fail=8;
 }
 if(calls!=12||writes<10||table_count!=2)fail=9;
 VP result={0};getviewport(&dobj,&result);if(result.w!=2560||result.h!=1440)fail=18;
 draw(&dobj,4,0,3);
 P tex=0;
 tw=3840;th=2160;tu=1;texture(&dobj,2560,1440,1,1,21,0,&tex,0);
 tw=2048;th=2048;texture(&dobj,2048,2048,1,1,21,0,&tex,0);
 tw=2560;th=1440;tu=0;texture(&dobj,2560,1440,1,0,21,0,&tex,0);
 State t={0};PP p={0};p.w=1920;p.h=1080;p.windowed=1;select_size(&t,&p);
 if(!t.active||t.rw!=3840||t.lw!=1920)fail=12;
 p.w=1920;p.h=1200;select_size(&t,&p);if(t.active)fail=13;
 p.h=1080;p.windowed=0;select_size(&t,&p);if(t.active)fail=14;
 p.windowed=1;enabled=0;select_size(&t,&p);if(t.active)fail=15;
 enabled=1;select_size(&t,&p);Rect a={0,0,1920,1080},b;
 rectscale(&t,&a,&b);if(b.right!=3840||b.bottom!=2160)fail=16;

 if(outer_calls!=2)fail=31;
 DllMain((P)1,0,0);
 #if defined(__x86_64__)
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");
#else
 __asm__ volatile("int $0x80"::"a"(1),"b"(fail):"memory");
#endif
 __builtin_unreachable();
}

U API GetFileAttributesW(const W*p){(void)p;return 0xffffffff;}
I API CreateDirectoryW(const W*p,P a){(void)p;(void)a;return 1;}
I API DeleteFileW(const W*p){(void)p;return 1;}
void API InitializeCriticalSection(P p){(void)p;}
void API EnterCriticalSection(P p){(void)p;}
void API LeaveCriticalSection(P p){(void)p;}

#include "profile_stubs.h"
