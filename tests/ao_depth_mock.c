#define BLOOM_TEST_ENTRY bloom_tests_unused
#include "bloom_mock.c"
#undef BLOOM_TEST_ENTRY
static P depth_texture=objtable;
static U depth_format=0x5a544e49,depth_draws,depth_fail,create_calls,create_failure;
static I API ad_desc(P o,Desc*out){I hr=getdesc(o,out);if(o==&olddepth)out->format=depth_format;return hr;}
static I API ad_container(P o,const void*iid,P*out){(void)iid;*out=o==&olddepth?&depth_texture:&scene_texture;return 0;}
static I API ad_quad(P o,U type,U count,const void*data,U stride){
 (void)o;(void)data;depth_draws++;
 if(!in_scene||type!=5||count!=2||stride!=24||current_depth||current_rt!=&oldrt||input!=&depth_texture||mask_input||write_mask!=7||blend_enabled||params[0]!=100||actualvp.w!=3840||actualvp.h!=2160)fail=70;
 return depth_fail?-1:0;
}
static I API ad_create(P o,U w,U h,U levels,U usage,U fmt,U pool,P*out,P shared){
 (void)o;(void)w;(void)h;(void)levels;(void)usage;(void)pool;(void)shared;
 create_calls++;
 if(fmt==0x5a544e49){if(create_failure)return -1;*out=&depth_texture;return 0;}
 if(fmt!=75)fail=71;*out=&scene_texture;return 0;
}
void _start(void){
 bm_init();lookup(&dobj)->original[83]=ad_quad;lookup(&dobj)->original[23]=ad_create;
 sv[12]=ad_desc;sv[11]=ad_container;
 State*t=newstate(&dobj);t->rw=3840;t->rh=2160;t->lw=3840;t->lh=2160;t->main=1;
 ao_depth_view=1;shadow_image=(unsigned char*)1;
 P result=0;I hr=texture(&dobj,3840,2160,1,2,75,0,&result,0);
 if(hr||result!=&depth_texture||create_calls!=1)fail=72;
 create_failure=1;result=0;hr=texture(&dobj,3840,2160,1,2,75,0,&result,0);
 if(hr||result!=&scene_texture||create_calls!=3)fail=73;
 texture(&dobj,4096,4096,1,2,75,0,&result,0);if(create_calls!=4||result!=&scene_texture)fail=74;
 ao_depth_view=0;texture(&dobj,3840,2160,1,2,75,0,&result,0);if(create_calls!=5||result!=&scene_texture)fail=75;
 ao_depth_view=1;depth_format=75;ao_depth_retain(t);if(t->ao_depth)fail=76;
 depth_format=0x5a544e49;ao_depth_retain(t);if(t->ao_depth!=&olddepth)fail=77;
 t->ao_ps=&psobj;bloom_apply(t,&oldrt);
 if(depth_draws!=1||t->ao_passes!=1||t->bl_passes||alloc_no||current_rt!=&oldrt||current_depth!=&olddepth||input!=(P)123||write_mask!=15||t->bl_failed)fail=78;
 current_rt=&backobj;bloom_apply(t,&oldrt);
 if(depth_draws!=2||current_rt!=&backobj||current_depth!=&olddepth||t->bl_failed)fail=79;
 depth_fail=1;bloom_apply(t,&oldrt);
 if(!t->ao_failed||t->bl_failed||t->ao_depth!=&olddepth||t->ao_ps||current_rt!=&backobj||current_depth!=&olddepth)fail=80;
 t->ao_failed=0;bloom_frame_end(t);bloom_apply(t,&oldrt);if(depth_draws!=3||!t->ao_missing)fail=81;
 /* Depth preview uses scene recognition even when bloom is disabled. */
 depth_fail=0;current_rt=&oldrt;t->ao_ps=&psobj;bloom_enabled=0;
 bl_ids[0]=(BloomShader){&psobj,1};bl_ids[1]=(BloomShader){&byteobj,2};bl_id_count=2;
 game_vs=&psobj;bloom_before_draw(&dobj,0);
 if(t->bl_scene!=&oldrt||t->ao_depth!=&olddepth)fail=82;
 game_vs=&byteobj;bloom_before_draw(&dobj,0);
 if(depth_draws!=4||!t->bl_done||t->bl_failed)fail=83;
 bloom_frame_end(t);if(t->ao_depth||t->bl_scene||t->bl_done)fail=84;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
