#define _start unused_texture_entry
#include "texture_mock.c"
#undef _start
static PVOID test_vt[119], test_dev = test_vt;
static UINT gets, kind = 3;

static UINT API kind0(PVOID o) {
    (void)o;
    return kind;
}

static INT API bound0(PVOID o, UINT stage, PVOID *out) {
    (void)o;
    if (stage) {
        fail = 71;
    }
    gets++;
    *out = &original_obj;
    return 0;
}

void _start(void) {
    test_vt[64] = bound0;
    hook_table_record(&test_dev, 119);
    orig_vt[2] = buffer_vt[2] = release0;
    orig_vt[10] = kind0;
    orig_vt[17] = describe0;
    buffer_vt[3] = buffer_pointer;
    buffer_vt[4] = buffer_size;
    circle_texture_capture(&test_dev);
    if (gets) {
        fail = 72;
    }
    scene_probe_enabled = scene_active = 1;
    scene_device = &test_dev;
    scene_captures = 1;
    circle_texture_capture(&test_dev);
    if (gets != 1 || save_calls != 1 || original_releases != 1 || buffer_releases != 1 ||
        circle_capture_bytes != 160) {
        fail = 73;
    }
    circle_capture_reads = 64;
    circle_texture_capture(&test_dev);
    if (gets != 1) {
        fail = 74;
    }
    scene_captures = 2;
    kind = 4;
    circle_texture_capture(&test_dev);
    if (gets != 2 || save_calls != 1 || original_releases != 2 || circle_capture_bytes) {
        fail = 75;
    }
    kind = 3;
    save_error = 1;
    circle_texture_capture(&test_dev);
    if (original_releases != 3 || buffer_releases != 2 || circle_capture_bytes) {
        fail = 76;
    }
    save_error = 0;
    limit_error = 1;
    circle_texture_capture(&test_dev);
    if (original_releases != 4 || buffer_releases != 3 || circle_capture_bytes) {
        fail = 77;
    }
    scene_active = 0;
    circle_texture_capture(&test_dev);
    if (gets != 4) {
        fail = 78;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
