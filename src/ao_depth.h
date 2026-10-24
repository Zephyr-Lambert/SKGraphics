/* Depth access and screen-space AO. Same pre-UI boundary as bloom.
 * INTZ replaces only the main-size D24S8 texture when explicitly enabled.
 * No color image is sampled or copied in the diagnostic pass.
 */
#include "ao_source.h"
static float ao_sqrt(float v){float r;__asm__("fsqrt":"=t"(r):"0"(v));return r;}
static I ao_projection(const float*m,float*out){
 /* Row-vector WVP from c8..11. For a rigid/uniformly scaled world-view,
  * column Z = A*column W (upper three rows), and m[14]-A*m[15]=B.
  * Orthogonalized X/Y columns recover projection scales and offsets.
  */
 float ww=0,zw=0,xw=0,yw=0;
 for(U i=0;i<3;i++){float w=m[i*4+3];ww+=w*w;zw+=m[i*4+2]*w;xw+=m[i*4]*w;yw+=m[i*4+1]*w;}
 if(!(ww>0.000001f&&ww<1000000))return 0;
 float a=zw/ww,b=m[14]-a*m[15],ox=xw/ww,oy=yw/ww;
 if(!(a>=1.0f&&a<1.2f&&b<-.0001f&&b>-10000))return 0;
 float xx=0,yy=0,xy=0,zz=0;
 for(U i=0;i<3;i++){float x=m[i*4]-ox*m[i*4+3],y=m[i*4+1]-oy*m[i*4+3],z=m[i*4+2]-a*m[i*4+3];xx+=x*x;yy+=y*y;xy+=x*y;zz+=z*z;}
 if(!(xx>0&&yy>0&&xx<100000000&&yy<100000000&&xy*xy<xx*yy*.00001f&&zz<ww*.000001f))return 0;
 float px=ao_sqrt(xx/ww),py=ao_sqrt(yy/ww);
 if(!(px>.05f&&px<100&&py>.05f&&py<100&&ox>-.5f&&ox<.5f&&oy>-.5f&&oy<.5f))return 0;
 out[0]=a;out[1]=b;out[2]=1/px;out[3]=1/py;out[4]=ox;out[5]=oy;
 return 1;
}
static void ao_camera(State*t){
 float m[16];P get=orig(t->obj,95);t->ao_valid=0;
 if(get&&((I(API*)(P,U,float*,U))get)(t->obj,8,m,4)>=0)t->ao_valid=ao_projection(m,t->ao_proj);
 if(!t->ao_valid&&!t->ao_projection_warned){begin("AO skipped: unsupported camera matrix");emit();t->ao_projection_warned=1;}
 if(t->ao_valid){t->ao_proj[6]=1.0f/(float)t->rw;t->ao_proj[7]=1.0f/(float)t->rh;}
}
static void ao_resources_release(State*t){
 com_release(t->ao_surf);com_release(t->ao_tex);t->ao_surf=t->ao_tex=0;t->ao_w=t->ao_h=0;
 for(U i=0;i<2;i++){com_release(t->ao_shaders[i]);t->ao_shaders[i]=0;}
}
static I ao_resources(State*t,U w,U h){
 if(t->ao_tex&&t->ao_surf&&t->ao_shaders[0]&&t->ao_shaders[1]&&t->ao_w==w&&t->ao_h==h)return 1;
 ao_resources_release(t);

 P mod=d3dx_library(),fn=mod?GetProcAddress(mod,"D3DXCompileShader"):0;
 if(!fn){begin("AO compiler unavailable");emit();return 0;}
 const char*entries[]={"obscurance","applyAO"};
 for(U i=0;i<2;i++){
  P code=0,err=0;I hr=((I(API*)(const char*,U,P,P,const char*,const char*,U,P*,P*,P*))fn)(ao_source,sizeof(ao_source)-1,0,0,entries[i],"ps_3_0",0,&code,&err,0);
  begin("AO shader compilation");kv("pass",i);hx("result",hr);emit();
  if(hr<0&&err){P*v=*(P**)err;const char*msg=((P(API*)(P))v[3])(err);begin("AO compiler message: ");if(msg)s(msg);emit();}
  com_release(err);
  if(hr>=0&&code){P*v=*(P**)code;hr=((I(API*)(P,const U*,P*))orig(t->obj,106))(t->obj,((P(API*)(P))v[3])(code),&t->ao_shaders[i]);}
  com_release(code);if(hr<0||!t->ao_shaders[i]){ao_resources_release(t);return 0;}
 }
 if(!aa_target(t->obj,w,h,21,&t->ao_tex,&t->ao_surf)){ao_resources_release(t);return 0;}
 t->ao_w=w;t->ao_h=h;begin("AO resources");kv("width",w);kv("height",h);emit();return 1;
}
static void ao_depth_retain(State*t){
 P surf=0;((I(API*)(P,P*))orig(t->obj,40))(t->obj,&surf);
 if(!surf)return;Desc d={0};P*v=*(P**)surf;
 if(((I(API*)(P,Desc*))v[12])(surf,&d)>=0&&d.w==t->rw&&d.h==t->rh&&!d.ms&&d.format==0x5a544e49){t->ao_depth=surf;return;}
 com_release(surf);
}
static I ao_depth_shader(State*t){
 if(t->ao_ps)return 1;
 static const char source[]=
 "sampler2D Depth:register(s0);float4 Params:register(c0);"
 "float4 main(float2 uv:TEXCOORD0):COLOR0{float d=tex2D(Depth,uv).r;"
 "clip(0.9999999-d);float g=1.0/(1.0+Params.x*(1.0-d));return float4(g,g,g,1);}";

 P mod=d3dx_library(),fn=mod?GetProcAddress(mod,"D3DXCompileShader"):0;
 if(!fn){begin("AO depth compiler unavailable");emit();return 0;}
 P code=0,err=0;I hr=((I(API*)(const char*,U,P,P,const char*,const char*,U,P*,P*,P*))fn)(source,sizeof(source)-1,0,0,"main","ps_3_0",0,&code,&err,0);
 begin("AO depth shader compilation");hx("result",hr);emit();
 if(hr<0&&err){P*v=*(P**)err;begin("AO depth compiler message: ");s(((P(API*)(P))v[3])(err));emit();}
 com_release(err);
 if(hr>=0&&code){P*v=*(P**)code;hr=((I(API*)(P,const U*,P*))orig(t->obj,106))(t->obj,((P(API*)(P))v[3])(code),&t->ao_ps);}
 com_release(code);return hr>=0&&t->ao_ps;
}
