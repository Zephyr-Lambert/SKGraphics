/* Host-only malformed shader input stress test; no game shaders included. */
#define _start old_mock_entry
#include "mock.c"
#undef _start
#include <stdlib.h>
static U seed=0x31415926;
static U nextword(void){seed=seed*1664525u+1013904223u;return seed;}
int main(void){
 for(U test=0;test<20000;test++){
  U count=2+nextword()%4095;U*data=malloc(count*4);if(!data)return 2;
  for(U i=0;i<count;i++)data[i]=nextword();
  data[0]=test&1?0xffff0300u:0xfffe0300u;
  if(test%3==0){for(U i=1;i<count;i++)data[i]=0;data[count-1]=65535;}
  if(test%5==0)data[1]=0x7fffFFFEu;
  sf_transform(data,count*4);ol_transform(data,count*4,test&1?0:1);
  character_transform(data,count*4,1+test%4);free(data);
 }
 return 0;
}
