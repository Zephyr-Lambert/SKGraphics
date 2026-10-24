/* SKGraphics 1.0. Injector code plus MIT-licensed SMAA; no game assets included.
 * Win32/i386 ABI, freestanding C. Windowed full-frame supersampling, with bounded diagnostics.
 */
typedef unsigned int U;
typedef int I;
typedef unsigned short W;
typedef void *P;
#define API __attribute__((stdcall))
extern U API GetPrivateProfileStringW(const W*,const W*,const W*,W*,U,const W*);
extern U API GetPrivateProfileSectionNamesW(W*,U,const W*);
extern U API GetPrivateProfileSectionW(const W*,W*,U,const W*);
extern U API GetFileAttributesW(const W*);
extern I API CreateDirectoryW(const W*,P);
extern I API DeleteFileW(const W*);
extern void API InitializeCriticalSection(P);
extern void API EnterCriticalSection(P);
extern void API LeaveCriticalSection(P);
extern P API GetModuleHandleW(const W*);
extern P API GetCurrentProcess(void);
extern I API FlushInstructionCache(P,const void*,U);
extern U API GetSystemDirectoryW(W*,U);
extern P API LoadLibraryW(const W*);
extern P API GetProcAddress(P,const char*);
extern U API GetModuleFileNameW(P,W*,U);
extern P API CreateFileW(const W*,U,U,P,U,U,P);
extern I API WriteFile(P,const void*,U,U*,P);
extern I API VirtualProtect(P,U,U,U*);
extern I API CloseHandle(P);
extern U API GetPrivateProfileIntW(const W*,const W*,I,const W*);
static P module,backend,logfile,d3dx_module;
static P d3dx_library(void){
 static const W dll[]={'d','3','d','x','9','_','4','3','.','d','l','l',0};
 if(!d3dx_module)d3dx_module=LoadLibraryW(dll);return d3dx_module;
}
static U logging_enabled;
static U load_reshade,character_preview,ao_exclude_characters;
static U events,limit=4000,frames;
static I initialized;
static U enabled=1,renderw=3840,renderh=2160;
static U aa_method=1,aa_span=8,aa_relative=125,aa_minimum=31,aa_strength=100;
static U smaa_quality=2;
static U shadow_enabled,shadow_size;
static U shadow_probe_enabled=0;
static U scene_probe_enabled=0,ao_probe_enabled=0;
static U ao_depth_view=0,ao_depth_scale=100;
static U ao_enabled=0,ao_strength=100,ao_radius=30,ao_quality=2,ao_bias=10,ao_max_distance=0;
static U bloom_enabled=0,bloom_strength=35,bloom_threshold=750,bloom_knee=100,bloom_radius=100;
static U shadow_filter=1,shadow_filter_radius=200;
static U outline_enabled=1,outline_thickness=100;
static U outline_color_override=0,outline_red=0,outline_green=0,outline_blue=0,outline_opacity=100;
static U texture_dump_enabled,texture_replace_enabled,texture_own_size,texture_dump_limit=500,texture_dump_mb=512;
static void textures_install(void);
static void category_read_options(void);
static W path[32768];
static char line[1024]; static U used;
static void s(const char *v){while(v && *v && used<1020)line[used++]=*v++;}
static void n(U v){char b[11];U j=0;do{b[j++]=(char)('0'+v%10);v/=10;}while(v);while(j && used<1020)line[used++]=b[--j];}
static void x(U v){s("0x");for(I j=7;j>=0;j--){U d=(v>>(j*4))&15;if(used<1020)line[used++]="0123456789abcdef"[d];}}
static void kv(const char *key,U value){s(" ");s(key);s("=");n(value);}
static void hx(const char *key,U value){s(" ");s(key);s("=");x(value);}
static void begin(const char *name){used=0;s(name);kv("frame",frames);}
static void emit(void){U written;if(!logfile || events>=limit)return;s("\r\n");WriteFile(logfile,line,used,&written,0);events++;if(events==limit){const char msg[]="Log event limit reached.\r\n";WriteFile(logfile,msg,sizeof(msg)-1,&written,0);}}
static void suffix(U pos,const char *a){if(pos>=32768)return;while(*a && pos<32767)path[pos++]=(W)*a++;path[pos]=0;}
static U config(const char *section,const char *key,U def){W a[64],b[64];U i=0;for(;section[i]&&i<63;i++)a[i]=section[i];a[i]=0;i=0;for(;key[i]&&i<63;i++)b[i]=key[i];b[i]=0;return GetPrivateProfileIntW(a,b,(I)def,path);}
static void init(void){
 if(initialized)return;initialized=1;
 U k=GetModuleFileNameW(module,path,32768);if(!k||k>=32768)return;
 if(k>32700)return;
 while(k && path[k-1]!='\\' && path[k-1]!='/')k--;
 suffix(k,"SKGraphics.ini");
 static const W section[]={'D','i','a','g','n','o','s','t','i','c','s',0};
 static const W key[]={'M','a','x','L','o','g','E','v','e','n','t','s',0};
 limit=GetPrivateProfileIntW(section,key,4000,path);if(limit<100)limit=100;if(limit>20000)limit=20000;
 logging_enabled=config("Diagnostics","Logging",0)!=0;
 character_preview=config("Diagnostics","CharacterPreview",0);
 load_reshade=config("Compatibility","LoadReShade",0);
 enabled=config("Rendering","Enabled",0);renderw=config("Rendering","Width",3840);renderh=config("Rendering","Height",2160);
 aa_method=config("Antialiasing","Method",0);if(aa_method>2)aa_method=0;
 aa_span=config("Antialiasing","Span",8);if(aa_span<1)aa_span=1;if(aa_span>16)aa_span=16;
 aa_relative=config("Antialiasing","EdgeThreshold",125);if(aa_relative>1000)aa_relative=1000;
 aa_minimum=config("Antialiasing","MinimumThreshold",31);if(aa_minimum>1000)aa_minimum=1000;
 aa_strength=config("Antialiasing","Strength",100);if(aa_strength>100)aa_strength=100;
 smaa_quality=config("Antialiasing","SMAAQuality",2);if(smaa_quality>3)smaa_quality=2;
 shadow_enabled=config("Shadows","Enabled",0);shadow_size=config("Shadows","Resolution",4096);
 bloom_enabled=config("Bloom","Enabled",0);
 bloom_strength=config("Bloom","Strength",35);if(bloom_strength>200)bloom_strength=200;
 bloom_threshold=config("Bloom","Threshold",750);if(bloom_threshold>1000)bloom_threshold=1000;
 bloom_knee=config("Bloom","Knee",100);if(bloom_knee>500)bloom_knee=500;
 bloom_radius=config("Bloom","Radius",100);if(bloom_radius<25)bloom_radius=25;if(bloom_radius>400)bloom_radius=400;
 scene_probe_enabled=config("Diagnostics","SceneProbe",0);
 ao_probe_enabled=config("Diagnostics","AOProbe",0);
 ao_exclude_characters=config("AmbientOcclusion","ExcludeCharacters",0);
 ao_enabled=config("AmbientOcclusion","Enabled",0);
 ao_strength=config("AmbientOcclusion","Strength",100);if(ao_strength>300)ao_strength=300;
 ao_radius=config("AmbientOcclusion","Radius",30);if(ao_radius<1)ao_radius=1;if(ao_radius>200)ao_radius=200;
 ao_quality=config("AmbientOcclusion","Quality",2);if(ao_quality<1)ao_quality=1;if(ao_quality>3)ao_quality=3;
 ao_bias=config("AmbientOcclusion","Bias",10);if(ao_bias>50)ao_bias=50;
 ao_max_distance=config("AmbientOcclusion","MaxDistance",0);if(ao_max_distance>100000)ao_max_distance=100000;
 ao_depth_view=config("AmbientOcclusion","DepthView",0);
 ao_depth_scale=config("AmbientOcclusion","DepthScale",100);if(ao_depth_scale<1)ao_depth_scale=1;if(ao_depth_scale>100000)ao_depth_scale=100000;
 shadow_probe_enabled=config("Diagnostics","ShadowProbe",0);
 shadow_filter=config("Shadows","Filter",0);if(shadow_filter>2)shadow_filter=0;
 shadow_filter_radius=config("Shadows","FilterRadius",200);if(shadow_filter_radius<25)shadow_filter_radius=25;if(shadow_filter_radius>800)shadow_filter_radius=800;
 outline_enabled=config("Outlines","Enabled",1);outline_thickness=config("Outlines","Thickness",100);if(outline_thickness<25)outline_thickness=25;if(outline_thickness>400)outline_thickness=400;
 outline_color_override=config("Outlines","ColorOverride",0);
 outline_red=config("Outlines","Red",0);if(outline_red>255)outline_red=255;
 outline_green=config("Outlines","Green",0);if(outline_green>255)outline_green=255;
 outline_blue=config("Outlines","Blue",0);if(outline_blue>255)outline_blue=255;
 outline_opacity=config("Outlines","Opacity",100);if(outline_opacity>100)outline_opacity=100;
 texture_dump_enabled=config("Textures","Dump",0);texture_replace_enabled=config("Textures","Replace",0);texture_own_size=config("Textures","UseReplacementSize",0);
 texture_dump_limit=config("Textures","DumpLimit",500);if(texture_dump_limit>10000)texture_dump_limit=10000;
 texture_dump_mb=config("Textures","MaxDumpMiB",512);if(texture_dump_mb>2048)texture_dump_mb=2048;
 if(!logging_enabled)scene_probe_enabled=ao_probe_enabled=shadow_probe_enabled=0;
 category_read_options();
 suffix(k,"SKGraphics.log");
 if(logging_enabled){logfile=CreateFileW(path,0x40000000,1,0,2,0x80,0);if(logfile==(P)-1)logfile=0;}
 begin("SKGraphics 1.0");kv("enabled",enabled);kv("configuredWidth",renderw);kv("configuredHeight",renderh);kv("aaMethod",aa_method);kv("smaaQuality",smaa_quality);emit();
 /* Load ReShade below our wrapper so it sees physical dimensions and its
  * internal device calls do not pass through the game's scaling hooks. */
 if(load_reshade){
  suffix(k,"ReShade\\d3d9.dll");backend=LoadLibraryW(path);
  if(backend==module)backend=0;
  if(backend&&!GetProcAddress(backend,"Direct3DCreate9"))backend=0;
  begin(backend?"Backend loaded from ReShade subdirectory":"ERROR: could not load ReShade backend; check ReShade/d3d9.dll");emit();
 }else{
  k=GetSystemDirectoryW(path,32768);if(!k||k>32750)return;
  suffix(k,"\\d3d9.dll");backend=LoadLibraryW(path);
  if(backend==module)backend=0;
  begin(backend?"Backend loaded from Windows system directory":"ERROR: could not load system D3D9");emit();
 }
 textures_install();
}
/* Patch each backend vtable once; retain originals by vtable, not by object.
 * AA-owned COM resources are released before reset or a new device creation.
 */
typedef struct { P *vt; P original[119]; U count; U hooked[4]; } Table;
static Table tables[16]; static U table_count;
static Table *lookup(P o){if(!o)return 0;P *v=*(P**)o;for(U i=0;i<table_count;i++)if(tables[i].vt==v)return &tables[i];return 0;}
static P orig(P o,U slot){Table*t=lookup(o);return t?t->original[slot]:0;}
static Table *record(P o,U count){Table*t=lookup(o);if(t)return t;if(table_count==16)return 0;t=&tables[table_count++];t->vt=*(P**)o;t->count=count;for(U i=0;i<count;i++)t->original[i]=t->vt[i];return t;}
/* Do not overwrite a wrapper installed above us on a later factory call.
 * Its saved trampoline may already point to our hook. Updating original[]
 * to that wrapper would create recursion; preserve both links instead.
 * Mark only successful installs, so a protection failure remains retryable. */
static void hook(Table*t,U slot,P fn){U old,unused;if(!t||slot>=t->count)return;U mask=1u<<(slot&31);if(t->hooked[slot>>5]&mask)return;if(VirtualProtect(t->vt+slot,4,0x40,&old)){P previous=t->vt[slot];t->vt[slot]=fn;if(VirtualProtect(t->vt+slot,4,old,&unused))t->hooked[slot>>5]|=mask;else{t->vt[slot]=previous;VirtualProtect(t->vt+slot,4,old,&unused);begin("ERROR: vtable hook rolled back after protection restoration failure");kv("slot",slot);emit();}}else{begin("ERROR: vtable hook failed");kv("slot",slot);emit();}}
typedef struct {U w,h,format,count,ms,quality,swap;P window;I windowed,auto_depth;U depth,flags,refresh,interval;} PP;
typedef struct {U format,type,usage,pool,ms,quality,w,h;} Desc;
typedef struct {U x,y,w,h;float minz,maxz;} VP;
typedef struct {I left,top,right,bottom;} Rect;
typedef struct {P obj;U lw,lh,rw,rh;I active,main;
 P cm_tex,cm_surf;U cm_ready,cm_failed,cm_w,cm_h,cm_draws;
 P ao_depth,ao_ps,ao_tex,ao_surf,ao_shaders[2];U ao_w,ao_h,ao_valid,ao_projection_warned,ao_failed;float ao_proj[8];U ao_passes,ao_missing;
 P bl_tex[2],bl_surf[2],bl_ps[5],bl_scene,bl_bgtex,bl_bgsurf;U bl_bgw,bl_bgh,bl_early_ui,bl_mask;U bl_w,bl_h,bl_done,bl_failed,bl_passes;
 I shadow_pass,shadow_sampled;U shadow_frame;
 P probe_color,probe_depth;U probe_checks;
 P sm_tex[2],sm_surface[2],sm_shader[3],sm_area,sm_search;U aa_cached_method;
 P aa_tex,aa_surface,aa_shader;U aa_w,aa_h,aa_format,aa_passes;I aa_busy,aa_failed;
} State;
static State states[8];static U state_count;
static State *state(P o){for(U i=0;i<state_count;i++)if(states[i].obj==o)return &states[i];return 0;}
static State *newstate(P o){State*t=state(o);if(t)return t;for(U i=0;i<state_count;i++)if(!states[i].obj){states[i].obj=o;return &states[i];}if(state_count==8)return 0;t=&states[state_count++];t->obj=o;return t;}
static void select_size(State*t,PP*p){
 t->active=0;t->main=1;t->lw=p->w;t->lh=p->h;t->rw=p->w;t->rh=p->h;
 if(!enabled)return;
 if(!p->windowed||!p->w||!p->h||p->ms||renderw<320||renderh<180||renderw>16384||renderh>16384){begin("Resolution override disabled: unsupported mode or dimensions");emit();return;}
 /* Strict same-aspect first release: no hidden image stretching. */
 if(renderw*p->h!=renderh*p->w){begin("Resolution override disabled: render/output aspect ratios differ");emit();return;}
 t->rw=renderw;t->rh=renderh;t->active=(t->rw!=t->lw||t->rh!=t->lh);
}
static U scale(U v,U to,U from){return (v*to+from/2)/from;}
static I main_surface(State*t,P target){if(!t||!target)return 0;Desc d={0};P*v=*(P**)target;I hr=((I(API*)(P,Desc*))v[12])(target,&d);return hr>=0&&d.w==t->rw&&d.h==t->rh;}
static void resize(State*t,U*w,U*h){if(t&&t->active&&*w==t->lw&&*h==t->lh){*w=t->rw;*h=t->rh;}}
static const Rect *rectscale(State*t,const Rect*r,Rect*out){if(!r||!t||!t->active)return r;*out=*r;out->left=(I)scale((U)r->left,t->rw,t->lw);out->right=(I)scale((U)r->right,t->rw,t->lw);out->top=(I)scale((U)r->top,t->rh,t->lh);out->bottom=(I)scale((U)r->bottom,t->rh,t->lh);return out;}
static void pp(const char *name,PP*p){begin(name);if(p){kv("width",p->w);kv("height",p->h);kv("format",p->format);kv("windowed",p->windowed);kv("autoDepth",p->auto_depth);kv("depthFormat",p->depth);kv("multisample",p->ms);}emit();}
/* Unique bind/viewport signatures avoid logging thousands of identical passes. */
static U seen[512][8],seen_count;
static I fresh(U a,U b,U c,U d,U e,U f,U g,U h){if(!logfile||events>=limit)return 0;U z[8]={a,b,c,d,e,f,g,h};for(U i=0;i<seen_count;i++){U j=0;while(j<8&&seen[i][j]==z[j])j++;if(j==8)return 0;}if(seen_count==512)return 0;for(U j=0;j<8;j++)seen[seen_count][j]=z[j];seen_count++;return 1;}
static void surface(const char *name,U kind,U slot,P o,I hr){
 if(!logfile||events>=limit)return;
 Desc d={0};I ok=-1;if(o){P *v=*(P**)o;ok=((I(API*)(P,Desc*))v[12])(o,&d);}
 if(!logfile)return;
 if(fresh(kind,slot,d.w,d.h,d.format,d.ms,(U)hr,(U)ok)){begin(name);kv("slot",slot);kv("width",d.w);kv("height",d.h);kv("format",d.format);kv("multisample",d.ms);hx("result",hr);emit();}
}
#include "aa_pass.h"
#include "shadows.h"
#include "texture_categories.h"
#include "textures.h"
#include "shadow_probe.h"
#include "shadow_filter.h"
#include "outlines.h"
#include "scene_probe.h"
static void character_cleanup(P);
#include "ao_depth.h"
#include "bloom.h"
#include "character_preview.h"
#include "character_mask.h"
static I API reset(P o,PP*p){
 if(scene_device==o)scene_finish("Reset");
 pp("Reset requested",p);State*t=state(o);if(!p||!t)return ((I(API*)(P,PP*))orig(o,16))(o,p);
 bloom_cleanup(t);t->bl_failed=0;t->ao_failed=0;t->cm_failed=0;probe_cleanup(t);aa_cleanup(t);t->aa_failed=0;State previous=*t;select_size(t,p);PP request=*p;request.w=t->rw;request.h=t->rh;
 I hr=((I(API*)(P,PP*))orig(o,16))(o,&request);
 begin("Reset result");hx("result",hr);emit();
 if(hr>=0){seen_count=0;pp("Reset actual",&request);*p=request;p->w=t->lw;p->h=t->lh;}else *t=previous;return hr;
}
static I API present(P o,P a,P b,P w,P dirty){State*t=state(o);scene_present(o);bloom_frame_end(t);aa_apply(t);Rect r;const Rect*src=rectscale(t,(const Rect*)a,&r);I hr=((I(API*)(P,const Rect*,P,P,P))orig(o,17))(o,src,b,w,dirty);frames++;if(frames==1||frames==120||frames%600==0){begin("Present");hx("result",hr);if(t){kv("logicalWidth",t->lw);kv("logicalHeight",t->lh);kv("renderWidth",t->rw);kv("renderHeight",t->rh);kv("shadowShadersInspected",sf_inspected);kv("shadowShadersApplied",sf_matches);kv("shadowShaderReadFailures",sf_read_failures);kv("outlinePixelsReplaced",ol_pixels);kv("outlineVerticesReplaced",ol_vertices);}emit();}return hr;}
static I API texture(P o,U w,U h,U levels,U usage,U fmt,U pool,P*out,P shared){if(usage&3)resize(state(o),&w,&h);State*t=state(o);if((ao_depth_view||ao_enabled)&&shadow_image&&t&&w==t->rw&&h==t->rh&&!(w==shadow_actual&&h==shadow_actual)&&usage==2&&fmt==75&&pool==0&&levels==1&&!shared&&out){P candidate=0;I test=((I(API*)(P,U,U,U,U,U,U,P*,P))orig(o,23))(o,w,h,levels,usage,0x5a544e49,pool,&candidate,shared);begin("AO INTZ depth allocation");kv("width",w);kv("height",h);hx("result",test);emit();if(test>=0&&candidate){*out=candidate;return test;}com_release(candidate);}I hr=((I(API*)(P,U,U,U,U,U,U,P*,P))orig(o,23))(o,w,h,levels,usage,fmt,pool,out,shared);if(usage&3){begin("CreateTexture render/depth target");kv("width",w);kv("height",h);kv("levels",levels);hx("usage",usage);kv("format",fmt);hx("result",hr);emit();}return hr;}
static I API rt(P o,U w,U h,U fmt,U ms,U quality,I lockable,P*out,P shared){resize(state(o),&w,&h);I hr=((I(API*)(P,U,U,U,U,U,I,P*,P))orig(o,28))(o,w,h,fmt,ms,quality,lockable,out,shared);begin("CreateRenderTarget");kv("width",w);kv("height",h);kv("format",fmt);kv("multisample",ms);hx("result",hr);emit();return hr;}
static I API depth(P o,U w,U h,U fmt,U ms,U quality,I discard,P*out,P shared){resize(state(o),&w,&h);I hr=((I(API*)(P,U,U,U,U,U,I,P*,P))orig(o,29))(o,w,h,fmt,ms,quality,discard,out,shared);begin("CreateDepthStencilSurface");kv("width",w);kv("height",h);kv("format",fmt);kv("multisample",ms);hx("result",hr);emit();return hr;}
static I API setrt(P o,U slot,P target){I hr=((I(API*)(P,U,P))orig(o,37))(o,slot,target);surface("SetRenderTarget",1,slot,target,hr);State*t=state(o);if(t&&slot==0&&hr>=0){t->main=main_surface(t,target);t->shadow_pass=shadow_binding(__builtin_return_address(0));if(t->shadow_pass)probe_surface(t,target,0);}return hr;}
static I API setdepth(P o,P target){I hr=((I(API*)(P,P))orig(o,39))(o,target);surface("SetDepthStencilSurface",2,0,target,hr);State*t=state(o);if(hr>=0&&t&&t->shadow_pass)probe_surface(t,target,1);return hr;}
static I API viewport(P o,const VP*v){State*t=state(o);VP adjusted;if(v&&t&&t->active&&t->main){adjusted=*v;adjusted.x=scale(v->x,t->rw,t->lw);adjusted.y=scale(v->y,t->rh,t->lh);adjusted.w=scale(v->w,t->rw,t->lw);adjusted.h=scale(v->h,t->rh,t->lh);v=&adjusted;}I hr=((I(API*)(P,const VP*))orig(o,47))(o,v);if(v&&fresh(3,v->x,v->y,v->w,v->h,0,0,(U)hr)){begin("SetViewport actual");kv("x",v->x);kv("y",v->y);kv("width",v->w);kv("height",v->h);hx("result",hr);emit();}return hr;}
static I API getviewport(P o,VP*v){I hr=((I(API*)(P,VP*))orig(o,48))(o,v);State*t=state(o);if(hr>=0&&v&&t&&t->active&&t->main){v->x=scale(v->x,t->lw,t->rw);v->y=scale(v->y,t->lh,t->rh);v->w=scale(v->w,t->lw,t->rw);v->h=scale(v->h,t->lh,t->rh);}return hr;}
static I API scissor(P o,const Rect*r){State*t=state(o);Rect out;const Rect*p=(t&&t->main)?rectscale(t,r,&out):r;return ((I(API*)(P,const Rect*))orig(o,75))(o,p);}
static I API getscissor(P o,Rect*r){I hr=((I(API*)(P,Rect*))orig(o,76))(o,r);State*t=state(o);if(hr>=0&&r&&t&&t->active&&t->main){r->left=scale(r->left,t->lw,t->rw);r->right=scale(r->right,t->lw,t->rw);r->top=scale(r->top,t->lh,t->rh);r->bottom=scale(r->bottom,t->lh,t->rh);}return hr;}
static I API stretch(P o,P src,P sr,P dst,P dr,U filter){State*t=state(o);Rect a,b;const Rect*srect=main_surface(t,src)?rectscale(t,sr,&a):sr;const Rect*drect=main_surface(t,dst)?rectscale(t,dr,&b):dr;I hr=((I(API*)(P,P,const Rect*,P,const Rect*,U))orig(o,34))(o,src,srect,dst,drect,filter);if(scene_recording(o)){scene_begin("SceneProbe StretchRect");scene_surface(src,"source");scene_surface(dst,"destination");hx("result",hr);scene_emit();}surface("StretchRect source",4,filter,src,hr);surface("StretchRect destination",5,filter,dst,hr);return hr;}
static U draw_sample_frame;static I draw_sampled;
static void draw_sample(P o,I hr){if(!logfile||events>=limit)return;State*t=state(o);if(!t||!t->main)return;if(hr>=0&&draw_sampled&&frames-draw_sample_frame<120)return;draw_sampled=1;draw_sample_frame=frames;VP v={0};I ok=((I(API*)(P,VP*))orig(o,48))(o,&v);begin("Main draw actual viewport");kv("width",v.w);kv("height",v.h);hx("drawResult",hr);hx("viewportResult",ok);emit();}
static I API draw(P o,U type,U start,U count){bloom_before_draw(o,__builtin_return_address(0));scene_draw(o,81,type,count,__builtin_return_address(0));probe_draw(o);P saved=character_before(o);I hr=((I(API*)(P,U,U,U))orig(o,81))(o,type,start,count);character_after(o,saved);CharacterMaskState mask;if(hr>=0&&cm_begin(o,&mask)){I replay=((I(API*)(P,U,U,U))orig(o,81))(o,type,start,count);cm_end(o,&mask,replay);}draw_sample(o,hr);shadow_draw_sample(o,hr);return hr;}
static I API indexed(P o,U type,I base,U min,U count,U start,U primitives){bloom_before_draw(o,__builtin_return_address(0));scene_draw(o,82,type,primitives,__builtin_return_address(0));probe_draw(o);P saved=character_before(o);I hr=((I(API*)(P,U,I,U,U,U,U))orig(o,82))(o,type,base,min,count,start,primitives);character_after(o,saved);CharacterMaskState mask;if(hr>=0&&cm_begin(o,&mask)){I replay=((I(API*)(P,U,I,U,U,U,U))orig(o,82))(o,type,base,min,count,start,primitives);cm_end(o,&mask,replay);}draw_sample(o,hr);shadow_draw_sample(o,hr);return hr;}
static I API drawup(P o,U type,U count,const void*v,U stride){bloom_before_draw(o,__builtin_return_address(0));scene_draw(o,83,type,count,__builtin_return_address(0));probe_draw(o);P saved=character_before(o);I hr=((I(API*)(P,U,U,const void*,U))orig(o,83))(o,type,count,v,stride);character_after(o,saved);CharacterMaskState mask;if(hr>=0&&cm_begin(o,&mask)){I replay=((I(API*)(P,U,U,const void*,U))orig(o,83))(o,type,count,v,stride);cm_end(o,&mask,replay);}return hr;}
static I API indexedup(P o,U type,U min,U count,U primitives,const void*indices,U format,const void*vertices,U stride){bloom_before_draw(o,__builtin_return_address(0));scene_draw(o,84,type,primitives,__builtin_return_address(0));probe_draw(o);P saved=character_before(o);I hr=((I(API*)(P,U,U,U,U,const void*,U,const void*,U))orig(o,84))(o,type,min,count,primitives,indices,format,vertices,stride);character_after(o,saved);CharacterMaskState mask;if(hr>=0&&cm_begin(o,&mask)){I replay=((I(API*)(P,U,U,U,U,const void*,U,const void*,U))orig(o,84))(o,type,min,count,primitives,indices,format,vertices,stride);cm_end(o,&mask,replay);}return hr;}
static I API device(P o,U adapter,U type,P window,U behavior,PP*p,P*out){
 /* This game's primary device is the supported path. Release cached AA
    * resources from previous device sessions before creating another device. */
 scene_finish("CreateDevice");
 for(U i=0;i<state_count;i++){bloom_cleanup(&states[i]);probe_cleanup(&states[i]);aa_cleanup(&states[i]);}
 pp("CreateDevice requested",p);
 State candidate={0};PP request;
 if(p){select_size(&candidate,p);request=*p;request.w=candidate.rw;request.h=candidate.rh;}
 I hr=((I(API*)(P,U,U,P,U,PP*,P*))orig(o,16))(o,adapter,type,window,behavior,p?&request:p,out);
 if(hr<0&&p&&candidate.active){begin("High-resolution CreateDevice failed; retrying original dimensions");hx("result",hr);emit();candidate.active=0;candidate.rw=candidate.lw;candidate.rh=candidate.lh;request=*p;hr=((I(API*)(P,U,U,P,U,PP*,P*))orig(o,16))(o,adapter,type,window,behavior,&request,out);}
 if(hr>=0&&p){pp("CreateDevice actual",&request);*p=request;p->w=candidate.lw;p->h=candidate.lh;}

 begin("CreateDevice result");hx("result",hr);emit();
 if(hr>=0&&out&&*out){State*st=newstate(*out);if(st){aa_cleanup(st);*st=candidate;st->obj=*out;}Table*t=record(*out,119);if(t){
 hook(t,41,scene_beginscene);hook(t,42,scene_endscene);hook(t,43,scene_clear);
 hook(t,16,reset);hook(t,17,present);hook(t,23,texture);hook(t,28,rt);hook(t,29,depth);hook(t,34,stretch);hook(t,37,setrt);hook(t,39,setdepth);hook(t,47,viewport);hook(t,48,getviewport);hook(t,75,scissor);hook(t,76,getscissor);hook(t,81,draw);hook(t,82,indexed);hook(t,83,drawup);hook(t,84,indexedup);hook(t,106,outline_pixel_shader);hook(t,91,bloom_vertex_shader);
 P back=0;P*v=*(P**)*out;I result=((I(API*)(P,U,U,U,P*))v[18])(*out,0,0,0,&back);if(result>=0&&back){surface("Initial backbuffer",6,0,back,result);P*b=*(P**)back;((U(API*)(P))b[2])(back);}
 /* Retry after normal game startup if its loaded code was not ready in init. */
 if(texture_deferred&&!shadow_attempted)textures_install();
 shadow_setup(*out);
 begin("AO configuration");kv("enabled",ao_enabled);kv("strength",ao_strength);kv("radius",ao_radius);kv("quality",ao_quality);kv("bias",ao_bias);kv("maxDistance",ao_max_distance);kv("depthView",ao_depth_view);kv("excludeCharacters",ao_exclude_characters);emit();
 begin("Bloom configuration");kv("enabled",bloom_enabled);kv("strength",bloom_strength);kv("threshold",bloom_threshold);kv("knee",bloom_knee);kv("radius",bloom_radius);emit();
 begin("Outline configuration");kv("enabled",outline_enabled);kv("thicknessPercent",outline_thickness);kv("colorOverride",outline_color_override);kv("red",outline_red);kv("green",outline_green);kv("blue",outline_blue);kv("opacity",outline_opacity);emit();
 pp("CreateDevice effective",p);
 }}return hr;
}
P API Direct3DCreate9(U sdk){init();if(!backend)return 0;P fn=GetProcAddress(backend,"Direct3DCreate9");if(!fn)return 0;P o=((P(API*)(U))fn)(sdk);begin("Direct3DCreate9");kv("sdk",sdk);kv("success",o!=0);emit();if(o)hook(record(o,17),16,device);return o;}
/* Optional performance markers used by middleware are transparently forwarded. */
I API D3DPERF_BeginEvent(U color,const W*name){init();P f=backend?GetProcAddress(backend,"D3DPERF_BeginEvent"):0;return f?((I(API*)(U,const W*))f)(color,name):-1;}
I API D3DPERF_EndEvent(void){init();P f=backend?GetProcAddress(backend,"D3DPERF_EndEvent"):0;return f?((I(API*)(void))f)():-1;}
void API D3DPERF_SetMarker(U color,const W*name){init();P f=backend?GetProcAddress(backend,"D3DPERF_SetMarker"):0;if(f)((void(API*)(U,const W*))f)(color,name);}
void API D3DPERF_SetRegion(U color,const W*name){init();P f=backend?GetProcAddress(backend,"D3DPERF_SetRegion"):0;if(f)((void(API*)(U,const W*))f)(color,name);}
U API D3DPERF_GetStatus(void){init();P f=backend?GetProcAddress(backend,"D3DPERF_GetStatus"):0;return f?((U(API*)(void))f)():0;}
I API D3DPERF_QueryRepeatFrame(void){init();P f=backend?GetProcAddress(backend,"D3DPERF_QueryRepeatFrame"):0;return f?((I(API*)(void))f)():0;}
void API D3DPERF_SetOptions(U opts){init();P f=backend?GetProcAddress(backend,"D3DPERF_SetOptions"):0;if(f)((void(API*)(U))f)(opts);}
I API DllMain(P m,U reason,P reserved){(void)reserved;if(reason==1)module=m;if(reason==0){if(logfile)CloseHandle(logfile);if(scene_log)CloseHandle(scene_log);}return 1;}
