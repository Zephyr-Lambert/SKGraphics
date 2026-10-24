#define TEXTURE_TEST_ENTRY original_texture_tests_unused
#include "texture_mock.c"
static void steam_fixture(void){
 fixture();texture_ready=0;game_build=&game_gog;
 for(U i=0;i<sizeof(fake_exe);i++)fake_exe[i]=0;
 fake_exe[0]='M';fake_exe[1]='Z';sh_put32(fake_exe+60,0x100);
 sh_put32(fake_exe+0x100,0x4550);fake_exe[0x104]=0x4c;fake_exe[0x105]=1;
 sh_put32(fake_exe+0x108,0x62f1e22e);fake_exe[0x118]=0x0b;fake_exe[0x119]=1;
 sh_put32(fake_exe+0x150,0xe63000);
 for(U i=0;i<sizeof(steam_shadow_site);i++)fake_exe[0x1b0cb1+i]=steam_shadow_site[i];
 const unsigned char rt[]={0xff,0x91,0x94,0,0,0},draw[]={0xff,0x91,0x48,1,0,0};
 for(U i=0;i<6;i++){
  fake_exe[0x1b0a18+i]=rt[i];fake_exe[0x1a40b5+i]=draw[i];
  fake_exe[0x1ae96d+i]=draw[i];fake_exe[0x1a4ecb+i]=draw[i];
 }
 *(P*)(fake_exe+0x351374)=memory0;
 *(P*)(fake_exe+0x3463b8)=(P)123;
}
void _start(void){
 P*slot=(P*)(fake_exe+0x351374);
 steam_fixture();textures_install();
 if(!texture_ready||game_build!=&game_steam||*slot!=(P)texture_from_memory||protections!=2||*(P*)(fake_exe+0x3463b8)!=(P)123)fail=50;
 textures_install();if(protections!=2||*slot!=(P)texture_from_memory)fail=51;
 steam_fixture();protection_fail=2;textures_install();if(texture_ready||*slot!=(P)memory0)fail=52;
 steam_fixture();*slot=(P)123;textures_install();if(texture_ready||protections||*slot!=(P)123)fail=53;
 steam_fixture();fake_exe[0x1b0cb1]^=1;textures_install();if(texture_ready||protections)fail=54;
 /* A failed early identification can succeed once normal startup exposes code. */
 fake_exe[0x1b0cb1]^=1;textures_install();if(!texture_ready||protections!=2)fail=55;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
