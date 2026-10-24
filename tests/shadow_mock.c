#include "../src/diagnostic.c"
static unsigned char exe[0x55000];
static P dev_vt[119],tex_vt[19],surf_vt[3],dev=dev_vt,tex[2],surf[2];
static U fail,allocations,releases,protects,flushes,allocation_failure,protection_failure,flush_failure,cap_limit=8192;
P API GetModuleHandleW(const W*p){(void)p;return exe;}
P API GetCurrentProcess(void){return (P)-1;}
I API FlushInstructionCache(P p,const void*a,U n){(void)p;(void)a;(void)n;return ++flushes!=flush_failure;}
I API VirtualProtect(P p,U n,U access,U*old){(void)p;(void)n;(void)access;*old=0x20;return ++protects!=protection_failure;}
U API GetSystemDirectoryW(W*p,U n){(void)p;(void)n;return 0;}
P API LoadLibraryW(const W*p){(void)p;return 0;}
P API GetProcAddress(P p,const char*n){(void)p;(void)n;return 0;}
U API GetModuleFileNameW(P p,W*q,U n){(void)p;(void)q;(void)n;return 0;}
P API CreateFileW(const W*p,U a,U b,P c,U d,U e,P f){(void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}
I API WriteFile(P p,const void*q,U n,U*w,P x){(void)p;(void)q;(void)x;*w=n;return 1;}
I API CloseHandle(P p){(void)p;return 1;}
U API GetPrivateProfileIntW(const W*a,const W*b,I c,const W*d){(void)a;(void)b;(void)d;return c;}
static I API caps0(P o,P out){(void)o;U*c=out;c[22]=c[23]=cap_limit;return 0;}
static U API release0(P o){(void)o;releases++;return 1;}
static I API surface0(P o,U level,P*out){if(level)fail=1;*out=o==&tex[0]?&surf[0]:&surf[1];return 0;}
static I API texture0(P o,U w,U h,U levels,U usage,U format,U pool,P*out,P shared){
 (void)o;U i=allocations++;
 if(i>=2||w!=shadow_size||h!=shadow_size||levels!=1||pool||shared||usage!=(i?1:2)||format!=(i?22:75))fail=2;
 if(allocations==allocation_failure)return (I)0x8007000e;
 tex[i]=tex_vt;surf[i]=surf_vt;*out=&tex[i];return 0;
}
static void fixture(void){
 for(U i=0;i<sizeof(exe);i++)exe[i]=0;
 exe[0]='M';exe[1]='Z';sh_put32(exe+60,0x100);sh_put32(exe+0x100,0x4550);
 exe[0x104]=0x4c;exe[0x105]=1;sh_put32(exe+0x108,0x62f1e445);
 exe[0x118]=0x0b;exe[0x119]=1;sh_put32(exe+0x118+56,0xe4e000);
 for(U i=0;i<sizeof(shadow_site);i++)exe[0x54845+i]=shadow_site[i];
 shadow_attempted=shadow_patched=0;shadow_image=0;shadow_enabled=1;shadow_size=4096;shadow_actual=2048;
 allocations=releases=protects=flushes=allocation_failure=protection_failure=flush_failure=0;cap_limit=8192;
}
static I unchanged(void){return sh_equal(exe+0x54845,shadow_site,sizeof(shadow_site));}
void _start(void){
 dev_vt[7]=caps0;dev_vt[23]=texture0;tex_vt[2]=release0;tex_vt[18]=surface0;surf_vt[2]=release0;record(&dev,119);
 fixture();shadow_setup(&dev);
 if(!shadow_patched||shadow_actual!=4096||allocations!=2||releases!=4||protects!=2||flushes!=1||sh_u32(exe+0x54846)!=4096||sh_u32(exe+0x5484b)!=4096)fail=3;
 for(U i=0;i<sizeof(shadow_site);i++)if((i==0||i==5||i>=10)&&exe[0x54845+i]!=shadow_site[i])fail=4;
 shadow_setup(&dev);if(allocations!=2||!shadow_binding(exe+0x1b985)||shadow_binding(exe+0x1b986))fail=5;
 fixture();exe[0x108]^=1;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=6;
 fixture();exe[0x54851]^=1;shadow_setup(&dev);if(shadow_patched||allocations||protects)fail=7;
 fixture();shadow_enabled=0;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=8;
 fixture();shadow_size=3000;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=9;
 fixture();cap_limit=2048;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=10;
 for(U i=1;i<=2;i++){fixture();allocation_failure=i;shadow_setup(&dev);if(shadow_patched||protects||releases!=(i-1)*2||!unchanged())fail=11;}
 fixture();protection_failure=1;shadow_setup(&dev);if(shadow_patched||!unchanged())fail=12;
 fixture();flush_failure=1;shadow_setup(&dev);if(shadow_patched||!unchanged()||protects!=2)fail=13;
 fixture();protection_failure=2;shadow_setup(&dev);if(shadow_patched||!unchanged()||protects!=3)fail=14;
 fixture();shadow_size=2048;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=15;
 fixture();shadow_size=8192;shadow_setup(&dev);if(!shadow_patched||sh_u32(exe+0x54846)!=8192)fail=16;
 fixture();shadow_size=12288;cap_limit=16384;shadow_setup(&dev);
 if(!shadow_patched||shadow_actual!=12288||allocations!=2||releases!=4||sh_u32(exe+0x54846)!=12288||sh_u32(exe+0x5484b)!=12288)fail=17;
 fixture();shadow_size=12288;shadow_setup(&dev);if(shadow_patched||allocations||!unchanged())fail=18;
 for(U i=1;i<=2;i++){fixture();shadow_size=12288;cap_limit=16384;allocation_failure=i;shadow_setup(&dev);if(shadow_patched||protects||releases!=(i-1)*2||!unchanged())fail=19;}
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}

U API GetFileAttributesW(const W*p){(void)p;return 0xffffffff;}
I API CreateDirectoryW(const W*p,P a){(void)p;(void)a;return 1;}
I API DeleteFileW(const W*p){(void)p;return 1;}
void API InitializeCriticalSection(P p){(void)p;}
void API EnterCriticalSection(P p){(void)p;}
void API LeaveCriticalSection(P p){(void)p;}

#include "profile_stubs.h"
