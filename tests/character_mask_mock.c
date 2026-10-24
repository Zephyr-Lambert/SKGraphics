/* State/selection/failure tests; does not simulate rasterization or a GPU. */
#define BLOOM_TEST_ENTRY unused_bloom_entry
#include "bloom_mock.c"
static U cm_rs[256],cm_clears,cm_fail_clear,cm_created_role;
static P cm_ps_vt[5],cm_ps=cm_ps_vt,cm_bound_ps;
static I API cm_get_rs(P o,U key,U*out){(void)o;*out=cm_rs[key];return 0;}
static I API cm_set_rs(P o,U key,U v){(void)o;cm_rs[key]=v;return 0;}
static I API cm_get_ps(P o,P*out){(void)o;*out=cm_bound_ps;return 0;}
static I API cm_set_ps(P o,P p){(void)o;cm_bound_ps=p;return 0;}
static U API cm_ref(P o){(void)o;return 1;}
static I API cm_code(P o,P out,U*bytes){
 (void)o;static const U code[]={0xffff0300,0x02000001,0x800f0800,0xa0e40000,0xffff};
 if(out){if(*bytes<sizeof(code))return -1;for(U i=0;i<5;i++)((U*)out)[i]=code[i];}
 *bytes=sizeof(code);return 0;
}
static I API cm_shader(P o,const U*code,P*out){
 (void)o;cm_created_role=code[3]==0x3f800000?3:4;
 if(code[11]!=0x80070800)fail=110;
 *out=&psobj;return 0;
}
static I API cm_clear(P o,U count,P rect,U flags,U color,float z,U stencil){
 (void)o;(void)rect;(void)z;(void)stencil;cm_clears++;
 if(count||flags!=1||color||current_rt!=&sobj||current_depth!=&olddepth||actualvp.w!=3840||actualvp.h!=2160||cm_rs[174])fail=111;
 return cm_fail_clear?-1:0;
}
static void cm_verify_restored(void){
 if(current_rt!=&oldrt||current_depth!=&olddepth||cm_bound_ps!=&cm_ps||actualvp.w!=3840||actualvp.h!=2160)fail=112;
 if(cm_rs[14]!=1||cm_rs[23]!=4||cm_rs[27]!=1||cm_rs[168]!=15||cm_rs[194]!=1||cm_rs[59]!=255||cm_rs[174]!=1)fail=113;
}
void _start(void){
 bm_init();Table*v=lookup(&dobj);
 v->original[23]=mktexture;v->original[43]=cm_clear;v->original[57]=cm_set_rs;v->original[58]=cm_get_rs;
 v->original[106]=cm_shader;v->original[107]=cm_set_ps;v->original[108]=cm_get_ps;
 objtable[18]=getsurface;cm_ps_vt[1]=cm_ref;cm_ps_vt[2]=release_aa;cm_ps_vt[4]=cm_code;
 State*t=newstate(&dobj);t->main=1;t->rw=3840;t->rh=2160;t->bl_scene=&oldrt;t->ao_depth=&olddepth;
 cm_bound_ps=&cm_ps;game_vs=&byteobj;bl_ids[0]=(BloomShader){&byteobj,1,1};bl_id_count=1;
 cm_rs[7]=cm_rs[14]=cm_rs[27]=cm_rs[194]=cm_rs[174]=1;cm_rs[23]=4;cm_rs[168]=15;cm_rs[59]=255;
 ao_enabled=1;ao_exclude_characters=1;CharacterMaskState m;
 if(!cm_begin(&dobj,&m))fail=114;
 else{
  if(cm_created_role!=3||cm_rs[14]||cm_rs[23]!=3||cm_rs[27]||cm_rs[168]!=7||cm_rs[59]||cm_rs[194]||!cm_rs[174]||current_rt!=&sobj||!t->cm_ready)fail=115;
  cm_end(&dobj,&m,0);
 }
 cm_verify_restored();if(cm_clears!=1||allocated!=1)fail=116;
 /* A later scenery draw writes black, clearing any earlier actor coverage. */
 bl_ids[0].character=0;
 if(!cm_begin(&dobj,&m))fail=117;else{if(cm_created_role!=4)fail=118;cm_end(&dobj,&m,0);}
 cm_verify_restored();if(cm_clears!=1||allocated!=1)fail=119;
 /* Floating icons are not actors, even if an icon were to write depth. */
 bl_ids[0].character=2;if(!cm_begin(&dobj,&m))fail=120;else{if(cm_created_role!=4)fail=121;cm_end(&dobj,&m,0);}
 t->bl_done=1;if(cm_begin(&dobj,&m))fail=122;t->bl_done=0;
 cm_rs[14]=0;if(cm_begin(&dobj,&m))fail=123;cm_rs[14]=1;
 /* A failed replay still restores all state and prevents partial-mask use. */
 if(!cm_begin(&dobj,&m))fail=124;else cm_end(&dobj,&m,-1);
 cm_verify_restored();if(!t->cm_failed||cm_begin(&dobj,&m))fail=125;
 t->cm_failed=0;t->cm_ready=0;cm_fail_clear=1;
 if(cm_begin(&dobj,&m)||!t->cm_failed||t->cm_ready)fail=126;
 cm_verify_restored();character_cleanup(&dobj);if(t->cm_tex||t->cm_surf)fail=127;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
