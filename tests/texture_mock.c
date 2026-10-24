/* Native COM/WinAPI test of texture IAT installation and loading orchestration.
 * No real image decoder, DDS exporter, disk I/O or GPU is exercised here. */
#include "../src/diagnostic.c"
static U fail,original_calls,file_calls,save_calls,original_releases,replacement_releases,buffer_releases,lock_depth;
static U dump_exists,dds_exists,png_exists,partial_write,delete_calls,write_calls,limit_error,load_error,save_error,original_error,desc_error,desc_format=21;
static U protections,protection_fail,seen_width,seen_height,seen_format,seen_levels;
static unsigned char fake_exe[0x352000],dds_bytes[160];
static P orig_vt[18],repl_vt[18],buffer_vt[5],original_obj=orig_vt,replacement_obj=repl_vt,buffer_obj=buffer_vt;
static I equal0(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static I has(const W*p,const char*needle){for(U i=0;p[i];i++){U j=0;while(needle[j]&&p[i+j]==(W)needle[j])j++;if(!needle[j])return 1;}return 0;}
static I API memory0(P dev,const void*src,U bytes,U w,U h,U levels,U usage,U fmt,U pool,U filter,U mip,U key,P info,P palette,P*out){
 (void)dev;(void)src;(void)w;(void)h;(void)levels;(void)usage;(void)fmt;(void)pool;(void)filter;(void)mip;(void)key;
 original_calls++;if(bytes!=3)fail=1;if(info)*(U*)info=123;if(palette)*(U*)palette=456;
 if(original_error){*out=0;return (I)0x80004005;}*out=&original_obj;return 0;
}
static I API file0(P dev,const W*name,U w,U h,U levels,U usage,U fmt,U pool,U filter,U mip,U key,P info,P palette,P*out){
 (void)dev;(void)filter;(void)mip;(void)key;
 file_calls++;seen_width=w;seen_height=h;seen_format=fmt;seen_levels=levels;
 if(!has(name,"SKGraphics_Textures\\replace\\")||usage||pool!=1||info||palette)fail=2;
 if(dds_exists&&!has(name,".dds"))fail=3;if(!dds_exists&&png_exists&&!has(name,".png"))fail=4;
 *out=&replacement_obj;return load_error?(I)0x80004005:0;
}
static I API save0(P*out,U fmt,P tex,P palette){(void)palette;save_calls++;if(fmt!=4||tex!=&original_obj)fail=5;*out=&buffer_obj;return save_error?(I)0x80004005:0;}
static U API release0(P p){if(p==&original_obj)original_releases++;else if(p==&replacement_obj)replacement_releases++;else if(p==&buffer_obj)buffer_releases++;else fail=6;return 1;}
static P API buffer_pointer(P p){(void)p;return dds_bytes;}
static U API buffer_size(P p){(void)p;return limit_error?0x40000000:sizeof(dds_bytes);}
static I API describe0(P p,U level,Desc*d){if(p!=&original_obj||level)fail=7;d->w=128;d->h=64;d->format=desc_format;return desc_error?-1:0;}
P API GetModuleHandleW(const W*p){(void)p;return fake_exe;}
P API GetCurrentProcess(void){return (P)-1;}
I API FlushInstructionCache(P p,const void*a,U n){(void)p;(void)a;(void)n;return 1;}
I API VirtualProtect(P p,U n,U access,U*old){(void)p;(void)n;(void)access;*old=2;return ++protections!=protection_fail;}
U API GetSystemDirectoryW(W*p,U n){(void)p;(void)n;return 0;}
P API LoadLibraryW(const W*p){(void)p;return (P)1;}
P API GetProcAddress(P p,const char*n){(void)p;if(equal0(n,"D3DXCreateTextureFromFileInMemoryEx"))return memory0;if(equal0(n,"D3DXCreateTextureFromFileExW"))return file0;if(equal0(n,"D3DXSaveTextureToFileInMemory"))return save0;return 0;}
U API GetModuleFileNameW(P p,W*q,U n){(void)p;(void)n;const char*s="C:\\game\\d3d9.dll";U i=0;while(s[i]){q[i]=(W)s[i];i++;}q[i]=0;return i;}
P API CreateFileW(const W*p,U a,U b,P c,U d,U e,P f){(void)b;(void)c;(void)e;(void)f;if(!has(p,"\\dump\\")||a!=0x40000000||d!=1)fail=8;if(dump_exists)return (P)-1;dump_exists=1;return (P)5;}
I API WriteFile(P p,const void*q,U n,U*w,P x){(void)x;if(p!=(P)5||q!=dds_bytes)fail=9;write_calls++;*w=partial_write?n-1:n;return 1;}
I API CloseHandle(P p){(void)p;return 1;}
U API GetPrivateProfileIntW(const W*a,const W*b,I c,const W*d){(void)a;(void)b;(void)d;return c;}
U API GetFileAttributesW(const W*p){if(has(p,"\\dump\\"))return dump_exists?0:0xffffffff;if(has(p,"\\replace\\"))return ((has(p,".dds")&&dds_exists)||(has(p,".png")&&png_exists))?0:0xffffffff;return 16;}
I API CreateDirectoryW(const W*p,P a){(void)p;(void)a;return 1;}
I API DeleteFileW(const W*p){if(!has(p,"\\dump\\"))fail=10;delete_calls++;dump_exists=0;return 1;}
void API InitializeCriticalSection(P p){(void)p;}
void API EnterCriticalSection(P p){(void)p;lock_depth++;}
void API LeaveCriticalSection(P p){(void)p;if(!lock_depth)fail=11;else lock_depth--;}
static void fixture(void){
 original_calls=file_calls=save_calls=original_releases=replacement_releases=buffer_releases=0;
 dump_exists=dds_exists=png_exists=partial_write=delete_calls=write_calls=0;limit_error=load_error=save_error=original_error=desc_error=0;desc_format=21;
 texture_ready=1;texture_original=memory0;texture_file=file0;texture_save=save0;
 texture_dump_enabled=texture_replace_enabled=1;texture_own_size=0;texture_dump_limit=500;texture_dump_mb=512;
 texture_loads=texture_dumps=texture_bytes=texture_replacements=texture_failures=texture_reported_limit=0;
 texture_root_len=0;protections=protection_fail=0;
}
static P run(U usage,U fmt){P out=0;U info=0,palette=0;I hr=texture_from_memory((P)4,"abc",3,0xffffffff,0xffffffff,4,usage,fmt,1,3,1,0,&info,&palette,&out);if(info!=123||palette!=456||lock_depth||((hr<0)!=!!original_error))fail=12;return out;}
#ifndef TEXTURE_TEST_ENTRY
#define TEXTURE_TEST_ENTRY _start
#endif
void TEXTURE_TEST_ENTRY(void){
 orig_vt[2]=repl_vt[2]=buffer_vt[2]=release0;orig_vt[17]=describe0;buffer_vt[3]=buffer_pointer;buffer_vt[4]=buffer_size;
 fixture();if(run(0,0)!=&original_obj||texture_dumps!=1||write_calls!=1||buffer_releases!=1||original_releases||file_calls)fail=13;
 run(0,0);if(save_calls!=1||texture_dumps!=1)fail=14;
 fixture();dds_exists=png_exists=1;if(run(0,0)!=&replacement_obj||original_releases!=1||texture_replacements!=1||save_calls!=1||seen_width!=128||seen_height!=64||seen_format!=21||seen_levels!=4)fail=15;
 fixture();png_exists=1;texture_own_size=1;run(0,0);if(file_calls!=1||seen_width!=0xfffffffe||seen_height!=0xfffffffe)fail=16;
 fixture();dds_exists=1;load_error=1;if(run(0,0)!=&original_obj||original_releases||replacement_releases!=1||texture_failures!=1)fail=17;
 fixture();texture_dump_enabled=0;dds_exists=1;run(0,0);if(save_calls||file_calls!=1)fail=18;
 fixture();texture_replace_enabled=0;dds_exists=1;run(0,0);if(file_calls||save_calls!=1)fail=19;
 fixture();run(1,0);run(0,40);desc_format=41;run(0,0);if(texture_loads||save_calls||file_calls)fail=20;
 fixture();original_error=1;run(0,0);if(save_calls||file_calls)fail=21;
 fixture();partial_write=1;run(0,0);if(delete_calls!=1||dump_exists||texture_dumps)fail=22;
 fixture();save_error=1;run(0,0);if(write_calls||buffer_releases!=1)fail=23;
 fixture();texture_dump_limit=0;run(0,0);if(save_calls||texture_dumps)fail=24;
 fixture();limit_error=1;run(0,0);if(write_calls||buffer_releases!=1)fail=25;
 fixture();texture_dump_mb=0;run(0,0);if(save_calls)fail=26;
 /* Actual install path: supported PE signature and expected IAT export. */
 fixture();texture_ready=0;fake_exe[0]='M';fake_exe[1]='Z';sh_put32(fake_exe+60,0x100);sh_put32(fake_exe+0x100,0x4550);fake_exe[0x104]=0x4c;fake_exe[0x105]=1;sh_put32(fake_exe+0x108,0x62f1e445);fake_exe[0x118]=0x0b;fake_exe[0x119]=1;sh_put32(fake_exe+0x118+56,0xe4e000);
 for(U i=0;i<sizeof(shadow_site);i++)fake_exe[0x54845+i]=shadow_site[i];
 P*slot=(P*)(fake_exe+0x3463b8);*slot=memory0;textures_install();if(!texture_ready||*slot!=(P)texture_from_memory||protections!=2||texture_root_len!=8)fail=27;
 fixture();texture_ready=0;*slot=memory0;protection_fail=2;textures_install();if(texture_ready||*slot!=(P)memory0)fail=28;
 fixture();texture_ready=0;*slot=(P)123;textures_install();if(texture_ready||protections||*slot!=(P)123)fail=29;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}

#include "profile_stubs.h"
