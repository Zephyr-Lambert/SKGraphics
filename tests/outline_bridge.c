#include "filter_bridge.c"

UINT outline_transform_test(const UINT *source, UINT bytes, UINT vertex, UINT thickness,
                            UINT *out) {
    outline_enabled = 0;
    outline_color_override = 0;
    outline_opacity = 100;
    outline_thickness = thickness;
    UINT words = ol_transform(source, bytes, vertex);
    for (UINT i = 0; i < words; i++) {
        out[i] = ol_output[i];
    }
    return words;
}

UINT outline_creation_test(const UINT *code, UINT bytes, UINT vertex, UINT enabled, UINT thickness,
                           UINT mode) {
    outline_color_override = 0;
    outline_opacity = 100;
    test_code = code;
    test_bytes = bytes;
    test_creates = 0;
    test_mode = mode;
    releases = 0;
    outline_enabled = enabled;
    outline_thickness = thickness;
    shadow_image = exe;
    shadow_filter = 0;
    ol_pixels = ol_vertices = 0;
    test_shader_vt[2] = release0;
    test_shader_vt[4] = test_get_function;
    table_count = 0;
    dev_vt[91] = test_create_shader;
    dev_vt[106] = test_create_shader;
    hook_table_record(&dev, 119);
    PVOID output = 0;
    INT hr = vertex ? outline_vertex_shader(&dev, code, &output)
                    : outline_pixel_shader(&dev, code, &output);
    INT change = vertex ? (enabled && thickness != 100) : !enabled, applied = change && !mode;
    return hr == 0 && output == (applied ? &test_filtered : &test_original) &&
           test_creates == (change ? 2u : 1u) &&
           releases == (change && (mode == 0 || mode == 2) ? 1u : 0u) &&
           ol_pixels + ol_vertices == (UINT)applied;
}

UINT outline_color_test(const UINT *source, UINT bytes, UINT enabled, UINT custom, UINT red,
                        UINT green, UINT blue, UINT opacity, UINT *out) {
    outline_enabled = enabled;
    outline_color_override = custom;
    outline_red = red;
    outline_green = green;
    outline_blue = blue;
    outline_opacity = opacity;
    UINT words = ol_transform(source, bytes, 0);
    for (UINT i = 0; i < words; i++) {
        out[i] = ol_output[i];
    }
    return words;
}
