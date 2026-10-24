/* Read-only one-frame pass capture for locating the pre-HUD bloom boundary.
 * No resource creation, shader replacement, render-state changes or retained
 * COM references. Three manually triggered frames, bounded output per frame.
 */
static P scene_log,scene_device;
static short(API *scene_key)(I);
static U scene_initialized,scene_down,scene_active,scene_captures,scene_lines,scene_draws;
static U scene_code[4096];
static U ao_constants[56],ao_constants_valid;
static void scene_emit(void){
 if(!scene_log||scene_lines>=4000)return;
 U written;s("\r\n");WriteFile(scene_log,line,used,&written,0);scene_lines++;
}
static void scene_begin(const char*name){begin(name);kv("capture",scene_captures);kv("sequence",scene_lines);}
static I scene_recording(P o){return scene_probe_enabled&&scene_active&&o==scene_device&&scene_lines<3998;}
static void scene_shader(P shader,char hash[65]){
 hash[0]='?';hash[1]=0;if(!shader){hash[0]='0';return;}
 U bytes=0;P*v=*(P**)shader;
 if(((I(API*)(P,P,U*))v[4])(shader,0,&bytes)<0||bytes<8||bytes>sizeof(scene_code)||bytes%4)return;
 U size=bytes;if(((I(API*)(P,P,U*))v[4])(shader,scene_code,&size)<0||size!=bytes)return;
 texture_hash(scene_code,bytes,hash);
}
static void scene_surface(P surf,const char*name){
 hx(name,(U)(unsigned long)surf);if(!surf)return;
 Desc d={0};P*v=*(P**)surf;
 if(((I(API*)(P,Desc*))v[12])(surf,&d)>=0){kv("w",d.w);kv("h",d.h);kv("fmt",d.format);}
}
/* Read-only AO investigation. Hex values preserve exact IEEE-754 float bits.
 * CTAB identifies c0..3=world, c4..7=local, c8..11=WVP, c149..150=eye.
 * Constants are evidence, not necessarily meaningful for every shader.
 */
static void scene_ao(P o,P depth,P vs,const VP*vp){
 if(!ao_probe_enabled||scene_lines>=3998)return;
 P tex=0;Desc d={0};I desc_hr=-1,container_hr=-1;
 if(depth){P*v=*(P**)depth;desc_hr=((I(API*)(P,Desc*))v[12])(depth,&d);
  container_hr=((I(API*)(P,const void*,P*))v[11])(depth,probe_texture_iid,&tex);}
 union {float f;U u;} zmin,zmax;zmin.f=vp->minz;zmax.f=vp->maxz;
 scene_begin("AOProbe depth");kv("draw",scene_draws);hx("surface",(U)(unsigned long)depth);
 hx("texture",(U)(unsigned long)tex);hx("descResult",desc_hr);hx("containerResult",container_hr);
 kv("width",d.w);kv("height",d.h);kv("format",d.format);kv("multisample",d.ms);
 hx("minZbits",zmin.u);hx("maxZbits",zmax.u);scene_emit();com_release(tex);
 if(!vs||scene_lines>=3998)return;
 union {float f[56];U u[56];} values;
 P get=orig(o,95);if(!get)return;
 I hr=((I(API*)(P,U,float*,U))get)(o,0,values.f,12);
 I eye=((I(API*)(P,U,float*,U))get)(o,149,values.f+48,2);
 if(hr<0||eye<0){scene_begin("AOProbe constants unavailable");kv("draw",scene_draws);hx("result",hr);hx("eyeResult",eye);scene_emit();ao_constants_valid=0;return;}
 U changed=!ao_constants_valid;for(U i=0;i<56;i++)if(values.u[i]!=ao_constants[i])changed=1;
 if(!changed)return;
 for(U group=0;group<4;group++){
  if(scene_lines>=3998)break;
  U start=group*16,count=group==3?8:16;
  scene_begin("AOProbe constants");kv("draw",scene_draws);kv("firstRegister",group==3?149:group*4);
  s(" floatBits=");for(U i=0;i<count;i++){if(i)s(",");x(values.u[start+i]);}scene_emit();
 }
 for(U i=0;i<56;i++)ao_constants[i]=values.u[i];ao_constants_valid=1;
}
static void scene_draw(P o,U slot,U type,U primitives,P caller){
 if(!scene_recording(o))return;
 P rt=0,depth=0,ps=0,vs=0,container=0;VP vp={0};char ph[65],vh[65];
 ((I(API*)(P,U,P*))orig(o,38))(o,0,&rt);
 ((I(API*)(P,P*))orig(o,40))(o,&depth);
 ((I(API*)(P,P*))orig(o,108))(o,&ps);((I(API*)(P,P*))orig(o,93))(o,&vs);
 scene_shader(ps,ph);scene_shader(vs,vh);
 ((I(API*)(P,VP*))orig(o,48))(o,&vp);
 scene_begin("SceneProbe draw");kv("draw",++scene_draws);kv("slot",slot);kv("type",type);kv("primitives",primitives);
 hx("callerRVA",(U)((unsigned long)caller-(unsigned long)GetModuleHandleW(0)));
 scene_surface(rt,"rt");hx("depth",(U)(unsigned long)depth);
 if(rt){P*v=*(P**)rt;((I(API*)(P,const void*,P*))v[11])(rt,probe_texture_iid,&container);}
 hx("rtTexture",(U)(unsigned long)container);kv("vpX",vp.x);kv("vpY",vp.y);kv("vpW",vp.w);kv("vpH",vp.h);
 s(" ps=");s(ph);s(" vs=");s(vh);
 const U rs[]={7,14,15,27,19,20,171,168};
 const char*names[]={"z","zwrite","atest","blend","src","dst","blendop","writeMask"};
 for(U j=0;j<8;j++){U val=0;I hr=((I(API*)(P,U,U*))orig(o,58))(o,rs[j],&val);kv(names[j],hr>=0?val:0xffffffff);}
 for(U j=0;j<8;j++){P tex=0;((I(API*)(P,U,P*))orig(o,64))(o,j,&tex);s(" t");n(j);s("=");x((U)(unsigned long)tex);com_release(tex);}
 scene_emit();scene_ao(o,depth,vs,&vp);com_release(container);com_release(rt);com_release(depth);com_release(ps);com_release(vs);
}
static void scene_finish(const char*reason){
 if(!scene_active)return;
 scene_begin("SceneProbe end");kv("draws",scene_draws);kv("truncated",scene_lines>=3998);s(" reason=");s(reason);scene_emit();scene_active=0;scene_device=0;
}
static void scene_present(P o){
 if(!scene_probe_enabled)return;
 if(scene_active&&o==scene_device)scene_finish("Present-before-AA");
 if(!scene_initialized){
  scene_initialized=1;static const W dll[]={'u','s','e','r','3','2','.','d','l','l',0};P lib=LoadLibraryW(dll);
  if(lib)scene_key=(short(API*)(I))GetProcAddress(lib,"GetAsyncKeyState");
  begin(scene_key?"SceneProbe ready: press F8 during gameplay (three captures per launch)":"SceneProbe unavailable: GetAsyncKeyState missing");emit();
 }
 if(!scene_key)return;
 U down=(((U)scene_key(0x77))&0x8000)!=0;
 if(down&&!scene_down&&scene_captures<3){
  if(!scene_log){
   W file[1024];U k=GetModuleFileNameW(module,file,1024);
   if(k&&k<980){while(k&&file[k-1]!='\\'&&file[k-1]!='/')k--;const char*name="SKGraphics_SceneProbe.log";while(*name)file[k++]=(W)*name++;file[k]=0;
    scene_log=CreateFileW(file,0x40000000,1,0,2,0x80,0);if(scene_log==(P)-1)scene_log=0;
   }
  }
  if(scene_log){scene_captures++;scene_lines=0;scene_draws=0;ao_constants_valid=0;scene_active=1;scene_device=o;scene_begin("SceneProbe start");s(" version=0.9.4 raw-shader-SHA256");kv("aoProbe",ao_probe_enabled);scene_emit();}
  else {begin("SceneProbe ERROR: cannot create capture log");emit();}
 }
 scene_down=down;
}
static I API scene_beginscene(P o){I hr=((I(API*)(P))orig(o,41))(o);if(scene_recording(o)){scene_begin("SceneProbe BeginScene");hx("result",hr);scene_emit();}return hr;}
static I API scene_endscene(P o){I hr=((I(API*)(P))orig(o,42))(o);if(scene_recording(o)){scene_begin("SceneProbe EndScene");hx("result",hr);scene_emit();}return hr;}
static I API scene_clear(P o,U count,const Rect*rects,U flags,U color,float z,U stencil){
 I hr=((I(API*)(P,U,const Rect*,U,U,float,U))orig(o,43))(o,count,rects,flags,color,z,stencil);
 if(scene_recording(o)){P rt=0;((I(API*)(P,U,P*))orig(o,38))(o,0,&rt);scene_begin("SceneProbe Clear");scene_surface(rt,"rt");kv("rects",count);hx("flags",flags);hx("color",color);hx("result",hr);scene_emit();com_release(rt);
  if(ao_probe_enabled&&scene_lines<3998){P ds=0;((I(API*)(P,P*))orig(o,40))(o,&ds);union{float f;U u;}bits;bits.f=z;
   scene_begin("AOProbe clear");scene_surface(ds,"depth");hx("flags",flags);hx("zBits",bits.u);kv("stencil",stencil);hx("result",hr);scene_emit();com_release(ds);}
 }return hr;
}
