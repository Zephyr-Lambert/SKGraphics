#define _start unused_texture_entry
#include "texture_mock.c"
#undef _start
static PVOID test_vt[119], test_dev = test_vt;
static UINT gets;
static unsigned char test_dds[32896];

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

static INT API desc256(PVOID o, UINT level, SurfaceDesc *d) {
    (void)o;
    (void)level;
    d->w = d->h = 256;
    d->format = 827611204;
    return 0;
}

static PVOID API data256(PVOID o) {
    (void)o;
    return test_dds;
}

static UINT API bytes256(PVOID o) {
    (void)o;
    return sizeof(test_dds);
}

void _start(void) {
    for (UINT i = 0; i < 3; i++) {
        if (tb_identify(tb_shader_ids[i], tb_shader_ids) != i + 1 ||
            tb_identify(tb_texture_ids[i], tb_texture_ids) != i + 1) {
            fail = 82;
        }
    }
    if (tb_identify("?", tb_texture_ids) || tb_identify("", tb_texture_ids) ||
        tb_identify(tb_shader_ids[0], tb_texture_ids)) {
        fail = 83;
    }
    test_vt[64] = bound0;
    hook_table_record(&test_dev, 119);
    orig_vt[2] = buffer_vt[2] = release0;
    orig_vt[10] = kind0;
    orig_vt[17] = desc256;
    buffer_vt[3] = data256;
    buffer_vt[4] = bytes256;
    if (tb_receiver(&test_dev, 1) || gets) {
        fail = 84;
    }
    ao_exclude_backgrounds = 1;
    if (tb_receiver(&test_dev, 0) || gets) {
        fail = 85;
    }
    if (tb_receiver(&test_dev, 1) || save_calls != 1 || tb_count != 1 || buffer_releases != 1 ||
        original_releases) {
        fail = 86;
    }
    if (tb_receiver(&test_dev, 1) || save_calls != 1 || original_releases != 1) {
        fail = 87;
    }
    tb_roles[0] = 2;
    if (tb_receiver(&test_dev, 1) || !tb_receiver(&test_dev, 2) || tb_receiver(&test_dev, 3)) {
        fail = 88;
    }
    tb_cleanup();
    if (tb_count || original_releases != 5) {
        fail = 89;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
