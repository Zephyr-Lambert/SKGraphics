/* Experimental pre-UI LDR bloom, restricted to observed GOG 01.06 shaders.
 * Injects inside the existing scene. Never calls StretchRect or BeginScene.
 */
#include "bloom_source.h"
static P bl_bytecode[5];static U bl_compile_failed;
typedef struct {P object;U role,character;} BloomShader;
static BloomShader bl_ids[512];static U bl_id_count;
static U bloom_role(const char*hash){
 static const char fade_vs[]="96d63e16bc9eefac691744b252bd935495c97d58027f4656be52b09506657e04";
 U f=0;while(f<64&&hash[f]&&hash[f]==fade_vs[f])f++;if(f==64)return 4;
 static const char copy_vs[]="36a760bcff380249fc5de7524044ad7dfe8d0483ba82df0e27c06bf0809925f5";
 U k=0;while(k<64&&hash[k]&&hash[k]==copy_vs[k])k++;if(k==64)return 3;
 static const char*ids[]={
 "15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7",
 "a71e412f97d8faffbf6cea5a0782b0a8428c03753132d208516ec27e2c076394",
 "ce7435051f0887ea339b5478ec68952257b3c8812bb714648859f07c8a78d004",
 "37ddb1d1007174af6cee0b44c6e2d00179f767d7daf731cd68152b25c3de58f2",
 "887e6b2524463b1f1a0110fa587ec2b0b4a98c25d6bbff5aa767ee1073f047c4",
 "e12a27770f809981713b650c3df251c060562e750addd859807e65ec7f06fec0",
 "36c01153a95b7492c259e56f352607d7ded0002ee58f4f1880926d4cdaafc4b3",
 "f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1",
 "32b581c0942e5d140a997a4827665e3897967c04be2e26611af3fb077ef7d5f7",
 "527fcbce9cb0d09a9fe5566a01b770bd986ed9a6b475b3eed9bc4a967a2028e6"};
 for(U i=0;i<10;i++){U j=0;while(j<64&&hash[j]&&hash[j]==ids[i][j])j++;if(j==64)return i<8?1:2;}return 0;
}
static U character_role(const char*hash){
 static const char*ids[]={
 "15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7",
 "a71e412f97d8faffbf6cea5a0782b0a8428c03753132d208516ec27e2c076394",
 "f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1",
 "411df34c3be010b04f9b5904a010c66e7fb3a0154175f60ccf7285063f88b4b6"};
 for(U i=0;i<4;i++)if(sh_equal((const unsigned char*)hash,(const unsigned char*)ids[i],64))return i==3?2:1;
 return 0;
}
static I API bloom_vertex_shader(P o,const U*code,P*out){
 I hr=outline_vertex_shader(o,code,out);
 if(hr<0||!out||!*out||(!bloom_enabled&&!ao_depth_view&&!ao_enabled&&!character_preview))return hr;
 /* Track ALL creations, including role zero, so reused object addresses
  * cannot inherit the classification of an earlier released shader. */
 U i=0;while(i<bl_id_count&&bl_ids[i].object!=*out)i++;
 if(i==512){begin("Bloom shader table full; disabling bloom conservatively");emit();bloom_enabled=0;ao_depth_view=0;ao_enabled=0;character_preview=0;return hr;}
 if(i==bl_id_count)bl_id_count++;
 char hash[65];scene_shader(*out,hash);bl_ids[i].object=*out;bl_ids[i].role=bloom_role(hash);bl_ids[i].character=character_role(hash);
 return hr;
}
static void bloom_frame_end(State*t){if(!t)return;t->cm_ready=0;com_release(t->ao_depth);t->ao_depth=0;t->ao_valid=0;com_release(t->bl_scene);t->bl_scene=0;t->bl_done=0;t->bl_early_ui=0;t->bl_mask=0;}
static void bloom_cleanup(State*t){
 if(!t)return;character_cleanup(t->obj);bloom_frame_end(t);ao_resources_release(t);com_release(t->ao_ps);t->ao_ps=0;
 for(U i=0;i<2;i++){com_release(t->bl_surf[i]);com_release(t->bl_tex[i]);t->bl_surf[i]=t->bl_tex[i]=0;}
 for(U i=0;i<5;i++){com_release(t->bl_ps[i]);t->bl_ps[i]=0;}t->bl_w=t->bl_h=0;com_release(t->bl_bgsurf);com_release(t->bl_bgtex);t->bl_bgsurf=t->bl_bgtex=0;t->bl_bgw=t->bl_bgh=0;
}
static I bloom_compile(void){
 if(bl_bytecode[0]&&bl_bytecode[1]&&bl_bytecode[2]&&bl_bytecode[3]&&bl_bytecode[4])return 1;if(bl_compile_failed)return 0;

 static const char*entries[]={"extract","blur","composite","copyScene","extractChanged"};
 P mod=d3dx_library();P fn=mod?GetProcAddress(mod,"D3DXCompileShader"):0;
 if(!fn){begin("Bloom compiler unavailable");emit();bl_compile_failed=1;return 0;}
 for(U i=0;i<5;i++){
  P err=0;I hr=((I(API*)(const char*,U,P,P,const char*,const char*,U,P*,P*,P*))fn)(bloom_source,sizeof(bloom_source)-1,0,0,entries[i],"ps_3_0",0,&bl_bytecode[i],&err,0);
  begin("Bloom shader compilation");kv("pass",i);hx("result",hr);emit();
  if(hr<0||!bl_bytecode[i]){bl_compile_failed=1;if(err){P*v=*(P**)err;char*msg=((P(API*)(P))v[3])(err);begin("Bloom compiler message: ");if(msg)s(msg);emit();}}
  com_release(err);if(bl_compile_failed){for(U j=0;j<5;j++){com_release(bl_bytecode[j]);bl_bytecode[j]=0;}return 0;}
 }return 1;
}
static I bloom_resources(State*t,U w,U h){
 if(t->bl_w==w&&t->bl_h==h&&t->bl_tex[0]&&t->bl_tex[1])return 1;
 /* Do not clear this frame's scene marker while replacing GPU resources. */
 for(U i=0;i<2;i++){com_release(t->bl_surf[i]);com_release(t->bl_tex[i]);t->bl_surf[i]=t->bl_tex[i]=0;}
 for(U i=0;i<5;i++){com_release(t->bl_ps[i]);t->bl_ps[i]=0;}t->bl_w=t->bl_h=0;
 if(!bloom_compile())return 0;
 for(U i=0;i<5;i++){P*v=*(P**)bl_bytecode[i];P code=((P(API*)(P))v[3])(bl_bytecode[i]);if(((I(API*)(P,const U*,P*))orig(t->obj,106))(t->obj,code,&t->bl_ps[i])<0||!t->bl_ps[i])return 0;}
 for(U i=0;i<2;i++)if(!aa_target(t->obj,w,h,21,&t->bl_tex[i],&t->bl_surf[i]))return 0;
 t->bl_w=w;t->bl_h=h;begin("Bloom resources");kv("width",w);kv("height",h);emit();return 1;
}
static void bloom_run(State*t,P target,U capture){
 P o=t->obj,source=0,block=0,saved[4]={0},depthbuf=0;
 Desc desc={0};VP oldvp={0};I hr=0,modified=0,restore=0;const char*step="source";
 P*v=*(P**)target;
 if(((I(API*)(P,Desc*))v[12])(target,&desc)<0||desc.ms||!desc.w||!desc.h)return;
 if(capture>=2){if(!t->ao_depth)goto done;v=*(P**)t->ao_depth;}
 hr=((I(API*)(P,const void*,P*))v[11])(capture>=2?t->ao_depth:target,probe_texture_iid,&source);if(hr<0||!source)goto done;
 hr=((I(API*)(P,VP*))orig(o,48))(o,&oldvp);if(hr<0)goto failed;
 /* Partial viewports and additional MRT outputs are not verified paths. */
 if(oldvp.x||oldvp.y||oldvp.w!=desc.w||oldvp.h!=desc.h)goto done;
 for(U i=0;i<4;i++)((I(API*)(P,U,P*))orig(o,38))(o,i,&saved[i]);
 if(!saved[0]||saved[1]||saved[2]||saved[3])goto done;
 ((I(API*)(P,P*))orig(o,40))(o,&depthbuf);
 step="resources";if(!(capture==3?ao_resources(t,(desc.w+1)/2,(desc.h+1)/2):capture==2?ao_depth_shader(t):bloom_resources(t,(desc.w+3)/4,(desc.h+3)/4))){hr=(I)0x80004005;goto failed;}
 if(capture==1&&(!t->bl_bgtex||t->bl_bgw!=desc.w||t->bl_bgh!=desc.h)){
  com_release(t->bl_bgsurf);com_release(t->bl_bgtex);t->bl_bgsurf=t->bl_bgtex=0;
  step="background resources";if(!aa_target(o,desc.w,desc.h,21,&t->bl_bgtex,&t->bl_bgsurf)){hr=(I)0x80004005;goto failed;}
  t->bl_bgw=desc.w;t->bl_bgh=desc.h;
 }
 step="state capture";hr=((I(API*)(P,U,P*))orig(o,59))(o,1,&block);if(hr<0||!block)goto failed;
 modified=1;
#define BL_DO(expr) do {hr=(expr);if(hr<0)goto failed;} while(0)
 step="setup";
 BL_DO(((I(API*)(P,P))orig(o,39))(o,0));
 for(U slot=0;slot<16;slot++)BL_DO(((I(API*)(P,U,P))orig(o,65))(o,slot,0));
 for(U slot=257;slot<261;slot++)BL_DO(((I(API*)(P,U,P))orig(o,65))(o,slot,0));
 BL_DO(((I(API*)(P,P))orig(o,92))(o,0));BL_DO(((I(API*)(P,U))orig(o,89))(o,0x104));
 BL_DO(((I(API*)(P,U,U))orig(o,102))(o,0,1));
 BL_DO(((I(API*)(P,U,U,U))orig(o,67))(o,0,11,0));BL_DO(((I(API*)(P,U,U,U))orig(o,67))(o,0,24,0));
 {static const U rs[][2]={{7,0},{14,0},{15,0},{27,0},{28,0},{52,0},{137,0},{174,0},{194,0},{206,0},{152,0},{22,1},{8,3},{168,15},{162,0xffffffff},{128,0}};
 for(U i=0;i<sizeof(rs)/sizeof(rs[0]);i++)BL_DO(aa_rs(o,rs[i][0],rs[i][1]));}
 {static const U ss[][2]={{1,3},{2,3},{5,2},{6,2},{7,0},{11,0},{8,0},{9,0},{10,1}};
 for(U sampler=0;sampler<3;sampler++)for(U i=0;i<sizeof(ss)/sizeof(ss[0]);i++)BL_DO(((I(API*)(P,U,U,U))orig(o,69))(o,sampler,ss[i][0],ss[i][1]));}
 for(U pass=0;pass<(capture==3?2u:capture?1u:4u);pass++){
  step=pass==0?"extract":pass==1?"horizontal blur":pass==2?"vertical blur":"composite";
  U w=capture==3?(pass?desc.w:t->ao_w):(capture||pass==3)?desc.w:t->bl_w,h=capture==3?(pass?desc.h:t->ao_h):(capture||pass==3)?desc.h:t->bl_h;
  P dst=capture==3?(pass?target:t->ao_surf):capture==2?target:capture==1?t->bl_bgsurf:pass==3?target:t->bl_surf[pass==1?1:0];
  P src=(capture==3||pass==0)?source:t->bl_tex[pass==2?1:0];
  BL_DO(((I(API*)(P,U,P))orig(o,65))(o,0,0));
  BL_DO(((I(API*)(P,U,P))orig(o,37))(o,0,dst));
  VP vp={0,0,w,h,0,1};BL_DO(((I(API*)(P,const VP*))orig(o,47))(o,&vp));
  BL_DO(((I(API*)(P,U,P))orig(o,65))(o,0,src));
  BL_DO(((I(API*)(P,P))orig(o,107))(o,(capture==3?t->ao_shaders[pass]:capture==2?t->ao_ps:t->bl_ps[capture==1?3:pass==0?(t->bl_mask?4:0):pass==3?2:1])));
  BL_DO(((I(API*)(P,U,P))orig(o,65))(o,1,capture==3&&pass==1?t->ao_tex:(!capture&&pass==0&&t->bl_mask)?t->bl_bgtex:0));
  float c[4]={0,0,0,0},radius=(float)bloom_radius/100.0f*(float)desc.h/1080.0f;
  if(pass==0){c[0]=0.25f/(float)w;c[1]=0.25f/(float)h;c[2]=(float)bloom_threshold/1000.0f;c[3]=(float)bloom_knee/1000.0f;}
  if(capture==2){c[0]=(float)ao_depth_scale;BL_DO(aa_rs(o,168,7));for(U key=5;key<=6;key++)BL_DO(((I(API*)(P,U,U,U))orig(o,69))(o,0,key,1));}
  if(pass==1)c[0]=radius/(float)w;
  if(pass==2)c[1]=radius/(float)h;
  if(pass==3){c[0]=(float)bloom_strength/100.0f;BL_DO(aa_rs(o,27,1));BL_DO(aa_rs(o,19,2));BL_DO(aa_rs(o,20,2));BL_DO(aa_rs(o,171,1));BL_DO(aa_rs(o,168,7));}
  if(capture==3){
   float ac[16];for(U i=0;i<8;i++)ac[i]=t->ao_proj[i];
   ac[8]=(float)ao_radius;ac[9]=(float)ao_strength/100.0f;ac[10]=(float)ao_bias/100.0f;ac[11]=(float)(ao_quality==1?8:ao_quality==2?16:32);
   ac[12]=1.0f/(float)t->ao_w;ac[13]=1.0f/(float)t->ao_h;ac[14]=(float)ao_max_distance;ac[15]=(ao_exclude_characters&&t->cm_ready&&!t->cm_failed)?1.0f:0;
   BL_DO(((I(API*)(P,U,P))orig(o,65))(o,2,pass==1&&ac[15]>0?t->cm_tex:0));
   for(U sampler=0;sampler<3;sampler++)for(U key=5;key<=6;key++)BL_DO(((I(API*)(P,U,U,U))orig(o,69))(o,sampler,key,1));
   if(pass==1){BL_DO(aa_rs(o,168,7));BL_DO(aa_rs(o,27,1));BL_DO(aa_rs(o,19,1));BL_DO(aa_rs(o,20,3));BL_DO(aa_rs(o,171,1));}
   BL_DO(((I(API*)(P,U,const float*,U))orig(o,109))(o,0,ac,4));
  }else BL_DO(((I(API*)(P,U,const float*,U))orig(o,109))(o,0,c,1));
  struct Vertex{float x,y,z,rhw,u,v;} quad[4]={{-.5f,-.5f,0,1,0,0},{(float)w-.5f,-.5f,0,1,1,0},{-.5f,(float)h-.5f,0,1,0,1},{(float)w-.5f,(float)h-.5f,0,1,1,1}};
  BL_DO(((I(API*)(P,U,U,const void*,U))orig(o,83))(o,5,2,quad,sizeof(quad[0])));
 }
 if(capture==3){t->ao_passes++;if(t->ao_passes==1||t->ao_passes%600==0){begin("AO pre-UI pass");kv("passes",t->ao_passes);kv("radius",ao_radius);kv("strength",ao_strength);kv("quality",ao_quality);emit();}goto done;}
 if(capture==2){t->ao_passes++;if(t->ao_passes==1||t->ao_passes%600==0){begin("AO depth preview pass");kv("passes",t->ao_passes);kv("scale",ao_depth_scale);emit();}goto done;}
 if(capture==1){t->bl_mask=1;goto done;}
 t->bl_passes++;if(t->bl_passes==1||t->bl_passes%600==0){begin("Bloom pre-UI pass");kv("width",desc.w);kv("height",desc.h);kv("strength",bloom_strength);kv("threshold",bloom_threshold);kv("knee",bloom_knee);kv("radius",bloom_radius);kv("passes",t->bl_passes);kv("backgroundMask",t->bl_mask);emit();}
 goto done;
failed:
 begin(capture>=2?"AO pass disabled after failure":"Bloom disabled after failure");s(" step=");s(step);hx("result",hr);emit();if(capture>=2)t->ao_failed=1;else t->bl_failed=1;
done:
 if(modified){
  I result=((I(API*)(P,U,P))orig(o,65))(o,0,0);if(result<0)restore=result;
  result=((I(API*)(P,U,P))orig(o,65))(o,1,0);if(result<0)restore=result;
  result=((I(API*)(P,U,P))orig(o,65))(o,2,0);if(result<0)restore=result;
  result=((I(API*)(P,P))orig(o,39))(o,0);if(result<0)restore=result;
  result=((I(API*)(P,U,P))orig(o,37))(o,0,saved[0]);if(result<0)restore=result;
  result=((I(API*)(P,P))orig(o,39))(o,depthbuf);if(result<0)restore=result;
  if(block){P*b=*(P**)block;result=((I(API*)(P))b[5])(block);if(result<0)restore=result;}
  result=((I(API*)(P,const VP*))orig(o,47))(o,&oldvp);if(result<0)restore=result;
 }
 if(restore<0){begin("Bloom state restoration failed");hx("result",restore);emit();t->bl_failed=1;}
 com_release(block);com_release(source);com_release(depthbuf);for(U i=0;i<4;i++)com_release(saved[i]);
 if(t->bl_failed)bloom_cleanup(t);
 else if(t->ao_failed){ao_resources_release(t);com_release(t->ao_ps);t->ao_ps=0;}
#undef BL_DO
}
static void bloom_apply(State*t,P target){
 if((ao_depth_view||ao_enabled)&&!t->ao_failed){
  if(t->ao_depth){if(ao_depth_view)bloom_run(t,target,2);else if(t->ao_valid&&ao_strength&&(!ao_exclude_characters||(t->cm_ready&&!t->cm_failed)))bloom_run(t,target,3);}
  else if(!t->ao_missing){begin("AO skipped: no matching INTZ scene depth");emit();t->ao_missing=1;}
 }
 if(!ao_depth_view&&!t->bl_failed&&bloom_enabled&&bloom_strength)bloom_run(t,target,0);
}
static void bloom_before_draw(P o,P caller){
 State*t=state(o);if((!ao_depth_view&&!ao_enabled&&(!bloom_enabled||!bloom_strength))||!shadow_image||!t||!t->main||t->bl_done||t->bl_failed)return;
 P vs=0;U role=0;if(((I(API*)(P,P*))orig(o,93))(o,&vs)>=0&&vs){for(U i=0;i<bl_id_count;i++)if(bl_ids[i].object==vs){role=bl_ids[i].role;break;}}com_release(vs);
 if(!role)return;
 P target=0;if(((I(API*)(P,U,P*))orig(o,38))(o,0,&target)<0||!target){com_release(target);return;}
 if(role==1&&!t->bl_scene){if(t->bl_early_ui&&!ao_depth_view&&bloom_enabled&&bloom_strength)bloom_run(t,target,1);if(!t->bl_failed){if(ao_depth_view||ao_enabled)ao_depth_retain(t);if(ao_enabled&&!ao_depth_view)ao_camera(t);t->bl_scene=target;return;}}
 if(role==2){if(t->bl_scene){t->bl_done=1;if(target==t->bl_scene)bloom_apply(t,target);}else t->bl_early_ui=1;}
 if(role==4&&target==t->bl_scene&&(unsigned long)caller-(unsigned long)shadow_image==game_build->fade_rva){
  P ps=0;char hash[65];((I(API*)(P,P*))orig(o,108))(o,&ps);scene_shader(ps,hash);com_release(ps);
  static const char fade_ps[]="06f603a6391741e8fd3813d835e1b65be8880258f34625561eaad339ae0b8fb8";
  U k=0;while(k<64&&hash[k]&&hash[k]==fade_ps[k])k++;if(k==64){t->bl_done=1;bloom_apply(t,target);}
 }
 /* Exact simple-quad path observed after 3D effects in all three animation
  * captures. Process the scene BEFORE a verified full-size copy so later
  * compositing cannot restore an unprocessed snapshot over our result. */
 U caller_rva=(U)((unsigned long)caller-(unsigned long)shadow_image);
 if(role==3&&t->bl_scene&&(caller_rva==game_build->copy_rva||caller_rva==game_build->scene_copy_rva)){
  P ps=0;char hash[65];((I(API*)(P,P*))orig(o,108))(o,&ps);scene_shader(ps,hash);com_release(ps);
  static const char copy_ps[]="5f86420a561c69d2dd4f76d37b8edc1e21c811d277cb365959f23d47d16cb377";
  U k=0;while(k<64&&hash[k]&&hash[k]==copy_ps[k])k++;
  if(k==64){
   I boundary=caller_rva==game_build->copy_rva&&target==t->bl_scene;
   if(!boundary&&target!=t->bl_scene){
    P back=0,tex=0,source=0;P*v=*(P**)t->bl_scene;
    ((I(API*)(P,U,U,U,P*))orig(o,18))(o,0,0,0,&back);
    ((I(API*)(P,U,P*))orig(o,64))(o,0,&tex);
    ((I(API*)(P,const void*,P*))v[11])(t->bl_scene,probe_texture_iid,&source);
    boundary=source&&tex==source&&((caller_rva==game_build->copy_rva&&back&&target==back)||(caller_rva==game_build->scene_copy_rva&&main_surface(t,target)));
    com_release(back);com_release(tex);com_release(source);
   }
   if(boundary){t->bl_done=1;if((t->bl_passes+t->ao_passes)==0||frames%600==0){begin("Bloom HUD-free boundary");kv("finalCopy",caller_rva==game_build->copy_rva&&target!=t->bl_scene);kv("sceneCopy",caller_rva==game_build->scene_copy_rva);emit();}bloom_apply(t,t->bl_scene);}
  }
 }
 com_release(target);
}
