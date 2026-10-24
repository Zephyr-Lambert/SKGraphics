/* Read-only, bounded runtime evidence for the next shadow-filter implementation.
 * Map identity comes from the validated CShadowMap::Begin call, not dimensions.
 * GetContainer references are released before Reset/device replacement.
 */
static const U probe_texture_iid[4]={0x85c31227,0x4f003de5,0x1af13a9b,0xb5188cc3};
static U probe_code[4096],probe_hashes[32],probe_sizes[32],probe_codes;
static U probe_pairs[128][4],probe_pairs_count;
static void probe_cleanup(State*t){if(!t)return;com_release(t->probe_color);com_release(t->probe_depth);t->probe_color=t->probe_depth=0;t->shadow_pass=0;}
static void probe_surface(State*t,P surface,U depth){
 if(!shadow_probe_enabled||!surface||!t)return;
 P tex=0;P*v=*(P**)surface;I hr=((I(API*)(P,const void*,P*))v[11])(surface,probe_texture_iid,&tex);
 P*dst=depth?&t->probe_depth:&t->probe_color;
 if(hr<0||!tex){com_release(tex);return;}
 if(*dst==tex){com_release(tex);return;}com_release(*dst);*dst=tex;
 Desc d={0};v=*(P**)tex;hr=((I(API*)(P,U,Desc*))v[17])(tex,0,&d);
 begin("ShadowProbe map identified");kv("depth",depth);kv("width",d.w);kv("height",d.h);kv("format",d.format);hx("result",hr);emit();
}
static U probe_shader(P shader,U vertex){
 if(!shader)return 0;
 U bytes=0;P*v=*(P**)shader;I hr=((I(API*)(P,P,U*))v[4])(shader,0,&bytes);
 if(hr<0||bytes<8||bytes>sizeof(probe_code)||bytes%4)return 0xffffffff;
 U size=bytes;hr=((I(API*)(P,P,U*))v[4])(shader,probe_code,&size);
 if(hr<0||size!=bytes)return 0xffffffff;
 U hash=2166136261;for(U i=0;i<bytes/4;i++){hash^=probe_code[i];hash*=16777619;}
 for(U i=0;i<probe_codes;i++)if(probe_hashes[i]==hash&&probe_sizes[i]==bytes)return i+1;
 if(probe_codes==32)return 0xffffffff;
 U id=++probe_codes;probe_hashes[id-1]=hash;probe_sizes[id-1]=bytes;
 begin("ShadowProbe shader");kv("id",id);kv("vertex",vertex);kv("bytes",bytes);hx("hash",hash);emit();
 for(U i=0;i<bytes/4;i+=24){begin("ShadowProbe code");kv("id",id);kv("word",i);for(U j=i;j<bytes/4&&j<i+24;j++){s(" ");x(probe_code[j]);}emit();}
 return id;
}
static void probe_draw(P o){
 State*t=state(o);
 /* One sampled frame per 60 presents; never retain shaders or change state.
  * A hard query cap also bounds unusual apps that draw without presenting. */
 if(!shadow_probe_enabled||!t||t->aa_busy||(!t->probe_color&&!t->probe_depth)||frames%60||t->probe_checks>=100000||events+400>=limit)return;
 t->probe_checks++;
 U color=0,depth=0;
 for(U stage=0;stage<16;stage++){
  P tex=0;I hr=((I(API*)(P,U,P*))orig(o,64))(o,stage,&tex);
  if(hr>=0&&tex){if(tex==t->probe_color)color|=1u<<stage;if(tex==t->probe_depth)depth|=1u<<stage;}com_release(tex);
 }
 if(!t->shadow_pass&&!color&&!depth)return;
 P ps=0,vs=0;I hp=((I(API*)(P,P*))orig(o,108))(o,&ps);I hv=((I(API*)(P,P*))orig(o,93))(o,&vs);
 U pi=hp>=0?probe_shader(ps,0):0xffffffff,vi=hv>=0?probe_shader(vs,1):0xffffffff;com_release(ps);com_release(vs);
 U role=(t->shadow_pass?1u:0u)|(color<<1);U found=0;
 for(U i=0;i<probe_pairs_count;i++)if(probe_pairs[i][0]==pi&&probe_pairs[i][1]==vi&&probe_pairs[i][2]==role&&probe_pairs[i][3]==depth){found=1;break;}
 if(found||probe_pairs_count==128)return;
 U*p=probe_pairs[probe_pairs_count++];p[0]=pi;p[1]=vi;p[2]=role;p[3]=depth;
 begin("ShadowProbe draw");kv("pixelShader",pi);kv("vertexShader",vi);kv("writer",t->shadow_pass);hx("colorSamplerMask",color);hx("depthSamplerMask",depth);emit();
 const U rs[]={7,14,15,19,20,22,27,168,174,175,195};
 for(U i=0;i<sizeof(rs)/sizeof(rs[0]);i++){U value=0;I hr=((I(API*)(P,U,U*))orig(o,58))(o,rs[i],&value);begin("ShadowProbe renderState");kv("type",rs[i]);hx("value",value);hx("result",hr);emit();}
 for(U stage=0;stage<16;stage++)if((color|depth)&(1u<<stage)){
  begin("ShadowProbe sampler");kv("stage",stage);
  const U types[]={1,2,5,6,7,10,11};const char*names[]={"addressU","addressV","mag","min","mip","maxAnisotropy","sRGB"};
  for(U j=0;j<7;j++){U value=0;I hr=((I(API*)(P,U,U,U*))orig(o,68))(o,stage,types[j],&value);kv(names[j],hr>=0?value:0xffffffff);}emit();
 }
}
