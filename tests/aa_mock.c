/* Native mock checks: AA pass order, state restoration, caching and failures.
 * No actual HLSL compiler or GPU is exercised here. */
#define _start resolution_test_entry
#include "mock.c"
#undef _start
static P objtable[19],texobj=objtable,psobj=objtable,byteobj=objtable,blockobj=objtable;
static P backobj=sv,oldrt=sv,olddepth=sv;
static U allocated,released,applied,drawn,begins,ends,copied,compile_calls;
static I in_scene,force_draw_failure;
static P current_rt,current_depth;static VP actualvp;
static U API release_aa(P p){(void)p;released++;return 1;}
static P API bytes(P p){(void)p;static U code[4]={0xffff0300,0x0000ffff};return code;}
static I API ok0(P o){(void)o;return 0;}
static I API begin0(P o){(void)o;if(in_scene)fail=20;in_scene=1;begins++;return 0;}
static I API end0(P o){(void)o;if(!in_scene)fail=21;in_scene=0;ends++;return 0;}
static I API back0(P o,U a,U b,U c,P*out){(void)o;(void)a;(void)b;(void)c;*out=&backobj;return 0;}
static I API readrt(P o,U slot,P*out){(void)o;*out=slot?0:current_rt;return slot?(I)0x8876086c:0;}
static I API writert(P o,U slot,P p){(void)o;if(slot==0)current_rt=p;return 0;}
static I API readdepth(P o,P*out){(void)o;*out=current_depth;return 0;}
static I API writedepth(P o,P p){(void)o;current_depth=p;return 0;}
static I API readvp(P o,VP*v){(void)o;*v=actualvp;return 0;}
static I API writevp(P o,const VP*v){(void)o;actualvp=*v;return 0;}
static I API mktexture(P o,U w,U h,U levels,U usage,U fmt,U pool,P*out,P shared){(void)o;(void)shared;if(w!=3840||h!=2160||levels!=1||usage!=1||fmt!=21||pool)fail=22;allocated++;*out=&texobj;return 0;}
static I API getsurface(P o,U level,P*out){(void)o;(void)level;*out=&sobj;return 0;}
static I API mkshader(P o,const U*code,P*out){(void)o;if(code[0]!=0xffff0300)fail=23;*out=&psobj;return 0;}
static I API stateblock(P o,U type,P*out){(void)o;if(type!=1)fail=24;*out=&blockobj;return 0;}
static I API applyblock(P o){(void)o;applied++;return 0;}
static I API copy0(P o,P a,P b,P c,P d,U f){(void)o;if(in_scene||a!=&backobj||b||c!=&sobj||d||f)fail=25;copied++;return 0;}
static I API oneptr(P o,P p){(void)o;(void)p;return 0;}
static I API oneuint(P o,U p){(void)o;(void)p;return 0;}
static I API twouint(P o,U p,U q){(void)o;(void)p;(void)q;return 0;}
static I API threeuint(P o,U p,U q,U r){(void)o;(void)p;(void)q;(void)r;return 0;}
static I API uintptr(P o,U p,P q){(void)o;(void)p;(void)q;return 0;}
static I API constants(P o,U start,const float*c,U count){(void)o;if(start||count!=2||c[0]<=0||c[1]<=0)fail=26;return 0;}
static I API quad(P o,U type,U count,const void*data,U stride){(void)o;(void)data;if(!in_scene||type!=5||count!=2||stride!=24||current_rt!=&backobj||current_depth||actualvp.w!=3840||actualvp.h!=2160)fail=27;drawn++;return force_draw_failure?(I)0x8876086c:0;}
#ifndef AA_TEST_ENTRY
#define AA_TEST_ENTRY _start
#endif
void AA_TEST_ENTRY(void){
 objtable[2]=release_aa;objtable[3]=bytes;objtable[5]=applyblock;objtable[18]=getsurface;
 sv[2]=release_aa;sv[12]=getdesc;
 dv[2]=release_aa;dv[3]=ok0;dv[18]=back0;dv[23]=mktexture;dv[34]=copy0;
 dv[37]=writert;dv[38]=readrt;dv[39]=writedepth;dv[40]=readdepth;
 dv[41]=begin0;dv[42]=end0;dv[47]=writevp;dv[48]=readvp;
 dv[57]=twouint;dv[59]=stateblock;dv[65]=uintptr;dv[67]=threeuint;dv[69]=threeuint;
 dv[83]=quad;dv[89]=oneuint;dv[92]=oneptr;dv[102]=twouint;dv[106]=mkshader;dv[107]=oneptr;dv[109]=constants;
 record(&dobj,119);State*t=newstate(&dobj);t->main=1;t->rw=3840;t->rh=2160;
 aa_method=1;aa_bytecode=&byteobj;current_rt=&oldrt;current_depth=&olddepth;actualvp.w=99;actualvp.h=88;
 aa_apply(t);
 if(t->aa_failed||drawn!=1||applied!=1||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=99||actualvp.h!=88||in_scene)fail=28;
 /* A transient device Release must not invalidate cached AA resources. */
 ((U(API*)(P))dv[2])(&dobj);
 if(!t->aa_tex||!t->aa_surface||!t->aa_shader)fail=34;
 aa_apply(t);if(allocated!=1||drawn!=2||t->aa_passes!=2||copied!=2)fail=29;
 aa_method=0;aa_apply(t);if(drawn!=2)fail=30;
 aa_method=1;force_draw_failure=1;aa_apply(t);
 if(!t->aa_failed||t->aa_tex||t->aa_surface||t->aa_shader||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=99||in_scene||applied!=3||begins!=ends)fail=31;
 aa_apply(t);if(drawn!=3)fail=32;
 aa_compile_failed=1;aa_bytecode=0;t->aa_failed=0;aa_apply(t);if(!t->aa_failed||drawn!=3||in_scene)fail=33;
 (void)compile_calls;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
