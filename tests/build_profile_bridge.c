/* Optional tests against user-supplied executables/capture; no game included. */
#define _start mock_unused_entry
#include "mock.c"
#undef _start
I check_build(unsigned char*b){
 game_reported=0;game_build=&game_gog;
 if(!shadow_identify(b))return 0;
 return game_build==&game_steam?2:1;
}
U build_field(U n){
 switch(n){case 0:return game_build->shadow_rva;case 1:return game_build->binding_rva;
 case 2:return game_build->texture_iat;case 3:return game_build->fade_rva;
 case 4:return game_build->copy_rva;case 5:return game_build->scene_copy_rva;}
 return 0;
}
I patch_build(unsigned char*b,U size,U fail_protect){
 protect_calls=0;protect_fail_at=fail_protect;
 return shadow_write(b,size);
}
