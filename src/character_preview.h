/* Diagnostic only: preserve original shader alpha/discards and tint RGB.
 * Cyan = candidate body/hair/enemy shader groups. Amber = ambiguous group.
 * Uses existing geometry/depth testing, not a second draw or an AO mask. */
typedef struct {P device,source,tint;U role;} CharacterShader;
static CharacterShader character_shaders[128];
static U character_count,character_words[4112],character_draws;
static U character_transform(const U*in,U bytes,U role){
 if(bytes<8||bytes>16384||bytes%4||in[0]!=0xffff0300u||!role)return 0;
 U total=bytes/4,p=1,n=7,writes=0;
 character_words[0]=in[0];character_words[1]=0x05000051;
 character_words[2]=0xa00f00df;
 character_words[3]=(role==2||role==3)?0x3f800000:0;
 character_words[4]=(role==1||role==3)?0x3f800000:role==2?0x3f000000:0;
 character_words[5]=(role==1||role==3)?0x3f800000:0;
 character_words[6]=0x3f800000;
 while(p<total){
  U token=in[p],op=token&65535,len=op==65534?(token>>16)&32767:(token>>24)&15;
  if(len>total-p-1)return 0;
  if(op==65535){
   if(p+1!=total||!writes)return 0;
   character_words[n++]=0x02000001;character_words[n++]=0x80070800;
   character_words[n++]=0xa0e400df;character_words[n++]=0x0000ffff;return n;
  }
  /* Do not rewrite callable/subroutine programs or occupied c223. */
  if(op==25||op==26||op==28||op==30)return 0;
  if(op!=65534){
   for(U j=1;j<=len;j++)if((in[p+j]&0x70001800u)==0x20000000u&&(in[p+j]&2047)==223)return 0;
   if(len&&((in[p+1]&0x70001800u)==0x00000800u)&&(in[p+1]&2047)==0&&(in[p+1]&0x70000))writes=1;
   for(U j=0;j<=len;j++)character_words[n++]=in[p+j];
  }
  p+=len+1;
 }
 return 0;
}
static void character_cleanup(P o){
 State*t=state(o);if(t){com_release(t->cm_surf);com_release(t->cm_tex);t->cm_surf=t->cm_tex=0;t->cm_ready=t->cm_w=t->cm_h=0;}
 for(U i=0;i<character_count;i++)if(character_shaders[i].device==o){
  com_release(character_shaders[i].source);com_release(character_shaders[i].tint);
  character_shaders[i].device=character_shaders[i].source=character_shaders[i].tint=0;
 }
}
static P character_variant(P o,P ps,U role){
 U i=0,free=128;
 for(;i<character_count;i++){
  if(!character_shaders[i].device&&free==128)free=i;
  if(character_shaders[i].device==o&&character_shaders[i].source==ps&&character_shaders[i].role==role)break;
 }
 if(i==character_count){
  if(free<128)i=free;else if(character_count<128)i=character_count++;else{return 0;}
  CharacterShader*c=&character_shaders[i];c->device=o;c->source=ps;c->role=role;
  ((U(API*)(P))(*(P**)ps)[1])(ps);
  U bytes=ol_read(ps),words=bytes?character_transform(ol_input,bytes,role):0;I hr=-1;
  if(words)hr=((I(API*)(P,const U*,P*))orig(o,106))(o,character_words,&c->tint);
  if(hr<0){com_release(c->tint);c->tint=0;}
  begin("Character preview shader");kv("role",role);kv("words",words);hx("result",hr);emit();
 }
 return character_shaders[i].tint;
}
static P character_before(P o){
 State*t=state(o);if(!character_preview||!shadow_image||!t||!t->main||t->shadow_pass)return 0;
 P vs=0;U role=0;
 if(((I(API*)(P,P*))orig(o,93))(o,&vs)>=0&&vs){
  for(U i=0;i<bl_id_count;i++)if(bl_ids[i].object==vs){role=bl_ids[i].character;break;}
 }
 com_release(vs);if(!role)return 0;
 P ps=0;if(((I(API*)(P,P*))orig(o,108))(o,&ps)<0||!ps)return 0;
 P replacement=character_variant(o,ps,role);
 if(!replacement||((I(API*)(P,P))orig(o,107))(o,replacement)<0){com_release(ps);return 0;}
 if(++character_draws==1){begin("Character preview active: cyan candidates, amber ambiguous");emit();}
 return ps;
}
static void character_after(P o,P saved){if(saved){((I(API*)(P,P))orig(o,107))(o,saved);com_release(saved);}}
