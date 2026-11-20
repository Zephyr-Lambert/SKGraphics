/* Test bridge: source shaders are supplied externally, never bundled. */
#define _start shadow_tests_unused
#include "shadow_mock.c"
#undef _start

UINT filter_transform_test(const UINT *source, UINT bytes, UINT quality, UINT radius, UINT size,
                           UINT *out) {
    shadow_filter = quality;
    shadow_filter_radius = radius;
    shadow_actual = size;
    UINT words = sf_transform(source, bytes);
    for (UINT i = 0; i < words; i++) {
        out[i] = sf_output[i];
    }
    return words;
}

static const UINT *test_code;
static UINT test_bytes, test_creates, test_mode;
static PVOID test_shader_vt[5], test_original = test_shader_vt, test_filtered = test_shader_vt;

static INT API test_get_function(PVOID o, PVOID out, UINT *bytes) {
    (void)o;
    if (!out) {
        *bytes = test_bytes;
        return 0;
    }
    if (*bytes < test_bytes) {
        return -1; /* Deliberately leave capacity unchanged on copy. */
    }
    for (UINT i = 0; i < test_bytes / 4; i++) {
        ((UINT *)out)[i] = test_code[i];
    }
    return 0;
}

static INT API test_create_shader(PVOID o, const UINT *code, PVOID *out) {
    (void)o;
    (void)code;
    test_creates++;
    if (test_creates == 1) {
        *out = &test_original;
        return 0;
    }
    *out = test_mode == 1 ? 0 : &test_filtered;
    return test_mode ? -1 : 0;
}

UINT filter_fallback_test(const UINT *code, UINT bytes, UINT mode) {
    test_code = code;
    test_bytes = bytes;
    test_creates = 0;
    test_mode = mode == 3 ? 0 : mode;
    releases = 0;
    sf_matches = 0;
    shadow_filter = mode == 3 ? 0 : 1;
    shadow_image = exe;
    test_shader_vt[2] = release0;
    test_shader_vt[4] = test_get_function;
    table_count = 0;
    dev_vt[106] = test_create_shader;
    hook_table_record(&dev, 119);
    PVOID output = 0;
    INT hr = filtered_pixel_shader(&dev, code, &output);
    return hr == 0 && output == (mode == 0 ? &test_filtered : &test_original) &&
           test_creates == (mode == 3 ? 1u : 2u) &&
           releases == (mode == 0 || mode == 2 ? 1u : 0u) && sf_matches == (mode == 0 ? 1u : 0u);
}
