#define _start old_mock_entry
#include "mock.c"
#undef _start
static U queries;
static I API counted_desc(P o,Desc*d){(void)o;(void)d;queries++;return 0;}
void _start(void){
 d3dx_module=0;library_loads=0;if(!d3dx_library()||!d3dx_library()||library_loads!=1)fail=57;
 Table t={0};P slots[4]={0};t.vt=slots;t.count=4;slots[1]=(P)11;
 protect_calls=0;protect_fail_at=2;hook(&t,1,(P)22);
 if(slots[1]!=(P)11||t.hooked[0]||protect_calls!=3)fail=50;
 protect_calls=0;protect_fail_at=0;hook(&t,1,(P)22);
 if(slots[1]!=(P)22||t.hooked[0]!=2)fail=51;
 hook(&t,1,(P)33);if(slots[1]!=(P)22||protect_calls!=2)fail=52;
 sv[12]=counted_desc;logfile=0;seen_count=0;
 surface("test",0,0,&sobj,0);draw_sample(&dobj,0);shadow_draw_sample(&dobj,0);
 if(queries||fresh(1,2,3,4,5,6,7,8)||seen_count)fail=53;
 logfile=(P)3;events=limit;
 surface("test",0,0,&sobj,0);if(queries||fresh(1,2,3,4,5,6,7,8))fail=54;
 if(main_surface(0,&sobj))fail=55;
 used=0;s(0);if(used)fail=56;
 suffix(32768,"x");
 __asm__ volatile("syscall"::"a"(60),"D"((unsigned long)fail):"rcx","r11","memory");__builtin_unreachable();
}
