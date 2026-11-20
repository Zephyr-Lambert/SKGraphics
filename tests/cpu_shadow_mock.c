#define _start unused_shadow_entry
#include "shadow_mock.c"
#undef _start
static PVOID bv[14], iv[14], dv[3], sv[6], pv[3];
static PVOID vbobj = bv, ibobj = iv, decobj = dv, saveobj = sv, writerobj = sv, vshader = pv,
             rtobj = pv, dsobj = pv;
static PVOID negative_vt[5], negative_obj = negative_vt, returned_shader;
static UINT shader_queries;

static INT API negative_code(PVOID o, PVOID out, UINT *bytes) {
    (void)o;
    shader_queries++;
    if (out) {
        UINT *p = out;
        p[0] = 0xfffe0300;
        p[1] = 0xffff;
    }
    *bytes = 8;
    return 0;
}

static PVOID bound_vb, bound_ib, bound_decl;
static UINT gets, adds, draws, applies, dynamic_buffer, set_failure, draw_failure, restore_failure;
static float world0[16], bone0[512], boundbones[512], boundlight[16];

static UINT API add0(PVOID p) {
    (void)p;
    adds++;
    return 2;
}

static INT API source0(PVOID o, UINT s, PVOID *p, UINT *off, UINT *stride) {
    (void)o;
    gets++;
    *p = s ? 0 : &vbobj;
    *off = 0;
    *stride = 28;
    if (*p) {
        add0(*p);
    }
    return 0;
}

static INT API indices0(PVOID o, PVOID *p) {
    (void)o;
    *p = &ibobj;
    add0(*p);
    return 0;
}

static INT API decl0(PVOID o, PVOID *p) {
    (void)o;
    *p = &decobj;
    add0(*p);
    return 0;
}

static INT API frequency0(PVOID o, UINT s, UINT *p) {
    (void)o;
    (void)s;
    *p = 1;
    return 0;
}

static INT API desc0(PVOID o, PVOID p) {
    UINT *d = p;
    for (UINT i = 0; i < 6; i++) {
        d[i] = 0;
    }
    d[0] = o == &ibobj ? 101 : 0;
    d[2] = dynamic_buffer ? 0x200 : 0;
    d[4] = 1024;
    return 0;
}

static INT API const0(PVOID o, UINT first, float *out, UINT count) {
    (void)o;
    if (first == 0 && count == 4) {
        for (UINT i = 0; i < 16; i++) {
            out[i] = world0[i];
        }
        return 0;
    }
    if (first == 16 && count == 128) {
        for (UINT i = 0; i < 512; i++) {
            out[i] = bone0[i];
        }
        return 0;
    }
    return -1;
}

static INT API vs0(PVOID o, PVOID *p) {
    (void)o;
    *p = returned_shader ? returned_shader : &vshader;
    add0(*p);
    return 0;
}

static INT API rt0(PVOID o, UINT n, PVOID *p) {
    (void)o;
    (void)n;
    *p = &rtobj;
    add0(*p);
    return 0;
}

static INT API ds0(PVOID o, PVOID *p) {
    (void)o;
    *p = &dsobj;
    add0(*p);
    return 0;
}

static INT API block0(PVOID o, UINT n, PVOID *p) {
    (void)o;
    if (n != 1) {
        fail = 101;
    }
    *p = &saveobj;
    add0(*p);
    return 0;
}

static INT API apply0(PVOID p) {
    applies++;
    if (p == &saveobj) {
        bound_vb = bound_ib = bound_decl = 0;
        return restore_failure ? -1 : 0;
    }
    return 0;
}

static INT API texture_none(PVOID o, UINT s, PVOID p) {
    (void)o;
    (void)s;
    if (p) {
        fail = 102;
    }
    return 0;
}

static INT API setdecl0(PVOID o, PVOID p) {
    (void)o;
    bound_decl = p;
    return 0;
}

static INT API setsource0(PVOID o, UINT s, PVOID p, UINT off, UINT stride) {
    (void)o;
    if (s || off || stride != 28) {
        fail = 103;
    }
    bound_vb = p;
    return set_failure ? -1 : 0;
}

static INT API setindex0(PVOID o, PVOID p) {
    (void)o;
    bound_ib = p;
    return 0;
}

static INT API setconst0(PVOID o, UINT first, const float *p, UINT count) {
    (void)o;
    if (first == 16 && count == 128) {
        for (UINT i = 0; i < 512; i++) {
            boundbones[i] = p[i];
        }
        return 0;
    }
    if (first == 144 && count == 4) {
        for (UINT i = 0; i < 16; i++) {
            boundlight[i] = p[i];
        }
        return 0;
    }
    fail = 104;
    return -1;
}

static INT API draw0(PVOID o, UINT type, INT base, UINT min, UINT count, UINT start, UINT prim) {
    (void)o;
    if (type != 4 || base || min || count != 3 || start || prim != 1 || bound_vb != &vbobj ||
        bound_ib != &ibobj || bound_decl != &decobj) {
        fail = 105;
    }
    draws++;
    return draw_failure ? -1 : 0;
}

static void identity(float *p) {
    for (UINT i = 0; i < 16; i++) {
        p[i] = i % 5 == 0 ? 1 : 0;
    }
}

static void setup(void) {
    cs_cleanup();
    cpu_shadow_test = 1;
    shadow_image = exe;
    cs_device = &dev;
    cs_writer = &writerobj;
    cs_rt = &rtobj;
    cs_depth = &dsobj;
    add0(cs_writer);
    add0(cs_rt);
    add0(cs_depth);
    identity(cs_light);
    cs_shader_refs[0] = &vshader;
    cs_shader_roles[0] = 1;
    cs_shader_count = 1;
    add0(&vshader);
    identity(world0);
    world0[12] = 20;
    for (UINT i = 0; i < 512; i++) {
        bone0[i] = (float)i;
    }
    cs_failed = 0;
    draws = applies = 0;
    cs_replayed = cs_skipped = 0;
    DeviceState *t = device_state(&dev);
    t->main = 1;
    t->shadow_pass = 0;
    t->aa_busy = 0;
    dynamic_buffer = set_failure = draw_failure = restore_failure = 0;
}
#ifndef CPU_SHADOW_TEST_ENTRY
#define CPU_SHADOW_TEST_ENTRY _start
#endif
void CPU_SHADOW_TEST_ENTRY(void) {
    dev_vt[101] = source0;
    dev_vt[105] = indices0;
    dev_vt[88] = decl0;
    dev_vt[103] = frequency0;
    dev_vt[95] = const0;
    dev_vt[93] = vs0;
    dev_vt[38] = rt0;
    dev_vt[40] = ds0;
    dev_vt[59] = block0;
    dev_vt[65] = texture_none;
    dev_vt[87] = setdecl0;
    dev_vt[100] = setsource0;
    dev_vt[104] = setindex0;
    dev_vt[94] = setconst0;
    dev_vt[82] = draw0;
    bv[1] = iv[1] = dv[1] = sv[1] = pv[1] = add0;
    bv[2] = iv[2] = dv[2] = sv[2] = pv[2] = release0;
    bv[13] = iv[13] = desc0;
    sv[5] = apply0;
    hook_table_record(&dev, 119);
    device_state_new(&dev);
    setup();
    negative_vt[1] = add0;
    negative_vt[2] = release0;
    negative_vt[4] = negative_code;
    returned_shader = &negative_obj;
    if (cs_role(&dev) != 0 || shader_queries != 2) {
        fail = 130;
    }
    if (cs_role(&dev) != 0 || shader_queries != 2 || cs_shader_count != 2) {
        fail = 131;
    }
    returned_shader = 0;
    float w[16], l[16], back[16], projected[16];
    identity(w);
    w[0] = 0;
    w[1] = 2;
    w[4] = -3;
    w[5] = 0;
    w[10] = 4;
    w[12] = 100;
    w[13] = -20;
    identity(l);
    l[0] = .001f;
    l[5] = .002f;
    l[10] = .003f;
    l[12] = .1f;
    l[13] = .2f;
    l[14] = .5f;
    cs_multiply(w, l, projected);
    if (!cs_light_matrix(w, projected, back)) {
        fail = 106;
    }
    for (UINT i = 0; i < 16; i++) {
        if (cs_abs(back[i] - l[i]) > 0.00001f) {
            fail = 107;
        }
    }
    w[10] = 0;
    if (cs_light_matrix(w, projected, back)) {
        fail = 108;
    }
    cpu_shadow_test = 0;
    UINT before = gets;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (gets != before) {
        fail = 109;
    }
    cpu_shadow_test = 1;
    cs_shader_roles[0] = 0;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank]) {
        fail = 110;
    }
    cs_shader_roles[0] = 1;
    dynamic_buffer = 1;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank]) {
        fail = 111;
    }
    dynamic_buffer = 0;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 1) {
        fail = 112;
    }
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 1) {
        fail = 113; /* duplicate */
    }
    /* Exact player identity is excluded, shared mesh at another world is not. */
    CSItem player;
    if (!cs_geometry(&dev, &player, 0, 0, 3, 0, 1)) {
        fail = 114;
    }
    cs_players[cs_player_count++] = player;
    cs_drop_bank(cs_bank);
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank]) {
        fail = 115;
    }
    world0[12] = 50;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 1) {
        fail = 116;
    }
    frame_count = 100;
    cs_flush(&dev);
    if (draws) {
        fail = 117;
    }
    cs_present(&dev, 0);
    frame_count++;
    cs_writer = &writerobj;
    cs_rt = &rtobj;
    cs_depth = &dsobj;
    add0(cs_writer);
    add0(cs_rt);
    add0(cs_depth);
    bone0[0] = 999;
    cs_flush(&dev);
    if (draws != 1 || boundbones[0] != 0 || boundlight[12] != 50 || bound_vb || bound_ib ||
        bound_decl || applies != 2 || cs_failed) {
        fail = 118;
    }
    cs_flush(&dev);
    if (draws != 1) {
        fail = 119;
    }
    cs_present(&dev, 0);
    frame_count++;
    if (cs_counts[0] || cs_counts[1]) {
        fail = 120;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 121;
    }
    /* A failed bind still restores state and disables further replay. */
    setup();
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    cs_present(&dev, 0);
    frame_count++;
    cs_writer = &writerobj;
    cs_rt = &rtobj;
    cs_depth = &dsobj;
    add0(cs_writer);
    add0(cs_rt);
    add0(cs_depth);
    set_failure = 1;
    cs_flush(&dev);
    if (!cs_failed || draws || applies != 2 || bound_vb) {
        fail = 122;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 123;
    }
    /* Failed draw and failed state restoration both stop future replays. */
    for (UINT mode = 0; mode < 2; mode++) {
        setup();
        cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
        cs_present(&dev, 0);
        frame_count++;
        cs_writer = &writerobj;
        cs_rt = &rtobj;
        cs_depth = &dsobj;
        add0(cs_writer);
        add0(cs_rt);
        add0(cs_depth);
        draw_failure = !mode;
        restore_failure = mode;
        cs_flush(&dev);
        if (!cs_failed || draws != 1 || applies != 2) {
            fail = 128;
        }
        cs_cleanup();
        if (adds != releases) {
            fail = 129;
        }
    }
    /* Collection-only mode keeps the snapshots but performs no replay or
  * writer Apply, isolating collection cost from shadow drawing. */
    setup();
    cpu_shadow_test = 2;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 1) {
        fail = 132;
    }
    cs_present(&dev, 0);
    frame_count++;
    cs_writer = &writerobj;
    cs_rt = &rtobj;
    cs_depth = &dsobj;
    add0(cs_writer);
    add0(cs_rt);
    add0(cs_depth);
    cs_flush(&dev);
    if (draws || applies || cs_failed) {
        fail = 133;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 134;
    }
    /* Lost presentation and overflow discard pending snapshots. */
    setup();
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    cs_present(&dev, -1);
    if (cs_ready || cs_counts[0] || cs_counts[1]) {
        fail = 124;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 125;
    }
    setup();
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    cs_overflow = 1;
    cs_present(&dev, 0);
    if (cs_ready || cs_counts[0] || cs_counts[1]) {
        fail = 126;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 127;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
