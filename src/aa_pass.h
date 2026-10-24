/* Full-frame AA, before Present. All game state is restored after the pass. */
#include "fxaa_source.h"
#include "smaa_source.h"
#include "smaa/AreaTex.h"
#include "smaa/SearchTex.h"
static P sm_bytecode[3];static I sm_compile_failed;
static P aa_bytecode;static I aa_compile_failed;
static void com_release(P p){if(p){P*v=*(P**)p;((U(API*)(P))v[2])(p);}}
static void aa_cleanup(State*t){if(!t||t->aa_busy)return;t->aa_busy=1;P surf=t->aa_surface,tex=t->aa_tex,ps=t->aa_shader;t->aa_surface=0;t->aa_tex=0;t->aa_shader=0;t->aa_w=t->aa_h=0;com_release(surf);com_release(tex);com_release(ps);for(U i=0;i<2;i++){com_release(t->sm_surface[i]);com_release(t->sm_tex[i]);t->sm_surface[i]=t->sm_tex[i]=0;}for(U i=0;i<3;i++){com_release(t->sm_shader[i]);t->sm_shader[i]=0;}com_release(t->sm_area);com_release(t->sm_search);t->sm_area=t->sm_search=0;t->aa_cached_method=0;t->aa_busy=0;}
/* Cache lifetime is tied to Reset / next CreateDevice, not transient device
 * Release calls (GetDevice/Release pairs occur during normal frames).
 * Cached resources may hold the last device references until next creation or
 * process teardown. Do not call COM cleanup from DllMain under loader lock.
 */
static I aa_compile(void){
 if(aa_bytecode)return 1;if(aa_compile_failed)return 0;

 P mod=d3dx_library();P f=mod?GetProcAddress(mod,"D3DXCompileShader"):0;
 if(!f){begin("AA compiler unavailable; AA disabled");emit();aa_compile_failed=1;return 0;}
 P errors=0;
 I hr=((I(API*)(const char*,U,P,P,const char*,const char*,U,P*,P*,P*))f)(aa_source,sizeof(aa_source)-1,0,0,"main","ps_3_0",0,&aa_bytecode,&errors,0);
 begin("AA shader compilation");hx("result",hr);emit();
 if(hr<0||!aa_bytecode){if(errors){P*v=*(P**)errors;char*msg=((P(API*)(P))v[3])(errors);begin("AA compiler message: ");if(msg)s(msg);emit();}aa_compile_failed=1;}
 com_release(errors);if(aa_compile_failed){com_release(aa_bytecode);aa_bytecode=0;}return !aa_compile_failed;
}
/* Three separately compiled ps_3_0 entry points. Presets are compile-time. */
static I sm_compile(void){
 if(sm_bytecode[0]&&sm_bytecode[1]&&sm_bytecode[2])return 1;if(sm_compile_failed)return 0;

 static const char*presets[]={"SMAA_PRESET_LOW","SMAA_PRESET_MEDIUM","SMAA_PRESET_HIGH","SMAA_PRESET_ULTRA"};
 static const char*entries[]={"edges","weights","blend"};
 struct Macro{const char*name,*value;} defines[2]={{presets[smaa_quality],"1"},{0,0}};
 P mod=d3dx_library();P f=mod?GetProcAddress(mod,"D3DXCompileShader"):0;
 if(!f){begin("SMAA compiler unavailable");emit();sm_compile_failed=1;return 0;}
 for(U i=0;i<3;i++){
  P errors=0;I hr=((I(API*)(const char*,U,P,P,const char*,const char*,U,P*,P*,P*))f)(smaa_source,sizeof(smaa_source)-1,defines,0,entries[i],"ps_3_0",0,&sm_bytecode[i],&errors,0);
  begin("SMAA shader compilation");kv("pass",i);kv("quality",smaa_quality);hx("result",hr);emit();
  if(hr<0||!sm_bytecode[i]){if(errors){P*v=*(P**)errors;char*msg=((P(API*)(P))v[3])(errors);begin("SMAA compiler message: ");if(msg)s(msg);emit();}sm_compile_failed=1;}
  com_release(errors);if(sm_compile_failed){for(U j=0;j<3;j++){com_release(sm_bytecode[j]);sm_bytecode[j]=0;}return 0;}
 }return 1;
}
static I aa_target(P o,U w,U h,U format,P*tex,P*surface){
 I hr=((I(API*)(P,U,U,U,U,U,U,P*,P))orig(o,23))(o,w,h,1,1,format,0,tex,0);
 if(hr>=0&&*tex){P*v=*(P**)*tex;hr=((I(API*)(P,U,P*))v[18])(*tex,0,surface);}
 return hr>=0&&*tex&&*surface;
}
/* Expand lookup channels into widely supported A8R8G8B8, respecting row pitch.
 * R/G contain the original area bytes; R contains the search byte.
 * MANAGED lookup textures are immutable after upload and released on Reset.
 */
static I sm_lookup(P o,U w,U h,const unsigned char*src,U channels,P*out){
 I hr=((I(API*)(P,U,U,U,U,U,U,P*,P))orig(o,23))(o,w,h,1,0,21,1,out,0);
 if(hr<0||!*out)return 0;
 struct Locked{I pitch;P bits;} lock={0};P*v=*(P**)*out;
 hr=((I(API*)(P,U,P,P,U))v[19])(*out,0,&lock,0,0);if(hr<0)return 0;
 if(!lock.bits||lock.pitch<(I)(w*4)){((I(API*)(P,U))v[20])(*out,0);return 0;}
 for(U y=0;y<h;y++){unsigned char*row=(unsigned char*)lock.bits+y*lock.pitch;for(U x=0;x<w;x++){
 U k=(y*w+x)*channels;row[x*4]=0;row[x*4+1]=channels==2?src[k+1]:0;row[x*4+2]=src[k];row[x*4+3]=255;
 }}
 return ((I(API*)(P,U))v[20])(*out,0)>=0;
}
static I aa_resources(State*t,Desc*d){
 if(t->aa_tex&&t->aa_w==d->w&&t->aa_h==d->h&&t->aa_format==d->format&&t->aa_cached_method==aa_method)return 1;
 t->aa_busy=0;aa_cleanup(t);t->aa_busy=1;
 P o=t->obj;I ok=1;
 if(aa_method==2){
  if(!sm_compile())return 0;
  for(U i=0;i<3&&ok;i++){P*v=*(P**)sm_bytecode[i];P code=((P(API*)(P))v[3])(sm_bytecode[i]);ok=((I(API*)(P,const U*,P*))orig(o,106))(o,code,&t->sm_shader[i])>=0&&t->sm_shader[i];}
  for(U i=0;i<2&&ok;i++)ok=aa_target(o,d->w,d->h,21,&t->sm_tex[i],&t->sm_surface[i]);
  if(ok)ok=sm_lookup(o,AREATEX_WIDTH,AREATEX_HEIGHT,areaTexBytes,2,&t->sm_area);
  if(ok)ok=sm_lookup(o,SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,searchTexBytes,1,&t->sm_search);
 }else{
  if(!aa_compile())return 0;P*v=*(P**)aa_bytecode;P code=((P(API*)(P))v[3])(aa_bytecode);
  ok=((I(API*)(P,const U*,P*))orig(o,106))(o,code,&t->aa_shader)>=0&&t->aa_shader;
 }
 if(ok)ok=aa_target(o,d->w,d->h,d->format,&t->aa_tex,&t->aa_surface);
 begin(aa_method==2?"SMAA resources":"AA resources");kv("width",d->w);kv("height",d->h);kv("format",d->format);kv("success",ok);emit();
 if(!ok)return 0;t->aa_w=d->w;t->aa_h=d->h;t->aa_format=d->format;t->aa_cached_method=aa_method;return 1;
}
static I aa_rs(P o,U key,U value){return ((I(API*)(P,U,U))orig(o,57))(o,key,value);}
static I aa_ss(P o,U key,U value){return ((I(API*)(P,U,U,U))orig(o,69))(o,0,key,value);}
static void aa_apply(State*t){
 if(!aa_method||!aa_strength||!t||t->aa_failed||t->aa_busy)return;
 P o=t->obj;I hr=0,restore=0,begun=0,modified=0;const char*step="cooperative level";
 P back=0,saved[4]={0},depthbuf=0,block=0;VP oldvp={0};Desc desc={0};
 t->aa_busy=1;
 hr=((I(API*)(P))orig(o,3))(o);if(hr<0)goto done;
 step="backbuffer";hr=((I(API*)(P,U,U,U,P*))orig(o,18))(o,0,0,0,&back);if(hr<0||!back)goto failed;
 {P*v=*(P**)back;hr=((I(API*)(P,Desc*))v[12])(back,&desc);}if(hr<0)goto failed;
 step="resources";if(!aa_resources(t,&desc)){hr=(I)0x80004005;goto failed;}
 step="frame copy";hr=((I(API*)(P,P,P,P,P,U))orig(o,34))(o,back,0,t->aa_surface,0,0);if(hr<0)goto failed;
 step="state capture";hr=((I(API*)(P,U,P*))orig(o,59))(o,1,&block);if(hr<0||!block)goto failed;
 /* MRTs and depth surfaces are not reliably covered by the state block. */
 for(U i=0;i<4;i++)((I(API*)(P,U,P*))orig(o,38))(o,i,&saved[i]);
 if(!saved[0]){hr=(I)0x80004005;goto failed;}
 ((I(API*)(P,P*))orig(o,40))(o,&depthbuf);
 hr=((I(API*)(P,VP*))orig(o,48))(o,&oldvp);if(hr<0)goto failed;
 step="begin scene";hr=((I(API*)(P))orig(o,41))(o);if(hr<0)goto failed;begun=1;modified=1;
#define AA_DO(expr) do {hr=(expr);if(hr<0)goto failed;} while(0)
 step="pass setup";
 AA_DO(((I(API*)(P,P))orig(o,39))(o,0));
 for(U i=1;i<4;i++)if(saved[i])AA_DO(((I(API*)(P,U,P))orig(o,37))(o,i,0));
 AA_DO(((I(API*)(P,U,P))orig(o,37))(o,0,back));
 {VP vp={0,0,desc.w,desc.h,0.0f,1.0f};AA_DO(((I(API*)(P,const VP*))orig(o,47))(o,&vp));}
 AA_DO(((I(API*)(P,P))orig(o,92))(o,0));
 AA_DO(((I(API*)(P,U))orig(o,89))(o,0x104));
 AA_DO(((I(API*)(P,U,U))orig(o,102))(o,0,1));
 if(aa_method==1)AA_DO(((I(API*)(P,P))orig(o,107))(o,t->aa_shader));
 AA_DO(((I(API*)(P,U,P))orig(o,65))(o,0,t->aa_tex));
 AA_DO(((I(API*)(P,U,U,U))orig(o,67))(o,0,11,0));
 AA_DO(((I(API*)(P,U,U,U))orig(o,67))(o,0,24,0));
 {static const U rs[][2]={{7,0},{14,0},{15,0},{27,0},{28,0},{52,0},{137,0},{174,0},{194,0},{206,0},{152,0},{22,1},{8,3},{168,15},{162,0xffffffff},{128,0}};
 for(U i=0;i<sizeof(rs)/sizeof(rs[0]);i++)AA_DO(aa_rs(o,rs[i][0],rs[i][1]));}
 {static const U ss[][2]={{1,3},{2,3},{5,2},{6,2},{7,0},{11,0}};
 for(U i=0;i<sizeof(ss)/sizeof(ss[0]);i++)AA_DO(aa_ss(o,ss[i][0],ss[i][1]));}
 struct Vertex{float x,y,z,rhw,u,v;} quad[4]={{-0.5f,-0.5f,0,1,0,0},{(float)desc.w-0.5f,-0.5f,0,1,1,0},{-0.5f,(float)desc.h-0.5f,0,1,0,1},{(float)desc.w-0.5f,(float)desc.h-0.5f,0,1,1,1}};
 if(aa_method==2){
  float c[8]={1.0f/(float)desc.w,1.0f/(float)desc.h,(float)desc.w,(float)desc.h,(float)aa_strength/100.0f,0,0,0};
  AA_DO(((I(API*)(P,U,const float*,U))orig(o,109))(o,0,c,2));
  for(U sampler=0;sampler<3;sampler++){
   static const U ss[][2]={{1,3},{2,3},{5,2},{6,2},{7,0},{11,0},{8,0},{9,0},{10,1}};
   for(U i=0;i<sizeof(ss)/sizeof(ss[0]);i++)AA_DO(((I(API*)(P,U,U,U))orig(o,69))(o,sampler,ss[i][0],ss[i][1]));
  }
  for(U pass=0;pass<3;pass++){
   step=pass==0?"SMAA edge detection":pass==1?"SMAA blending weights":"SMAA neighborhood blend";
   /* Unbind previous inputs before making any of them a render target. */
   for(U slot=0;slot<3;slot++)AA_DO(((I(API*)(P,U,P))orig(o,65))(o,slot,0));
   AA_DO(((I(API*)(P,U,P))orig(o,37))(o,0,pass<2?t->sm_surface[pass]:back));
   {VP vp={0,0,desc.w,desc.h,0,1};AA_DO(((I(API*)(P,const VP*))orig(o,47))(o,&vp));}
   /* Edge detection discards non-edges: clear every frame to avoid stale data. */
   if(pass<2)AA_DO(((I(API*)(P,U,P,U,U,float,U))orig(o,43))(o,0,0,1,0,1.0f,0));
   AA_DO(((I(API*)(P,P))orig(o,107))(o,t->sm_shader[pass]));
   AA_DO(((I(API*)(P,U,P))orig(o,65))(o,0,pass==1?t->sm_tex[0]:t->aa_tex));
   if(pass==1){AA_DO(((I(API*)(P,U,P))orig(o,65))(o,1,t->sm_area));AA_DO(((I(API*)(P,U,P))orig(o,65))(o,2,t->sm_search));}
   if(pass==2)AA_DO(((I(API*)(P,U,P))orig(o,65))(o,1,t->sm_tex[1]));
   AA_DO(((I(API*)(P,U,U,const void*,U))orig(o,83))(o,5,2,quad,sizeof(quad[0])));
  }
 }else{
  float c[8]={1.0f/(float)desc.w,1.0f/(float)desc.h,(float)aa_relative/1000.0f,(float)aa_minimum/1000.0f,(float)aa_span,0.125f,0.0078125f,(float)aa_strength/100.0f};
  AA_DO(((I(API*)(P,U,const float*,U))orig(o,109))(o,0,c,2));step="FXAA draw";
  AA_DO(((I(API*)(P,U,U,const void*,U))orig(o,83))(o,5,2,quad,sizeof(quad[0])));
 }
 t->aa_passes++;if(t->aa_passes==1||t->aa_passes%600==0){begin(aa_method==2?"SMAA 1x pass":"AA FXAA-style pass");kv("width",desc.w);kv("height",desc.h);kv("passes",t->aa_passes);hx("result",hr);emit();}
 goto done;
failed:
 begin("AA disabled after failure");s(" step=");s(step);hx("result",hr);emit();t->aa_failed=1;
done:
 if(begun){I result=((I(API*)(P))orig(o,42))(o);if(result<0)restore=result;}
 if(modified){
  I result=((I(API*)(P,P))orig(o,39))(o,0);if(result<0)restore=result;
  for(U i=0;i<4;i++)if(i==0||saved[i]){result=((I(API*)(P,U,P))orig(o,37))(o,i,saved[i]);if(result<0)restore=result;}
  result=((I(API*)(P,P))orig(o,39))(o,depthbuf);if(result<0)restore=result;
  if(block){P*v=*(P**)block;result=((I(API*)(P))v[5])(block);if(result<0)restore=result;}
  result=((I(API*)(P,const VP*))orig(o,47))(o,&oldvp);if(result<0)restore=result;
 }
 if(restore<0){begin("AA state restoration failed");hx("result",restore);emit();t->aa_failed=1;}
 com_release(block);for(U i=0;i<4;i++)com_release(saved[i]);com_release(depthbuf);com_release(back);
 t->aa_busy=0;if(t->aa_failed)aa_cleanup(t);
#undef AA_DO
}
