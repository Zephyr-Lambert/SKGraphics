#include "filter_bridge.c"
U outline_transform_test(const U*source,U bytes,U vertex,U thickness,U*out){outline_enabled=0;outline_color_override=0;outline_opacity=100;outline_thickness=thickness;U words=ol_transform(source,bytes,vertex);for(U i=0;i<words;i++)out[i]=ol_output[i];return words;}
U outline_creation_test(const U*code,U bytes,U vertex,U enabled,U thickness,U mode){
 outline_color_override=0;outline_opacity=100;test_code=code;test_bytes=bytes;test_creates=0;test_mode=mode;releases=0;outline_enabled=enabled;outline_thickness=thickness;shadow_image=exe;shadow_filter=0;ol_pixels=ol_vertices=0;
 test_shader_vt[2]=release0;test_shader_vt[4]=test_get_function;table_count=0;dev_vt[91]=test_create_shader;dev_vt[106]=test_create_shader;record(&dev,119);
 P output=0;I hr=vertex?outline_vertex_shader(&dev,code,&output):outline_pixel_shader(&dev,code,&output);
 I change=vertex?(enabled&&thickness!=100):!enabled,applied=change&&!mode;
 return hr==0&&output==(applied?&test_filtered:&test_original)&&test_creates==(change?2u:1u)&&releases==(change&&(mode==0||mode==2)?1u:0u)&&ol_pixels+ol_vertices==(U)applied;
}

U outline_color_test(const U*source,U bytes,U enabled,U custom,U red,U green,U blue,U opacity,U*out){outline_enabled=enabled;outline_color_override=custom;outline_red=red;outline_green=green;outline_blue=blue;outline_opacity=opacity;U words=ol_transform(source,bytes,0);for(U i=0;i<words;i++)out[i]=ol_output[i];return words;}
