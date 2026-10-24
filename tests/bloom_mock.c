#define AA_TEST_ENTRY aa_tests_unused
#include "aa_mock.c"
static P bt[19],bs[13],btex[3],bsurf[3],scene_texture=objtable;
static P input,game_vs;
static U write_mask=15,blend_enabled,srcblend,dstblend,blendop,pass_no,fail_pass,fail_alloc,alloc_no;
static float params[4];static U captured;static P mask_input;
static I API bm_create(P o,U w,U h,U levels,U usage,U fmt,U pool,P*out,P shared){
 (void)o;(void)shared;U i=alloc_no++;
 if(w!=(i==2?3840u:960u)||h!=(i==2?2160u:540u)||levels!=1||usage!=1||fmt!=21||pool||i>2)fail=40;
 if(fail_alloc&&i==1)return -1;
 btex[i]=bt;bsurf[i]=bs;*out=&btex[i];return 0;
}
static I API bm_surface(P o,U level,P*out){if(level)fail=41;*out=o==&btex[0]?&bsurf[0]:o==&btex[1]?&bsurf[1]:&bsurf[2];return 0;}
static I API bm_container(P o,const void*iid,P*out){(void)o;(void)iid;*out=&scene_texture;return 0;}
static I API bm_tex(P o,U slot,P p){(void)o;if(!slot)input=p;if(slot==1)mask_input=p;return 0;}
static I API bm_rs(P o,U key,U val){(void)o;if(key==168)write_mask=val;if(key==27)blend_enabled=val;if(key==19)srcblend=val;if(key==20)dstblend=val;if(key==171)blendop=val;return 0;}
static I API bm_constants(P o,U start,const float*c,U count){(void)o;if(start||count!=1)fail=42;for(U i=0;i<4;i++)params[i]=c[i];return 0;}
static I API bm_apply(P o){(void)o;applied++;input=(P)123;write_mask=15;blend_enabled=0;return 0;}
static I API bm_vs(P o,P*out){(void)o;*out=game_vs;return 0;}
static I API bm_quad(P o,U type,U count,const void*data,U stride){
 (void)o;(void)data;if(current_rt==&bsurf[2]){if(input!=&scene_texture||mask_input||actualvp.w!=3840||actualvp.h!=2160||blend_enabled)fail=61;captured++;return 0;}U p=pass_no++;
 if(!in_scene||type!=5||count!=2||stride!=24||current_depth)fail=43;
 P want_rt=p==3?&oldrt:&bsurf[p==1?1:0];P want_in=p==0?&scene_texture:&btex[p==2?1:0];
 if(current_rt!=want_rt||input!=want_in||actualvp.w!=(p==3?3840u:960u)||actualvp.h!=(p==3?2160u:540u))fail=44;
 if(p<3&&(blend_enabled||write_mask!=15))fail=45;
 if(captured&&p==0&&mask_input!=&btex[2])fail=65;
 if(p==3&&(!blend_enabled||write_mask!=7||srcblend!=2||dstblend!=2||blendop!=1))fail=46;
 if(p==0&&(params[2]!=.75f||params[3]!=.1f))fail=47;
 if(p==1&&(params[0]<=0||params[1]))fail=48;
 if(p==2&&(params[1]<=0||params[0]))fail=49;
 if(p==3&&params[0]!=.35f)fail=50;
 return fail_pass==p+1?-1:0;
}
static void bm_init(void){
 bloom_enabled=1;
 bt[2]=release_aa;bt[18]=bm_surface;bs[2]=release_aa;
 objtable[2]=release_aa;objtable[3]=bytes;objtable[5]=bm_apply;
 sv[2]=release_aa;sv[11]=bm_container;sv[12]=getdesc;
 dv[23]=bm_create;dv[37]=writert;dv[38]=readrt;dv[39]=writedepth;dv[40]=readdepth;
 dv[47]=writevp;dv[48]=readvp;dv[57]=bm_rs;dv[59]=stateblock;
 dv[65]=bm_tex;dv[67]=threeuint;dv[69]=threeuint;dv[83]=bm_quad;
 dv[89]=oneuint;dv[92]=oneptr;dv[93]=bm_vs;dv[102]=twouint;dv[106]=mkshader;dv[107]=oneptr;dv[109]=bm_constants;
 record(&dobj,119);for(U i=0;i<5;i++)bl_bytecode[i]=&byteobj;
 current_rt=&oldrt;current_depth=&olddepth;actualvp=(VP){0,0,3840,2160,0,1};in_scene=1;
}
#ifndef BLOOM_TEST_ENTRY
#define BLOOM_TEST_ENTRY _start
#endif
void BLOOM_TEST_ENTRY(void){
 bm_init();State*t=newstate(&dobj);t->rw=3840;t->rh=2160;t->main=1;
 bloom_apply(t,&oldrt);
 if(t->bl_failed||pass_no!=4||t->bl_passes!=1||applied!=1||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=3840||actualvp.h!=2160||input!=(P)123||write_mask!=15||!in_scene)fail=51;
 pass_no=0;bloom_apply(t,&oldrt);if(alloc_no!=2||pass_no!=4||t->bl_passes!=2)fail=52;
 /* HUD-free final copy: scene source is no longer the active RT. Bloom must
  * write the retained scene, then restore the game's backbuffer binding. */
 pass_no=0;current_rt=&backobj;bloom_apply(t,&oldrt);
 if(t->bl_failed||pass_no!=4||current_rt!=&backobj||current_depth!=&olddepth||input!=(P)123||actualvp.w!=3840)fail=60;
 current_rt=&oldrt;
 /* Each injected draw can fail; game state and cached resources must recover. */
 for(U f=1;f<=4;f++){
  pass_no=0;fail_pass=f;t->bl_failed=0;bloom_apply(t,&oldrt);
  if(!t->bl_failed||t->bl_tex[0]||t->bl_surf[0]||t->bl_ps[0]||pass_no!=f||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=3840||input!=(P)123||!in_scene)fail=53;
  alloc_no=0;
 }
 fail_pass=0;t->bl_failed=0;fail_alloc=1;bloom_apply(t,&oldrt);
 if(!t->bl_failed||t->bl_tex[0]||t->bl_tex[1]||t->bl_ps[2]||current_rt!=&oldrt)fail=54;
 fail_alloc=0;t->bl_failed=0;alloc_no=0;pass_no=0;
 /* UI-first / paused frames skip. Confirm one trigger per scene frame. */
 bl_ids[0]=(BloomShader){&psobj,1};bl_ids[1]=(BloomShader){&byteobj,2};bl_id_count=2;
 shadow_image=(unsigned char*)1;bloom_enabled=1;game_vs=&byteobj;
 bloom_before_draw(&dobj,0);if(pass_no||t->bl_done||!t->bl_early_ui)fail=55;
 bloom_frame_end(t);game_vs=&psobj;bloom_before_draw(&dobj,0);if(t->bl_scene!=&oldrt||pass_no)fail=56;
 game_vs=&byteobj;bloom_before_draw(&dobj,0);if(pass_no!=4||!t->bl_done||t->bl_failed)fail=57;
 bloom_before_draw(&dobj,0);if(pass_no!=4)fail=58;
 bloom_frame_end(t);if(t->bl_done||t->bl_scene)fail=59;
 bloom_cleanup(t);alloc_no=0;pass_no=0;
 game_vs=&byteobj;bloom_before_draw(&dobj,0);
 game_vs=&psobj;bloom_before_draw(&dobj,0);
 if(captured!=1||!t->bl_mask||pass_no||t->bl_failed||current_rt!=&oldrt||input!=(P)123)fail=62;
 game_vs=&byteobj;bloom_before_draw(&dobj,0);
 if(pass_no!=4||t->bl_failed||!t->bl_done)fail=63;
 bloom_cleanup(t);if(t->bl_bgtex||t->bl_bgsurf||t->bl_mask||t->bl_early_ui)fail=64;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
