#define _start shadow_tests_unused
#include "shadow_mock.c"
#undef _start
static P rtv[13],psv[5],rtobj=rtv,psobj=psv;
static U acquired,queries,key_down,forwarded,read_mode;
static U constant_calls,constant_fail,constant_variant;
static I API constantsget(P o,U start,float*out,U count){
 (void)o;constant_calls++;
 if(!((start==0&&count==12)||(start==149&&count==2)))fail=22;
 if(constant_fail)return -1;
 for(U i=0;i<count*4;i++)out[i]=(float)(start+i+constant_variant);return 0;
}
static short API key0(I key){if(key!=0x77)fail=1;return key_down?(short)0x8000:0;}
static I API rtget(P o,U slot,P*out){(void)o;if(slot)fail=2;*out=&rtobj;acquired++;queries++;return 0;}
static I API nullget(P o,P*out){(void)o;*out=0;return -1;}
static I API psget(P o,P*out){(void)o;*out=&psobj;acquired++;return 0;}
static I API rsget(P o,U slot,U*out){(void)o;*out=slot;return 0;}
static I API texget(P o,U slot,P*out){(void)o;(void)slot;*out=0;return 0;}
static I API vpget(P o,VP*out){(void)o;out->w=2560;out->h=1440;return 0;}
static I API descget(P o,Desc*out){(void)o;out->w=2560;out->h=1440;out->format=22;return 0;}
static I API containerget(P o,const void*iid,P*out){(void)o;(void)iid;*out=0;return -1;}
static I API codeget(P o,P data,U*size){
 (void)o;if(!data){*size=read_mode==1?20000:8;return 0;}
 if(*size!=8)fail=3;if(read_mode==2)return -1;
 U*p=data;p[0]=0xffff0300;p[1]=65535;return 0; /* leaves size unchanged */
}
static I API simple0(P o){if(o!=&dev)fail=4;forwarded++;return -7;}
static I API clear0(P o,U count,const Rect*rect,U flags,U color,float z,U stencil){
 if(o!=&dev||count||rect||flags!=3||color!=123||z!=1.0f||stencil)fail=5;forwarded++;return -8;
}
void _start(void){
 dev_vt[38]=rtget;dev_vt[40]=nullget;dev_vt[108]=psget;dev_vt[93]=nullget;
 dev_vt[48]=vpget;dev_vt[58]=rsget;dev_vt[64]=texget;dev_vt[95]=constantsget;
 dev_vt[41]=simple0;dev_vt[42]=simple0;dev_vt[43]=clear0;record(&dev,119);
 rtv[2]=release0;rtv[11]=containerget;rtv[12]=descget;psv[2]=release0;psv[4]=codeget;
 scene_draw(&dev,82,4,3,exe+123);if(queries)fail=6;
 scene_probe_enabled=1;scene_log=(P)1;scene_initialized=1;scene_key=key0;
 key_down=1;scene_present(&dev);if(!scene_active||scene_captures!=1||scene_lines!=1)fail=7;
 scene_draw((P)2,82,4,3,exe+123);if(queries)fail=8;
 scene_draw(&dev,82,4,3,exe+123);if(queries!=1||scene_draws!=1||acquired!=releases)fail=9;
 if(scene_beginscene(&dev)!=-7||scene_endscene(&dev)!=-7||scene_clear(&dev,0,0,3,123,1.0f,0)!=-8||forwarded!=3)fail=10;
 scene_present(&dev);if(scene_active||scene_captures!=1)fail=11;
 scene_present(&dev);if(scene_captures!=1)fail=12; /* held F8 */
 key_down=0;scene_present(&dev);key_down=1;scene_present(&dev);if(scene_captures!=2||!scene_active)fail=13;
 scene_lines=3998;U before=queries;scene_draw(&dev,82,4,3,exe);if(queries!=before)fail=14;
 scene_finish("Reset");if(scene_active||scene_lines!=3999)fail=15;
 key_down=0;scene_present(&dev);key_down=1;scene_present(&dev);if(scene_captures!=3)fail=16;
 key_down=0;scene_present(&dev);key_down=1;scene_present(&dev);if(scene_active||scene_captures!=3)fail=17;
 char hash[65];read_mode=1;scene_shader(&psobj,hash);if(hash[0]!='?')fail=18;
 read_mode=2;scene_shader(&psobj,hash);if(hash[0]!='?')fail=19;
 read_mode=0;scene_shader(&psobj,hash);if(hash[0]=='?'||!hash[63]||hash[64])fail=20;
 if(acquired!=releases)fail=21;
 VP vp={0};vp.maxz=1.0f;scene_lines=0;
 scene_ao(&dev,&rtobj,&psobj,&vp);if(constant_calls||scene_lines)fail=23;
 ao_probe_enabled=1;scene_ao(&dev,&rtobj,&psobj,&vp);
 if(constant_calls!=2||scene_lines!=5||!ao_constants_valid)fail=24;
 scene_ao(&dev,&rtobj,&psobj,&vp);if(constant_calls!=4||scene_lines!=6)fail=25;
 constant_variant=1;scene_ao(&dev,&rtobj,&psobj,&vp);if(scene_lines!=11)fail=26;
 constant_fail=1;scene_ao(&dev,0,&psobj,&vp);if(scene_lines!=13||ao_constants_valid)fail=27;
 scene_ao(&dev,0,0,&vp);if(scene_lines!=14)fail=28;
 constant_fail=0;scene_lines=3996;scene_ao(&dev,0,&psobj,&vp);
 if(scene_lines!=3998)fail=29;
 before=constant_calls;scene_ao(&dev,0,&psobj,&vp);if(constant_calls!=before||scene_lines!=3998)fail=30;
 scene_active=1;scene_finish("limit");if(scene_lines!=3999||scene_active)fail=31;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
