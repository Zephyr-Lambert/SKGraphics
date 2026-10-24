/* Verified GOG/Steam shadow-size patches. All addresses are RVAs in the loaded EXE.
 * Only two PUSH imm32 operands change. The game receives the new dimensions
 * in CShadowMap::Init, stores them and creates both shadow resources itself.
 * Build profiles also select texture imports and scene-boundary call sites.
 * No on-disk EXE writes, detours, shader substitution or blanket RT resizing.
 */
static U shadow_enabled=1,shadow_size=4096,shadow_actual=2048;
static I shadow_attempted,shadow_patched;
static unsigned char*shadow_image;
static const unsigned char shadow_site[]={0x68,0,8,0,0,0x68,0,8,0,0,0x6a,0,0xe8,0x4a,0x73,0xfc,0xff};
static U sh_u32(const unsigned char*p){return (U)p[0]|((U)p[1]<<8)|((U)p[2]<<16)|((U)p[3]<<24);}
static void sh_put32(unsigned char*p,U n){for(U i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static I sh_equal(const unsigned char*a,const unsigned char*b,U count){for(U i=0;i<count;i++)if(a[i]!=b[i])return 0;return 1;}
static I shadow_valid_size(U n){return n==1024||n==2048||n==4096||n==8192||n==12288;}
typedef struct {
 const char*name;U timestamp,image_size,shadow_rva,binding_rva,texture_iat;
 U fade_rva,copy_rva,scene_copy_rva;const unsigned char*site;U site_size;
} GameBuild;
static const unsigned char steam_shadow_site[]={0x68,0,8,0,0,0x68,0,8,0,0,0x8b,0xc8,0xff,0x52,4};
static const GameBuild game_gog={"GOG 01.06",0x62f1e445,0xe4e000,0x54845,0x1b985,0x3463b8,0xd6ef,0x18a03,0xe4bb,shadow_site,sizeof(shadow_site)};
static const GameBuild game_steam={"Steam 01.06",0x62f1e22e,0xe63000,0x1b0cb1,0x1b0a1e,0x351374,0x1a40bb,0x1ae973,0x1a4ed1,steam_shadow_site,sizeof(steam_shadow_site)};
static const GameBuild*game_build=&game_gog;
static I game_reported;
static I shadow_identify(unsigned char*b){
 if(!b||b[0]!='M'||b[1]!='Z')return 0;
 U pe=sh_u32(b+60);if(pe<64||pe>0x1000)return 0;
 if(sh_u32(b+pe)!=0x4550||b[pe+4]!=0x4c||b[pe+5]!=1||b[pe+24]!=0x0b||b[pe+25]!=1)return 0;
 const GameBuild*g=0;U stamp=sh_u32(b+pe+8),size=sh_u32(b+pe+24+56);
 if(stamp==game_gog.timestamp&&size==game_gog.image_size)g=&game_gog;
 else if(stamp==game_steam.timestamp&&size==game_steam.image_size)g=&game_steam;
 if(!g||!sh_equal(b+g->shadow_rva,g->site,g->site_size))return 0;
 if(g==&game_steam){
  /* Loaded code, not protected on-disk bytes. Confirm each mapped D3D call.
   * These sequences contain no absolute addresses and survive relocation. */
  static const unsigned char setrt[]={0xff,0x91,0x94,0,0,0};
  static const unsigned char draw[]={0xff,0x91,0x48,1,0,0};
  if(!sh_equal(b+g->binding_rva-6,setrt,6)||!sh_equal(b+g->fade_rva-6,draw,6)||
     !sh_equal(b+g->copy_rva-6,draw,6)||!sh_equal(b+g->scene_copy_rva-6,draw,6))return 0;
 }
 game_build=g;
 if(!game_reported){begin("Supported executable identified");s(" build=");s(g->name);hx("timestamp",stamp);emit();game_reported=1;}
 return 1;
}
static I shadow_write(unsigned char*b,U size){
 unsigned char*p=b+game_build->shadow_rva;U old,unused;
 if(!sh_equal(p,game_build->site,game_build->site_size))return 0;
 if(!VirtualProtect(p,game_build->site_size,0x40,&old)){begin("Shadow patch skipped: page protection failed");emit();return 0;}
 sh_put32(p+1,size);sh_put32(p+6,size);
 if(!FlushInstructionCache(GetCurrentProcess(),p,game_build->site_size)){
  sh_put32(p+1,2048);sh_put32(p+6,2048);FlushInstructionCache(GetCurrentProcess(),p,game_build->site_size);
  if(!VirtualProtect(p,game_build->site_size,old,&unused)){begin("Shadow patch ERROR: page protection restoration failed");emit();}
  begin("Shadow patch skipped: instruction cache flush failed; original operands restored");emit();return 0;
 }
 if(!VirtualProtect(p,game_build->site_size,old,&unused)){
  /* The writable mapping remains ours, so undo operands before a retry. */
  sh_put32(p+1,2048);sh_put32(p+6,2048);FlushInstructionCache(GetCurrentProcess(),p,game_build->site_size);
  if(!VirtualProtect(p,game_build->site_size,old,&unused)){begin("Shadow patch ERROR: page protection restoration failed");emit();}
  begin("Shadow patch skipped: could not restore page protection; original operands restored");emit();return 0;
 }
 return 1;
}
static void shadow_setup(P o){
 if(shadow_attempted)return;shadow_attempted=1;
 begin("Shadow configuration");kv("enabled",shadow_enabled);kv("requestedSize",shadow_size);kv("filter",shadow_filter);kv("filterRadiusHundredths",shadow_filter_radius);emit();
 unsigned char*b=GetModuleHandleW(0);
 if(!shadow_identify(b)){begin("Shadow patch/probe skipped: executable signature does not match a supported build");emit();return;}
 shadow_image=b;
 if(!shadow_enabled||shadow_size==2048)return;
 if(!shadow_valid_size(shadow_size)){begin("Shadow patch skipped: use 1024, 2048, 4096, 8192 or 12288");emit();return;}
 /* D3DCAPS9: MaxTextureWidth / MaxTextureHeight are DWORDs 22 / 23.
  * Buffer exceeds the complete structure; no version-specific SDK dependency. */
 U caps[128]={0};I hr=((I(API*)(P,P))orig(o,7))(o,caps);
 if(hr<0||caps[22]<shadow_size||caps[23]<shadow_size){begin("Shadow patch skipped: device texture limits");kv("maxWidth",caps[22]);kv("maxHeight",caps[23]);hx("result",hr);emit();return;}
 /* Preflight BOTH resources together, including their level-zero surfaces.
  * Release before the game performs the real allocations. Not a permanent
  * memory guarantee; later runtime allocation failures are still logged. */
 P tex[2]={0},surfaces[2]={0};U formats[2]={75,22},usage[2]={2,1};I ok=1;
 for(U i=0;i<2&&ok;i++){
  hr=((I(API*)(P,U,U,U,U,U,U,P*,P))orig(o,23))(o,shadow_size,shadow_size,1,usage[i],formats[i],0,&tex[i],0);
  if(hr>=0&&tex[i]){P*v=*(P**)tex[i];hr=((I(API*)(P,U,P*))v[18])(tex[i],0,&surfaces[i]);}
  ok=hr>=0&&tex[i]&&surfaces[i];
  begin("Shadow allocation preflight");kv("size",shadow_size);kv("format",formats[i]);hx("result",hr);kv("success",ok);emit();
 }
 for(U i=0;i<2;i++){com_release(surfaces[i]);com_release(tex[i]);}
 if(!ok){begin("Shadow patch skipped: preflight failed; using original 2048 size");emit();return;}
 if(!shadow_write(b,shadow_size))return;
 shadow_image=b;shadow_actual=shadow_size;shadow_patched=1;
 begin("Shadow size patch applied");kv("originalSize",2048);kv("newSize",shadow_actual);hx("siteRVA",game_build->shadow_rva);emit();
}
static I shadow_binding(P caller){return shadow_image&&caller==(P)(shadow_image+game_build->binding_rva);}
static void shadow_draw_sample(P o,I hr){
 if(!logfile||events>=limit)return;
 State*t=state(o);if(!t||!t->shadow_pass)return;
 if(hr>=0&&t->shadow_sampled&&frames-t->shadow_frame<600)return;
 t->shadow_sampled=1;t->shadow_frame=frames;
 VP vp={0};Rect rect={0};I v=((I(API*)(P,VP*))orig(o,48))(o,&vp);I sc=((I(API*)(P,Rect*))orig(o,76))(o,&rect);
 begin("Shadow draw actual");kv("mapSize",shadow_actual);kv("viewportWidth",vp.w);kv("viewportHeight",vp.h);kv("scissorRight",rect.right);kv("scissorBottom",rect.bottom);hx("drawResult",hr);hx("viewportResult",v);hx("scissorResult",sc);emit();
}
