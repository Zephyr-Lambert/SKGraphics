/* Configuration integration: missing keys, minimal and explicit settings. */
#define _start old_mock_entry
#include "mock.c"
#undef _start
static I weq(const W*w,const char*s){while(*s&&*w==(W)*s){w++;s++;}return !*s&&!*w;}
static U mode;
static U options(const W*a,const W*b,I def,const W*p){
 (void)p;
 if(mode==1&&weq(a,"Rendering")&&weq(b,"Enabled"))return 1;
 if(mode>=2){
  if(weq(a,"Diagnostics")){
   if(weq(b,"Logging"))return mode==2;
   if(weq(b,"SceneProbe")||weq(b,"AOProbe")||weq(b,"ShadowProbe"))return 1;
  }
  if(weq(a,"Rendering")&&weq(b,"Enabled"))return 1;
  if(weq(a,"Antialiasing")&&weq(b,"Method"))return 2;
  if(weq(a,"Shadows")&&weq(b,"Enabled"))return 1;
  if(weq(a,"Shadows")&&weq(b,"Filter"))return 2;
  if(weq(a,"Outlines")&&weq(b,"Enabled"))return 0;
 }
 return def;
}
void _start(void){
 profile_int=options;
 for(mode=0;mode<4;mode++){
  initialized=0;logfile=0;file_creates=writes=events=0;init();
  if(mode==0&&(enabled||aa_method||shadow_enabled||shadow_filter||bloom_enabled||ao_enabled||texture_dump_enabled||texture_replace_enabled||load_reshade||character_preview||ao_depth_view||ao_exclude_characters))fail=40;
  if(mode<2&&(!outline_enabled||outline_thickness!=100||outline_opacity!=100||outline_color_override))fail=41;
  if(mode==1&&(!enabled||renderw!=3840||renderh!=2160||aa_method||shadow_enabled||shadow_filter))fail=42;
  if(mode>=2&&(!enabled||aa_method!=2||!shadow_enabled||shadow_filter!=2||outline_enabled))fail=43;
  if(mode==2){if(!logfile||file_creates!=1||!writes||!scene_probe_enabled||!ao_probe_enabled||!shadow_probe_enabled)fail=44;}
  else if(logfile||file_creates||writes||scene_probe_enabled||ao_probe_enabled||shadow_probe_enabled)fail=45;
 }
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
