#define _start unused_texture_entry
#include "texture_mock.c"
#undef _start
static PVOID test_vt[119], test_dev = test_vt;
static UINT gets;

static UINT API kind0(PVOID o) {
    (void)o;
    return 3;
}

static INT API bound0(PVOID o, UINT stage, PVOID *out) {
    (void)o;
    if (stage) {
        fail = 81;
    }
    gets++;
    *out = &original_obj;
    return 0;
}

void _start(void) {
    test_vt[64] = bound0;
    hook_table_record(&test_dev, 119);
    device_state_new(&test_dev)->main = 1;
    orig_vt[2] = buffer_vt[2] = release0;
    orig_vt[10] = kind0;
    orig_vt[17] = describe0;
    buffer_vt[3] = buffer_pointer;
    buffer_vt[4] = buffer_size;
    bg_capture(&test_dev, "?");
    if (gets) {
        fail = 82;
    }
    background_probe = scene_probe_enabled = scene_active = 1;
    scene_device = &test_dev;
    bg_capture(&test_dev, "?");
    if (gets != 1 || save_calls != 1 || bg_count != 1 || original_releases ||
        buffer_releases != 1 || bg_bytes != 160) {
        fail = 83;
    }
    bg_capture(&test_dev, "?");
    if (gets != 2 || save_calls != 1 || bg_count != 1 || original_releases != 1) {
        fail = 84;
    }
    bg_capture(&test_dev, "15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7");
    if (gets != 2) {
        fail = 85;
    }
    scene_finish("test");
    if (bg_count || bg_bytes || original_releases != 2 || scene_active) {
        fail = 86;
    }
    scene_active = 1;
    scene_device = &test_dev;
    save_error = 1;
    bg_capture(&test_dev, "?");
    if (bg_bytes || buffer_releases != 2) {
        fail = 87;
    }
    bg_clear();
    if (original_releases != 3) {
        fail = 88;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
