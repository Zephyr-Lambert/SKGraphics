/* First texture coverage: main EXE's D3DXCreateTextureFromFileInMemoryEx import.
 * Original textures are loaded first: preserve source-info/palette outputs and
 * keep a valid fallback. Only the final returned COM texture may be replaced.
 */
#include "texture_hash.h"
typedef I(API*TextureMemoryFn)(P,const void*,U,U,U,U,U,U,U,U,U,U,P,P,P*);
typedef I(API*TextureFileFn)(P,const W*,U,U,U,U,U,U,U,U,U,P,P,P*);
typedef I(API*TextureSaveFn)(P*,U,P,P);
static TextureMemoryFn texture_original;
static TextureFileFn texture_file;
static TextureSaveFn texture_save;
static W texture_root[900];static U texture_root_len;
static U texture_lock[6],texture_ready,texture_loads,texture_dumps,texture_bytes,texture_replacements,texture_failures,texture_reported_limit;
static void texture_path(W*out,const char*folder,const char*hash,const char*extension){
 U n=0;for(;n<texture_root_len;n++)out[n]=texture_root[n];
 while(*folder)out[n++]=(W)*folder++;
 if(hash){out[n++]='\\';while(*hash)out[n++]=(W)*hash++;while(*extension)out[n++]=(W)*extension++;}out[n]=0;
}
static void texture_note(const char*event,const char*hash,I hr){begin(event);s(" hash=");s(hash);hx("result",hr);emit();}
static void texture_dump_original(P tex,P palette,const char*hash,W*name){
 if(!texture_dump_enabled||!texture_save)return;
 if(!category_allows(hash)){category_filtered++;return;}
 texture_path(name,"SKGraphics_Textures\\dump",hash,".dds");
 if(GetFileAttributesW(name)!=0xffffffff)return;
 if(texture_dumps>=texture_dump_limit||texture_bytes>=texture_dump_mb*1048576){
  if(!texture_reported_limit++){begin("Texture dump session limit reached; increase DumpLimit/MaxDumpMiB or continue next launch");emit();}return;
 }
 P buffer=0;I hr=texture_save(&buffer,4,tex,palette);
 if(hr<0||!buffer){texture_note("Texture DDS export failed",hash,hr);com_release(buffer);return;}
 P*v=*(P**)buffer;U size=((U(API*)(P))v[4])(buffer);P bytes=((P(API*)(P))v[3])(buffer);
 if(!size||!bytes||size>texture_dump_mb*1048576-texture_bytes){com_release(buffer);return;}
 /* CREATE_NEW: never overwrite an existing dump. A failed partial write is
  * removed; the original texture itself is never modified. */
 P file=CreateFileW(name,0x40000000,1,0,1,0x80,0);
 if(file!=(P)-1&&file){U written=0;I ok=WriteFile(file,bytes,size,&written,0);I closed=CloseHandle(file);
  if(ok&&written==size&&closed){texture_dumps++;texture_bytes+=size;texture_note("Texture dumped DDS",hash,0);}
  else{DeleteFileW(name);texture_note("Texture dump write failed",hash,(I)0x80004005);}
 }else if(GetFileAttributesW(name)==0xffffffff)texture_note("Texture dump file creation failed",hash,(I)0x80004005);
 com_release(buffer);
}
static I API texture_from_memory(P device,const void*source,U bytes,U width,U height,U levels,U usage,U format,U pool,U filter,U mipfilter,U colorkey,P info,P palette,P*out){
 I hr=texture_original(device,source,bytes,width,height,levels,usage,format,pool,filter,mipfilter,colorkey,info,palette,out);
 /* Render/depth/dynamic and palettized textures are outside this prototype. */
 if(hr<0||!out||!*out||!source||!bytes||usage||format==40||format==41||!texture_ready)return hr;
 if(!texture_dump_enabled&&!texture_replace_enabled)return hr;
 Desc d={0};P*v=*(P**)*out;I described=((I(API*)(P,U,Desc*))v[17])(*out,0,&d);
 if(described<0||d.format==40||d.format==41)return hr;
 char hash[65];W name[1024];texture_hash(source,bytes,hash);
 EnterCriticalSection(texture_lock);texture_loads++;
 if(texture_loads<=16){begin("Texture loader observed");s(" hash=");s(hash);kv("width",d.w);kv("height",d.h);kv("format",d.format);kv("sourceBytes",bytes);emit();}
 texture_dump_original(*out,palette,hash,name);
 if(texture_replace_enabled&&texture_file){
  texture_path(name,"SKGraphics_Textures\\replace",hash,".dds");
  U attr=GetFileAttributesW(name);
  if(attr==0xffffffff){texture_path(name,"SKGraphics_Textures\\replace",hash,".png");attr=GetFileAttributesW(name);}
  if(attr!=0xffffffff&&!(attr&16)){
   P replacement=0;
   /* Preserve original metadata returned to the game; DDS/PNG need no palette.
    * Same-size mode uses actual original dimensions, not D3DX default flags. */
   U rw=texture_own_size?0xfffffffe:d.w,rh=texture_own_size?0xfffffffe:d.h;
   I result=texture_file(device,name,rw,rh,levels,usage,d.format,pool,filter,mipfilter,colorkey,0,0,&replacement);
   if(result>=0&&replacement){P original=*out;*out=replacement;com_release(original);texture_replacements++;texture_note("Texture replacement loaded",hash,result);}
   else{com_release(replacement);texture_failures++;texture_note("Texture replacement failed; original retained",hash,result);}
  }
 }
 if(texture_loads==1||texture_loads%250==0){begin("Texture summary");kv("loads",texture_loads);kv("dumped",texture_dumps);kv("categorySkipped",category_filtered);kv("dumpBytes",texture_bytes);kv("replacements",texture_replacements);kv("replacementFailures",texture_failures);emit();}
 LeaveCriticalSection(texture_lock);return hr;
}
static I texture_deferred;
static void textures_install(void){
 if(texture_ready||(!texture_dump_enabled&&!texture_replace_enabled))return;
 texture_deferred=0;
 /* This import RVA was resolved from the provided EXE's import table. Guard
  * with the build signature AND exact resolved export address before writing. */
 unsigned char*b=GetModuleHandleW(0);
 if(!shadow_identify(b)){texture_deferred=1;begin("Texture hook skipped: executable signature does not match a supported build");emit();return;}
P lib=d3dx_library();
 texture_original=(TextureMemoryFn)(lib?GetProcAddress(lib,"D3DXCreateTextureFromFileInMemoryEx"):0);
 texture_file=(TextureFileFn)(lib?GetProcAddress(lib,"D3DXCreateTextureFromFileExW"):0);
 texture_save=(TextureSaveFn)(lib?GetProcAddress(lib,"D3DXSaveTextureToFileInMemory"):0);
 P*slot=(P*)(b+game_build->texture_iat);
 if(!texture_original||*slot!=(P)texture_original){begin("Texture hook skipped: expected D3DX import not found");emit();return;}
 U n=GetModuleFileNameW(module,texture_root,900);if(!n||n>=800){begin("Texture hook skipped: mod directory path too long");emit();return;}
 while(n&&texture_root[n-1]!='\\'&&texture_root[n-1]!='/')n--;texture_root_len=n;
 InitializeCriticalSection(texture_lock);
 W folder[1024];texture_path(folder,"SKGraphics_TextureCategories.ini",0,0);category_load(folder);
 texture_path(folder,"SKGraphics_Textures",0,0);CreateDirectoryW(folder,0);
 texture_path(folder,"SKGraphics_Textures\\dump",0,0);CreateDirectoryW(folder,0);
 texture_path(folder,"SKGraphics_Textures\\replace",0,0);CreateDirectoryW(folder,0);
 U old,unused;if(!VirtualProtect(slot,4,0x04,&old)){begin("Texture hook skipped: import protection failed");emit();return;}
 *slot=(P)texture_from_memory;
 if(!VirtualProtect(slot,4,old,&unused)){
  *slot=(P)texture_original;VirtualProtect(slot,4,old,&unused);begin("Texture hook skipped: import protection restoration failed");emit();return;
 }
 texture_ready=1;begin("Texture loader hook installed");kv("dump",texture_dump_enabled);kv("replace",texture_replace_enabled);kv("ownSize",texture_own_size);kv("dumpAvailable",texture_save!=0);kv("replacementAvailable",texture_file!=0);emit();
}
