#define BLOOM_TEST_ENTRY bloom_tests_unused
#include "bloom_mock.c"
static P adepthtex=objtable;
static P receiver_mask_input;
static I API am_tex(P o,U slot,P p){if(slot==2)receiver_mask_input=p;return bm_tex(o,slot,p);}
static U ao_draw_count,ao_fail_draw;static float ac[16];
static I API am_desc(P o,Desc*out){I hr=getdesc(o,out);if(o==&olddepth)out->format=0x5a544e49;return hr;}
static I API am_container(P o,const void*iid,P*out){(void)iid;*out=o==&olddepth?&adepthtex:&scene_texture;return 0;}
static I API am_constants(P o,U start,const float*c,U count){(void)o;if(start||count!=4)fail=90;for(U i=0;i<16;i++)ac[i]=c[i];return 0;}
static I API am_quad(P o,U type,U count,const void*data,U stride){
 (void)o;(void)data;U p=ao_draw_count++;
 if(ao_exclude_characters&&p%2==1&&(receiver_mask_input!=&byteobj||ac[15]!=1))fail=107;
 if(!in_scene||type!=5||count!=2||stride!=24||current_depth||input!=&adepthtex)fail=91;
 if(p%2==0){if(current_rt!=&bsurf[0]||mask_input||blend_enabled||actualvp.w!=1920||actualvp.h!=1080)fail=92;}
 else if(current_rt!=&oldrt||mask_input!=&btex[0]||!blend_enabled||srcblend!=1||dstblend!=3||blendop!=1||write_mask!=7||actualvp.w!=3840||actualvp.h!=2160)fail=93;
 if(ac[8]!=30||ac[9]!=1||ac[10]!=.1f||ac[11]!=16)fail=94;
 return ao_fail_draw==p+1?-1:0;
}
static void am_cached(State*t){btex[0]=bt;bsurf[0]=bs;t->ao_tex=&btex[0];t->ao_surf=&bsurf[0];t->ao_shaders[0]=t->ao_shaders[1]=&psobj;t->ao_w=1920;t->ao_h=1080;}
void _start(void){
 bm_init();lookup(&dobj)->original[83]=am_quad;lookup(&dobj)->original[109]=am_constants;
 sv[12]=am_desc;sv[11]=am_container;
 State*t=newstate(&dobj);t->rw=3840;t->rh=2160;t->main=1;
 float m[16]={1.3f,0,0,0, 0,2.4f,0,0, 0,0,1.0001f,1, 0,0,-1.0001f,0};
 if(!ao_projection(m,t->ao_proj)||t->ao_proj[2]<.769f||t->ao_proj[2]>.770f||t->ao_proj[3]<.416f||t->ao_proj[3]>.417f)fail=95;
 float saved=m[11];m[11]=0;if(ao_projection(m,t->ao_proj))fail=96;m[11]=saved;
 m[1]=1;if(ao_projection(m,t->ao_proj))fail=97;m[1]=0;
 /* Real captured WVPs: hub, stage and rotated dressing-room camera.
  * Check recovery of approximately unit near plane and projection scales,
  * then a depth->view distance round trip independent of camera rotation. */
 const float captures[3][16]={
 {1.30367517f,-.000000100559f,-.000000138851f,-.000000138843f,0,2.21990108f,-.27587074f,-.27585390f,-.000000196849f,-.66597021f,-.91956937f,-.91951323f,-.00000518034f,-300.61169f,558.40454f,559.37048f},
 {1.15369523f,.10593849f,.14769606f,.14769459f,0,1.96272576f,-.32649797f,-.32649469f,.18251568f,-.66964513f,-.93359792f,-.93358856f,167.55031f,-89.09289f,536.57483f,537.56946f},
 {0,-7.29192352f,0,0,4.10170698f,0,0,0,0,0,-.96005857f,-.95999998f,-350.76932f,0,349.02130f,350}
 };
 for(U j=0;j<3;j++){
  float out[8];if(!ao_projection(captures[j],out))fail=102;
  float near=-out[1]/out[0];if(near<.99f||near>1.01f)fail=103;
  float depth=out[0]+out[1]/100.0f,z=out[1]/(depth-out[0]);if(z<99.9f||z>100.1f)fail=104;
 }
 ao_projection(m,t->ao_proj);t->ao_proj[6]=1.0f/3840;t->ao_proj[7]=1.0f/2160;
 ao_enabled=1;bloom_enabled=0;t->ao_valid=1;ao_depth_retain(t);am_cached(t);
 bloom_apply(t,&oldrt);
 if(t->bl_failed||ao_draw_count!=2||t->ao_passes!=1||t->bl_passes||current_rt!=&oldrt||current_depth!=&olddepth||input!=(P)123||write_mask!=15||alloc_no)fail=98;
 current_rt=&backobj;bloom_apply(t,&oldrt);if(ao_draw_count!=4||current_rt!=&backobj||current_depth!=&olddepth)fail=99;
 /* Exclusion refuses an absent/failed mask; valid mask reaches sampler 2. */
 lookup(&dobj)->original[65]=am_tex;ao_exclude_characters=1;
 bloom_apply(t,&oldrt);if(ao_draw_count!=4)fail=108;
 t->cm_ready=1;t->cm_tex=&byteobj;bloom_apply(t,&oldrt);
 if(ao_draw_count!=6||receiver_mask_input)fail=109;
 t->cm_failed=1;bloom_apply(t,&oldrt);if(ao_draw_count!=6)fail=110;
 t->cm_failed=0;t->cm_ready=0;t->cm_tex=0;ao_exclude_characters=0;ao_draw_count=4;
 t->ao_valid=0;bloom_apply(t,&oldrt);if(ao_draw_count!=4)fail=100;t->ao_valid=1;
 for(U f=1;f<=2;f++){
  ao_draw_count=0;ao_fail_draw=f;t->bl_failed=0;t->ao_failed=0;bloom_frame_end(t);t->ao_valid=1;ao_depth_retain(t);am_cached(t);
  bloom_apply(t,&oldrt);
  if(!t->ao_failed||t->bl_failed||ao_draw_count!=f||t->ao_tex||t->ao_surf||t->ao_shaders[0]||t->ao_depth!=&olddepth||current_rt!=&backobj||current_depth!=&olddepth||input!=(P)123)fail=101;
 }
 /* Missing AO compiler simulates a resource/compile failure before drawing.
  * Bloom must still execute all four passes now and on subsequent frames. */
 lookup(&dobj)->original[83]=bm_quad;lookup(&dobj)->original[109]=bm_constants;
 t->ao_failed=0;t->ao_valid=1;bloom_enabled=1;pass_no=0;alloc_no=0;
 bloom_apply(t,&oldrt);
 if(!t->ao_failed||t->bl_failed||pass_no!=4||t->bl_passes!=1||current_rt!=&backobj||current_depth!=&olddepth)fail=105;
 pass_no=0;bloom_apply(t,&oldrt);
 if(t->bl_failed||pass_no!=4||t->bl_passes!=2||alloc_no!=2)fail=106;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
