/* Exercise SMAA's real resource/pass orchestration with mock COM objects.
 * Does not execute HLSL or validate GPU output. */
#define AA_TEST_ENTRY fxaa_test_entry
#include "aa_mock.c"
static P textures[5],surfaces[3],texture_vt[21],surface_vt[13];
static U widths[5],heights[5],lookup_count,clears,pass_index,fail_pass;
static P bound[3];
static unsigned char pixels[560*(160*4+16)];
static I texture_index(P o){for(I i=0;i<5;i++)if(o==&textures[i])return i;return -1;}
static I API sm_mktex(P o,U w,U h,U levels,U usage,U fmt,U pool,P*out,P shared){
 (void)o;(void)shared;
 if(allocated>=5||levels!=1||fmt!=21) {fail=40;return -1;}
 U i=allocated++;widths[i]=w;heights[i]=h;textures[i]=texture_vt;*out=&textures[i];
 if(usage==1){if(pool||w!=3840||h!=2160)fail=41;}
 else if(pool!=1||!((w==160&&h==560)||(w==64&&h==16)))fail=42;
 return 0;
}
static I API sm_surface(P o,U level,P*out){
 (void)level;I i=texture_index(o);U j=i==4?2:(U)i;if(j>=3){fail=43;return -1;}
 surfaces[j]=surface_vt;*out=&surfaces[j];return 0;
}
static I API lock0(P o,U level,P data,P rect,U flags){
 (void)level;(void)rect;(void)flags;I i=texture_index(o);
 struct Locked{I pitch;P bits;}*r=data;r->pitch=(I)(widths[i]*4+16);r->bits=pixels;return 0;
}
static I API unlock0(P o,U level){
 (void)level;I i=texture_index(o);U w=widths[i],h=heights[i],channels=w==160?2:1;
 const unsigned char*src=channels==2?areaTexBytes:searchTexBytes;
 for(U y=0;y<h;y++)for(U x=0;x<w;x++){
  unsigned char*p=pixels+y*(w*4+16)+x*4;U k=(y*w+x)*channels;
  if(p[0]||p[1]!=(channels==2?src[k+1]:0)||p[2]!=src[k]||p[3]!=255)fail=44;
 }
 lookup_count++;return 0;
}
static I API sm_bind(P o,U slot,P p){(void)o;if(slot<3)bound[slot]=p;return 0;}
static I API sm_rt(P o,U slot,P p){
 (void)o;if(!slot){
 for(U i=0;i<3;i++)if(p==&surfaces[i])for(U j=0;j<3;j++)if(bound[j]==&textures[i==2?4:i])fail=45;
 current_rt=p;
 }return 0;
}
static I API clear0(P o,U count,P rect,U flags,U color,float z,U stencil){
 (void)o;(void)z;(void)stencil;
 if(!in_scene||count||rect||flags!=1||color||current_rt==&backobj)fail=46;
 clears++;return 0;
}
static I API sm_copy(P o,P a,P b,P c,P d,U f){(void)o;if(in_scene||a!=&backobj||b||c!=&surfaces[2]||d||f)fail=47;copied++;return 0;}
static I API sm_quad(P o,U type,U count,const void*data,U stride){
 (void)o;(void)data;U pass=pass_index++%3;
 if(!in_scene||type!=5||count!=2||stride!=24||current_depth||actualvp.w!=3840||actualvp.h!=2160)fail=48;
 if(current_rt!=(pass<2?&surfaces[pass]:&backobj))fail=49;
 if(bound[0]!=(pass==1?&textures[0]:&textures[4]))fail=50;
 if(pass==1&&(bound[1]!=&textures[2]||bound[2]!=&textures[3]))fail=51;
 if(pass==2&&bound[1]!=&textures[1])fail=52;
 drawn++;return fail_pass==pass+1?(I)0x8876086c:0;
}
static void prepare(State*t){
 aa_cleanup(t);t->aa_failed=0;t->aa_passes=0;allocated=lookup_count=clears=pass_index=drawn=copied=0;
 for(U i=0;i<3;i++)bound[i]=0;
 current_rt=&oldrt;current_depth=&olddepth;actualvp.w=99;actualvp.h=88;
}
void _start(void){
 objtable[2]=release_aa;objtable[3]=bytes;objtable[5]=applyblock;
 surface_vt[2]=release_aa;surface_vt[12]=getdesc;sv[2]=release_aa;sv[12]=getdesc;
 texture_vt[2]=release_aa;texture_vt[18]=sm_surface;texture_vt[19]=lock0;texture_vt[20]=unlock0;
 dv[2]=release_aa;dv[3]=ok0;dv[18]=back0;dv[23]=sm_mktex;dv[34]=sm_copy;
 dv[37]=sm_rt;dv[38]=readrt;dv[39]=writedepth;dv[40]=readdepth;
 dv[41]=begin0;dv[42]=end0;dv[43]=clear0;dv[47]=writevp;dv[48]=readvp;
 dv[57]=twouint;dv[59]=stateblock;dv[65]=sm_bind;dv[67]=threeuint;dv[69]=threeuint;
 dv[83]=sm_quad;dv[89]=oneuint;dv[92]=oneptr;dv[102]=twouint;dv[106]=mkshader;dv[107]=oneptr;dv[109]=constants;
 record(&dobj,119);State*t=newstate(&dobj);aa_method=2;
 for(U i=0;i<3;i++)sm_bytecode[i]=&byteobj;
 prepare(t);aa_apply(t);aa_apply(t);
 if(t->aa_failed||allocated!=5||lookup_count!=2||clears!=4||drawn!=6||copied!=2||t->aa_passes!=2||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=99||in_scene)fail=53;
 ((U(API*)(P))dv[2])(&dobj);if(!t->sm_tex[0]||!t->sm_area)fail=54;
 for(U i=1;i<=3;i++){
  prepare(t);fail_pass=i;aa_apply(t);
  if(!t->aa_failed||t->aa_tex||t->sm_tex[0]||t->sm_tex[1]||t->sm_area||t->sm_search||t->sm_shader[2]||current_rt!=&oldrt||current_depth!=&olddepth||actualvp.w!=99||in_scene||begins!=ends)fail=55;
 }
 fail_pass=0;prepare(t);aa_strength=0;aa_apply(t);if(allocated||drawn)fail=56;
 aa_strength=100;sm_compile_failed=1;for(U i=0;i<3;i++)sm_bytecode[i]=0;
 aa_apply(t);if(!t->aa_failed||allocated||drawn)fail=57;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
