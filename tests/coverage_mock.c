#define CPU_SHADOW_TEST_ENTRY unused_cpu_test
#include "cpu_shadow_mock.c"
static float light0[16];
static UINT light_reads, light_sets, light_get_fail, light_set_fail;

static INT API coverage_get(PVOID o, UINT first, float *out, UINT count) {
    if (first == 144 && count == 4) {
        light_reads++;
        if (light_get_fail) {
            return -1;
        }
        for (UINT i = 0; i < 16; i++) {
            out[i] = light0[i];
        }
        return 0;
    }
    return const0(o, first, out, count);
}

static INT API coverage_set(PVOID o, UINT first, const float *in, UINT count) {
    if (first == 144 && count == 4) {
        light_sets++;
        if (light_set_fail == light_sets) {
            return -1;
        }
        for (UINT i = 0; i < 16; i++) {
            light0[i] = in[i];
        }
        return 0;
    }
    return setconst0(o, first, in, count);
}

static void coverage_fixture(void) {
    setup();
    coverage_reset();
    shadow_coverage = 0;
    light_reads = light_sets = light_get_fail = light_set_fail = 0;
    for (UINT i = 0; i < 16; i++) {
        light0[i] = (float)(i + 1);
    }
}

void _start(void) {
    dev_vt[101] = source0;
    dev_vt[105] = indices0;
    dev_vt[88] = decl0;
    dev_vt[103] = frequency0;
    dev_vt[95] = coverage_get;
    dev_vt[93] = vs0;
    dev_vt[38] = rt0;
    dev_vt[40] = ds0;
    dev_vt[59] = block0;
    dev_vt[65] = texture_none;
    dev_vt[87] = setdecl0;
    dev_vt[100] = setsource0;
    dev_vt[104] = setindex0;
    dev_vt[94] = coverage_set;
    dev_vt[82] = draw0;
    bv[1] = iv[1] = dv[1] = sv[1] = pv[1] = add0;
    bv[2] = iv[2] = dv[2] = sv[2] = pv[2] = release0;
    bv[13] = iv[13] = desc0;
    sv[5] = apply0;
    hook_table_record(&dev, 119);
    device_state_new(&dev);
    coverage_fixture();
    DeviceState *t = device_state(&dev);
    CoverageDraw c;
    coverage_begin(&dev, &c);
    coverage_end(&dev, &c);
    if (light_reads || light_sets) {
        fail = 150;
    }
    for (UINT mode = 1; mode <= 2; mode++) {
        shadow_coverage = mode;
        cs_shader_roles[0] = 2;
        t->shadow_pass = 1;
        coverage_begin(&dev, &c);
        if (!c.changed || coverage_failed) {
            fail = 151;
        }
        for (UINT i = 0; i < 16; i++) {
            float expected = (float)(i + 1) / (i % 4 < 2 ? (float)(mode + 1) : 1.0f);
            if (cs_abs(light0[i] - expected) > .00001f) {
                fail = 152;
            }
        }
        coverage_end(&dev, &c);
        for (UINT i = 0; i < 16; i++) {
            if (light0[i] != (float)(i + 1)) {
                fail = 153;
            }
        }
        cs_shader_roles[0] = 3;
        t->shadow_pass = 0;
        coverage_begin(&dev, &c);
        if (!c.changed || coverage_failed) {
            fail = 154;
        }
        for (UINT i = 0; i < 16; i++) {
            float expected = (float)(i + 1) / (i % 4 < 2 ? (float)(mode + 1) : 1.0f);
            if (cs_abs(light0[i] - expected) > .00001f) {
                fail = 155;
            }
        }
        coverage_end(&dev, &c);
    }
    UINT before = light_sets;
    cs_shader_roles[0] = 1;
    coverage_begin(&dev, &c);
    coverage_end(&dev, &c);
    if (light_sets != before) {
        fail = 156;
    }
    cs_shader_roles[0] = 2;
    coverage_begin(&dev, &c);
    coverage_end(&dev, &c);
    if (light_sets != before) {
        fail = 157; /* writer outside shadow pass */
    }
    cs_shader_roles[0] = 3;
    t->shadow_pass = 1;
    coverage_begin(&dev, &c);
    coverage_end(&dev, &c);
    if (light_sets != before) {
        fail = 158;
    }
    t->shadow_pass = 0;
    light_get_fail = 1;
    coverage_begin(&dev, &c);
    if (!coverage_failed || c.changed || light_sets != before) {
        fail = 159;
    }
    coverage_reset();
    light_get_fail = 0;
    light_set_fail = light_sets + 1;
    coverage_begin(&dev, &c);
    if (!coverage_failed || c.changed) {
        fail = 160;
    }
    for (UINT i = 0; i < 16; i++) {
        if (light0[i] != (float)(i + 1)) {
            fail = 161;
        }
    }
    coverage_fixture();
    shadow_coverage = 1;
    cs_shader_roles[0] = 3;
    light_set_fail = 2;
    coverage_begin(&dev, &c);
    coverage_end(&dev, &c);
    if (!coverage_failed) {
        fail = 162;
    }
    /* The replay's direct original-device draw must get the same scale exactly
  * once; it bypasses the normal game-draw hook. */
    for (UINT mode = 0; mode <= 2; mode++) {
        coverage_fixture();
        shadow_coverage = mode;
        cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
        cs_present(&dev, 0);
        frame_count++;
        cs_writer = &writerobj;
        cs_rt = &rtobj;
        cs_depth = &dsobj;
        add0(cs_writer);
        add0(cs_rt);
        add0(cs_depth);
        cs_flush(&dev);
        if (draws != 1 || cs_failed || cs_abs(light0[12] - 20.0f / (float)(mode + 1)) > .00001f ||
            light0[10] != 1 || light0[15] != 1) {
            fail = 163;
        }
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 164;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
