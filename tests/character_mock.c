#define _start unused_mock_entry
#include "mock.c"
#undef _start
void _start(void){
 U code[]={0xffff0300,0x02000001,0x800f0800,0xa0e40000,0xffff};
 U n=character_transform(code,sizeof(code),1);
 if(n!=14||character_words[8]!=code[2]||character_words[9]!=code[3]||character_words[11]!=0x80070800||character_words[4]!=0x3f800000)fail=40;
 if(!character_transform(code,sizeof(code),2)||character_words[3]!=0x3f800000||character_words[5]!=0)fail=41;
 code[3]=0xa0e400df;if(character_transform(code,sizeof(code),1))fail=42;
 code[3]=0xa0e40000;if(character_transform(code,sizeof(code)-1,1))fail=43;
 if(character_role("15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7")!=1)fail=44;
 if(character_role("f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1")!=1)fail=45;
 if(character_role("37ddb1d1007174af6cee0b44c6e2d00179f767d7daf731cd68152b25c3de58f2"))fail=46;
 if(character_role("527fcbce9cb0d09a9fe5566a01b770bd986ed9a6b475b3eed9bc4a967a2028e6"))fail=47;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");
 __builtin_unreachable();
}
