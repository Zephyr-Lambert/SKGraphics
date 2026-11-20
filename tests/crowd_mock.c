#define CPU_SHADOW_TEST_ENTRY unused_cpu_test
#include "cpu_shadow_mock.c"

static void crowd_fixture(void) {
    setup();
    crowd_shadow_test = 1;
    shadow_coverage = 0;
    coverage_reset();
    cs_shader_roles[0] = 4;
}

void _start(void) {
    /* Synthetic CTAB, not game shader bytecode. Parser rejects truncated and
  * incompatible tables before any palette layout is trusted. */
    UINT code[35] = {0};
    code[0] = 0xfffe0300;
    code[1] = (32u << 16) | 65534;
    code[2] = 0x42415443;
    code[34] = 65535;
    unsigned char *b = (unsigned char *)(code + 3);
    sh_put32(b, 28);
    sh_put32(b + 12, 1);
    sh_put32(b + 16, 28);
    unsigned char *e = b + 28;
    sh_put32(e, 48);
    e[4] = 2;
    e[6] = 16;
    e[8] = 128;
    sh_put32(e + 12, 64);
    const char *name = "g_matBone";
    for (UINT i = 0; i < 10; i++) {
        b[48 + i] = (unsigned char)name[i];
    }
    b[64] = 3;
    b[66] = 3;
    b[68] = 4;
    b[70] = 4;
    b[72] = 32;
    if (!cs_crowd_layout(code, sizeof(code))) {
        fail = 170;
    }
    e[6] = 20;
    if (cs_crowd_layout(code, sizeof(code))) {
        fail = 171;
    }
    e[6] = 16;
    e[8] = 64;
    if (cs_crowd_layout(code, sizeof(code))) {
        fail = 172;
    }
    e[8] = 128;
    sh_put32(e, 123);
    if (cs_crowd_layout(code, sizeof(code))) {
        fail = 173;
    }
    sh_put32(e, 48);
    sh_put32(e + 12, 120);
    if (cs_crowd_layout(code, sizeof(code))) {
        fail = 174;
    }
    sh_put32(e + 12, 64);
    sh_put32(b + 12, 0xffffffff);
    if (cs_crowd_layout(code, sizeof(code))) {
        fail = 175;
    }
    sh_put32(b + 12, 1);
    if (cs_crowd_layout(code, 40)) {
        fail = 176;
    }
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
    crowd_fixture();
    crowd_shadow_test = 0;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank]) {
        fail = 177;
    }
    crowd_shadow_test = 1;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 1 || cs_items[cs_bank][0].role != 4) {
        fail = 178;
    }
    /* Shared crowd mesh at distinct world locations must retain both instances. */
    world0[12] = 30;
    cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    if (cs_counts[cs_bank] != 2) {
        fail = 179;
    }
    frame_count = 100;
    cs_present(&dev, 0);
    frame_count++;
    cs_writer = &writerobj;
    cs_rt = &rtobj;
    cs_depth = &dsobj;
    add0(cs_writer);
    add0(cs_rt);
    add0(cs_depth);
    cs_flush(&dev);
    if (draws != 2 || cs_crowd_replayed != 2 || cs_failed) {
        fail = 180;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 181;
    }
    crowd_fixture();
    for (UINT i = 0; i < CS_MAX + 1; i++) {
        world0[12] = (float)i;
        cs_indexed(&dev, 4, 0, 0, 3, 0, 1);
    }
    if (cs_counts[cs_bank] != CS_MAX || !cs_overflow) {
        fail = 182;
    }
    cs_present(&dev, 0);
    if (cs_ready || cs_counts[0] || cs_counts[1]) {
        fail = 183;
    }
    cs_cleanup();
    if (adds != releases) {
        fail = 184;
    }
    /* The crowd shader dump is once per device session: frames keep the flag,
     * only cleanup (Reset/new device) clears it. */
    crowd_fixture();
    cs_crowd_reported = 1;
    cs_present(&dev, 0);
    if (!cs_crowd_reported) {
        fail = 185;
    }
    cs_cleanup();
    if (cs_crowd_reported) {
        fail = 186;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
