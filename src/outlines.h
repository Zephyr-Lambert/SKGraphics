/* GOG 01.06 outline-only shader transformations. No game bytecode bundled.
 * Exact hashes exclude comments. Defaults preserve the original shader and game opacity.
 */
static U ol_input[4096],ol_code[4096],ol_output[4096],ol_pixels,ol_vertices,ol_failures;
static const char*ol_pixel_ids[]={
 "d2bdfe540acc09934e0daaac7f53bcd665c2e008a9279c3c412ce8ea5e0fdd31",
 "777fe32149cdec586700d4dc5bc1ef5d23cbc14043b05c401ffb2edeb10b34dc",
 "32538427c48417742a6f01fdcc8ab059ec7cb2c68e14c6f51207318395bba1ef",
 "46347fc709b49ce071519681b3eba7eabd7f2558f8df96fbf8560066a2fd6349"};
static const char*ol_vertex_ids[]={
 "817b0bf44a817f12b498c3f2b352779266d746da5920751edae566dd080ad608",
 "bf658c4048e6425e7a8aa75f7fa4f54c76bafd7a28cf4babacc085f9167d491a",
 "213fce887b2ad4ef68ccc0f879d05f53eff52a40a7e6fbccb7263e3490cb910e",
 "84c299078d921e72f8b9c3cf922377d942876899f9fec39acc818806f6288cfd",
 "7ab0211e9567a687fe551b4b3ef757caa0f43271deecee76edb64a5d1999e7ef",
 "7d28450017031630618001ab9652605c87ca1728aeee808df2ce342667bc8a86"};
static const U ol_discard[]={0xffff0300,0x05000051,0xa00f0000,0xbf800000,0xbf800000,0xbf800000,0xbf800000,0x02000001,0x800f0000,0xa0e40000,0x01000041,0x800f0000,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
static U ol_transform(const U*input,U bytes,U vertex){
 if(bytes<8||bytes>sizeof(ol_input)||bytes%4||input[0]!=(vertex?0xfffe0300u:0xffff0300u))return 0;
 U p=1,n=0,total=bytes/4;I ended=0;
 while(p<total){U op=input[p]&65535,len=op==65534?(input[p]>>16)&32767:(input[p]>>24)&15;
  if(len>total-p-1)return 0;
  if(op!=65534)for(U j=0;j<=len;j++)ol_code[n++]=input[p+j];
  p+=len+1;if(op==65535){ended=1;break;}
 }
 if(!ended||p!=total)return 0;
 char hash[65];texture_hash(ol_code,n*4,hash);U count=vertex?6:4;const char**ids=vertex?ol_vertex_ids:ol_pixel_ids;I matched=0;
 for(U i=0;i<count;i++)if(sh_equal((const unsigned char*)hash,(const unsigned char*)ids[i],64)){matched=1;break;}
 if(!matched)return 0;
 if(!vertex){
  if(!outline_enabled||!outline_opacity){for(U i=0;i<sizeof(ol_discard)/4;i++)ol_output[i]=ol_discard[i];return sizeof(ol_discard)/4;}
  if(!outline_color_override&&outline_opacity==100)return 0;
  /* Replace reads of the outline uniform c165 with an adjusted local r31.
   * Preserve texture masks, fog and game-driven alpha (opacity multiplier).
   * r31/c223 are unused in these exact pixel programs. */
  U used=0,inserted=0,changed=0;ol_output[used++]=0xffff0300;
  ol_output[used++]=0x05000051;ol_output[used++]=0xa00f00df;
  ol_output[used++]=sf_float((float)outline_red/255.0f);
  ol_output[used++]=sf_float((float)outline_green/255.0f);
  ol_output[used++]=sf_float((float)outline_blue/255.0f);
  ol_output[used++]=sf_float((float)outline_opacity/100.0f);
  for(p=0;p<n;){U op=ol_code[p]&65535,len=(ol_code[p]>>24)&15;
   if(!inserted&&op!=81&&op!=31){
    ol_output[used++]=0x02000001;ol_output[used++]=0x800f001f;ol_output[used++]=0xa0e400a5;
    if(outline_color_override){ol_output[used++]=0x02000001;ol_output[used++]=0x8007001f;ol_output[used++]=0xa0e400df;}
    ol_output[used++]=0x03000005;ol_output[used++]=0x8008001f;ol_output[used++]=0x80ff001f;ol_output[used++]=0xa0ff00df;
    inserted=1;
   }
   for(U j=0;j<=len;j++){
    U token=ol_code[p+j];
    if(op!=81&&op!=31&&j>=2&&(token&0x70001800u)==0x20000000u&&(token&2047)==165){token=(token&0x8fff0000u)|31;changed++;}
    ol_output[used++]=token;
   }p+=len+1;
  }
  return inserted&&changed?used:0;
 }
 U used=0,changes=0;ol_output[used++]=0xfffe0300;
 /* c255 and r31 are unused by the six known programs (bones occupy c16..143).
  * Scale only the normal term in XY extrusion. Z/W and game depth bias stay.
  */
 ol_output[used++]=0x05000051;ol_output[used++]=0xa00f00ff;
 ol_output[used++]=sf_float((float)outline_thickness/100.0f);
 ol_output[used++]=0;ol_output[used++]=0;ol_output[used++]=0;
 for(p=0;p<n;){U op=ol_code[p]&65535,len=(ol_code[p]>>24)&15;
  I expand=(op==4||op==5)&&len>=3&&ol_code[p+3]==0xa00000a6&&(ol_code[p+1]&0x000f0000)==0x00030000;
  if(expand){
   if(ol_code[p+2]!=0x80e40000)return 0;
   ol_output[used++]=0x03000005;ol_output[used++]=0x8003001f;ol_output[used++]=ol_code[p+2];ol_output[used++]=0xa00000ff;
   changes++;
  }
  for(U j=0;j<=len;j++)ol_output[used++]=expand&&j==2?0x80e4001f:ol_code[p+j];p+=len+1;
 }
 return changes==1?used:0;
}
static U ol_read(P shader){
 P*v=*(P**)shader;U bytes=0;I hr=((I(API*)(P,P,U*))v[4])(shader,0,&bytes);U size=bytes;
 if(hr>=0&&bytes>=8&&bytes<=sizeof(ol_input)&&!(bytes%4))hr=((I(API*)(P,P,U*))v[4])(shader,ol_input,&size);else hr=-1;
 if(hr<0||size!=bytes){if(ol_failures++<8){begin("Outline shader read failed");kv("bytes",bytes);hx("result",hr);emit();}return 0;}return bytes;
}
static void ol_replace(P device,P*out,U vertex){
 U bytes=ol_read(*out);if(!bytes)return;U words=ol_transform(ol_input,bytes,vertex);if(!words)return;
 P replacement=0;I hr=((I(API*)(P,const U*,P*))orig(device,vertex?91:106))(device,ol_output,&replacement);
 I applied=hr>=0&&replacement;
 if(applied){com_release(*out);*out=replacement;if(vertex)ol_vertices++;else ol_pixels++;}else com_release(replacement);
 begin("Outline shader");kv("vertex",vertex);kv("applied",applied);kv("enabled",outline_enabled);kv("thicknessPercent",outline_thickness);kv("colorOverride",outline_color_override);kv("opacity",outline_opacity);kv("words",words);hx("result",hr);emit();
}
static I API outline_pixel_shader(P o,const U*code,P*out){
 I hr=filtered_pixel_shader(o,code,out);
 if(hr>=0&&out&&*out&&shadow_image&&(!outline_enabled||outline_color_override||outline_opacity!=100))ol_replace(o,out,0);
 return hr;
}
static I API outline_vertex_shader(P o,const U*code,P*out){
 I hr=((I(API*)(P,const U*,P*))orig(o,91))(o,code,out);
 if(hr>=0&&out&&*out&&shadow_image&&outline_enabled&&outline_thickness!=100)ol_replace(o,out,1);
 return hr;
}
