/* Test bridge: source shaders are supplied externally, never bundled. */
#define _start shadow_tests_unused
#include "shadow_mock.c"
#undef _start
U filter_transform_test(const U*source,U bytes,U quality,U radius,U size,U*out){
 shadow_filter=quality;shadow_filter_radius=radius;shadow_actual=size;
 U words=sf_transform(source,bytes);for(U i=0;i<words;i++)out[i]=sf_output[i];return words;
}
static const U*test_code;static U test_bytes,test_creates,test_mode;
static P test_shader_vt[5],test_original=test_shader_vt,test_filtered=test_shader_vt;
static I API test_get_function(P o,P out,U*bytes){(void)o;if(!out){*bytes=test_bytes;return 0;}if(*bytes<test_bytes)return -1;/* Deliberately leave capacity unchanged on copy. */for(U i=0;i<test_bytes/4;i++)((U*)out)[i]=test_code[i];return 0;}
static I API test_create_shader(P o,const U*code,P*out){(void)o;(void)code;test_creates++;if(test_creates==1){*out=&test_original;return 0;}*out=test_mode==1?0:&test_filtered;return test_mode?-1:0;}
U filter_fallback_test(const U*code,U bytes,U mode){
 test_code=code;test_bytes=bytes;test_creates=0;test_mode=mode==3?0:mode;releases=0;sf_matches=0;shadow_filter=mode==3?0:1;shadow_image=exe;
 test_shader_vt[2]=release0;test_shader_vt[4]=test_get_function;table_count=0;dev_vt[106]=test_create_shader;record(&dev,119);
 P output=0;I hr=filtered_pixel_shader(&dev,code,&output);
 return hr==0&&output==(mode==0?&test_filtered:&test_original)&&test_creates==(mode==3?1u:2u)&&releases==(mode==0||mode==2?1u:0u)&&sf_matches==(mode==0?1u:0u);
}
