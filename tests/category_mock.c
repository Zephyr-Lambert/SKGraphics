#define TEXTURE_TEST_ENTRY texture_test_unused
#include "texture_mock.c"
static const char*hash_a="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
static const char*hash_b="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
static const char*hash_c="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static U section_overflow;
static U wide(W*out,const char*s){U i=0;while(s[i]){out[i]=(W)s[i];i++;}out[i]=0;return i;}
static U names0(W*out,U cap,const W*path0){(void)cap;(void)path0;U n=wide(out,"Menus")+1;n+=wide(out+n,"Characters")+1;out[n]=0;return n;}
static U section0(const W*name,W*out,U cap,const W*path0){(void)path0;if(section_overflow)return cap-2;
 U n=wide(out,hash_a);n+=wide(out+n,".DDS=Shared menu/character asset")+1;
 if(name[0]=='M'){n+=wide(out+n,hash_b);n+=wide(out+n,"=Background")+1;n+=wide(out+n,"bad-hash=Ignored")+1;}
 out[n]=0;return n;
}
static void fresh_categories(void){for(U i=0;i<32768;i++)category_table[i].used=category_table[i].selected=0;category_bad=category_count=category_entries=category_filtered=category_mode=0;category_unknown=1;category_selection[0]=0;section_overflow=0;profile_names=names0;profile_section=section0;}
void _start(void){
 fresh_categories();wide(category_selection," characters , EFFECTS ");category_load((const W[]){0});
 if(category_entries!=2||category_count!=2||category_bad)fail=40;
 category_mode=1;if(!category_allows(hash_a)||category_allows(hash_b)||!category_allows(hash_c))fail=41;
 category_unknown=0;if(category_allows(hash_c))fail=42;
 category_mode=2;if(category_allows(hash_a)||!category_allows(hash_b))fail=43;
 category_mode=0;if(!category_allows(hash_a)||!category_allows(hash_b)||category_allows(hash_c))fail=44;
 W key[100];unsigned char digest[32];wide(key,hash_a);wide(key+64,".png ");if(!category_key(key,69,digest))fail=45;
 key[4]='Z';if(category_key(key,69,digest))fail=46;
 fresh_categories();section_overflow=1;category_load((const W[]){0});if(!category_bad||category_allows(hash_c))fail=47;
 fresh_categories();category_read_options();if(category_bad||category_mode||!category_unknown)fail=48;
 /* Integration: excluded source must still replace, without exporting or
  * consuming any of the dump byte/file allowance. */
 fixture();orig_vt[2]=repl_vt[2]=buffer_vt[2]=release0;orig_vt[17]=describe0;buffer_vt[3]=buffer_pointer;buffer_vt[4]=buffer_size;
 fresh_categories();wide(category_selection,"Menus");category_load((const W[]){0});category_mode=2;dds_exists=1;
 if(run(0,0)!=&replacement_obj||file_calls!=1||save_calls||texture_dumps||texture_bytes||category_filtered!=1)fail=49;
 /* Invalid filter blocks dumping only, never replacement. */
 category_bad=1;if(run(0,0)!=&replacement_obj||file_calls!=2||save_calls)fail=50;
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
