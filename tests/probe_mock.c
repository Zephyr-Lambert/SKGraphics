#define _start shadow_tests_unused
#include "shadow_mock.c"
#undef _start
static PVOID mapvt[19], surfacevt[13], shadervt[5],
    mapcolor = mapvt, mapdepth = mapvt, ps = shadervt, vs = shadervt, surfaceobj = surfacevt;
static UINT queries, acquires, bad_iid, big_shader;
static PVOID bound[16], container;

static INT API getcontainer(PVOID o, const UINT *iid, PVOID *out) {
    (void)o;
    const unsigned char want[16] = {0x27, 0x12, 0xc3, 0x85, 0xe5, 0x3d, 0x00, 0x4f,
                                    0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5};
    if (!sh_equal((const unsigned char *)iid, want, 16)) {
        bad_iid = 1;
    }
    *out = container;
    acquires++;
    return 0;
}

static INT API getdesc(PVOID o, UINT level, SurfaceDesc *d) {
    (void)o;
    (void)level;
    d->w = d->h = 4096;
    d->format = 22;
    return 0;
}

static INT API gettex(PVOID o, UINT stage, PVOID *out) {
    (void)o;
    queries++;
    *out = bound[stage];
    if (*out) {
        acquires++;
    }
    return 0;
}

static INT API getps(PVOID o, PVOID *out) {
    (void)o;
    *out = &ps;
    acquires++;
    return 0;
}

static INT API getvs(PVOID o, PVOID *out) {
    (void)o;
    *out = &vs;
    acquires++;
    return 0;
}

static INT API getcode(PVOID o, PVOID data, UINT *size) {
    if (big_shader) {
        *size = 32768;
        return 0;
    }
    *size = 8;
    if (data) {
        UINT *p = data;
        p[0] = o == &ps ? 0xffff0300 : 0xfffe0300;
        p[1] = 65535;
    }
    return 0;
}

static INT API getrs(PVOID o, UINT type, UINT *out) {
    (void)o;
    (void)type;
    *out = 0;
    return 0;
}

static INT API getsamp(PVOID o, UINT stage, UINT type, UINT *out) {
    (void)o;
    (void)stage;
    (void)type;
    *out = 2;
    return 0;
}

void _start(void) {
    dev_vt[64] = gettex;
    dev_vt[108] = getps;
    dev_vt[93] = getvs;
    dev_vt[58] = getrs;
    dev_vt[68] = getsamp;
    hook_table_record(&dev, 119);
    mapvt[2] = release0;
    mapvt[17] = getdesc;
    surfacevt[11] = getcontainer;
    shadervt[2] = release0;
    shadervt[4] = getcode;
    DeviceState *t = device_state_new(&dev);
    shadow_probe_enabled = 1;
    log_limit = 20000;
    frame_count = 60;
    probe_draw(&dev);
    if (queries) {
        fail = 20;
    }
    container = &mapcolor;
    probe_surface(t, &surfaceobj, 0);
    probe_surface(t, &surfaceobj, 0);
    container = &mapdepth;
    probe_surface(t, &surfaceobj, 1);
    if (bad_iid || t->probe_color != &mapcolor || t->probe_depth != &mapdepth) {
        fail = 21;
    }
    probe_draw(&dev);
    if (queries != 16 || probe_codes || probe_pairs_count) {
        fail = 22;
    }
    t->shadow_pass = 1;
    probe_draw(&dev);
    if (probe_codes != 2 || probe_pairs_count != 1 || probe_pairs[0][2] != 1) {
        fail = 23;
    }
    probe_draw(&dev);
    if (probe_pairs_count != 1 || probe_codes != 2) {
        fail = 24;
    }
    t->shadow_pass = 0;
    bound[3] = &mapcolor;
    bound[7] = &mapdepth;
    probe_draw(&dev);
    if (probe_pairs_count != 2 || probe_pairs[1][2] != 16 || probe_pairs[1][3] != 128) {
        fail = 25;
    }
    UINT before = queries;
    frame_count = 61;
    probe_draw(&dev);
    shadow_probe_enabled = 0;
    frame_count = 120;
    probe_draw(&dev);
    if (queries != before) {
        fail = 26;
    }
    shadow_probe_enabled = 1;
    t->probe_checks = 100000;
    probe_draw(&dev);
    if (queries != before) {
        fail = 27;
    }
    big_shader = 1;
    if (probe_shader(&ps, 0) != 0xffffffff) {
        fail = 28;
    }
    probe_cleanup(t);
    if (t->probe_color || t->probe_depth || t->shadow_pass || acquires != releases) {
        fail = 29;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
