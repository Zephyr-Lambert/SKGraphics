#define SCENE_TEST_ENTRY scene_tests_unused
#include "scene_mock.c"
#undef SCENE_TEST_ENTRY
static UINT reads;

static INT API streamget(PVOID o, UINT stream, PVOID *out, UINT *offset, UINT *stride) {
    (void)o;
    reads++;
    *offset = 8;
    *stride = 32;
    *out = stream ? 0 : &rtobj;
    if (*out) {
        acquired++;
    }
    return 0;
}

static INT API allconstants(PVOID o, UINT first, float *out, UINT count) {
    (void)o;
    reads++;
    if (first || count != 256) {
        fail = 41;
    }
    if (constant_fail) {
        return -1;
    }
    for (UINT i = 0; i < count * 4; i++) {
        out[i] = (float)i;
    }
    return 0;
}

void _start(void) {
    dev_vt[101] = streamget;
    dev_vt[105] = psget;
    dev_vt[88] = psget;
    dev_vt[95] = allconstants;
    hook_table_record(&dev, 119);
    rtv[2] = release0;
    psv[2] = release0;
    scene_probe_enabled = 1;
    scene_active = 1;
    scene_device = &dev;
    scene_log = (PVOID)1;
    scene_geometry(&dev, 82, -2, 0, 10, 3, 0);
    if (reads || scene_lines) {
        fail = 42;
    }
    actor_shadow_probe = 1;
    scene_geometry(&dev, 82, -2, 0, 10, 3, 0);
    if (reads != 4 || scene_lines != 2 || acquired != releases) {
        fail = 43;
    }
    scene_geometry(&dev, 84, 0, 0, 10, 0, 32);
    if (reads != 4 || scene_lines != 3 || acquired != releases) {
        fail = 44;
    }
    scene_actor(&dev, &psobj, "unknown");
    if (reads != 5 || scene_lines != 10) {
        fail = 45;
    }
    constant_fail = 1;
    scene_actor(&dev, &psobj, "unknown");
    if (reads != 6 || scene_lines != 12) {
        fail = 46;
    }
    /* Shader capture: read failure does not consume a slot, duplicate shaders
  * emit once, and the per-capture budget bounds work. */
    psv[4] = codeget;
    char hash[65];
    for (UINT i = 0; i < 64; i++) {
        hash[i] = 'a';
    }
    hash[64] = 0;
    UINT before = scene_lines;
    read_mode = 2;
    scene_actor_shader(&psobj, 1, hash);
    if (actor_shader_count || scene_lines != before) {
        fail = 49;
    }
    read_mode = 0;
    scene_actor_shader(&psobj, 1, hash);
    if (actor_shader_count != 1 || scene_lines != before + 2) {
        fail = 50;
    }
    scene_actor_shader(&psobj, 1, hash);
    if (actor_shader_count != 1 || scene_lines != before + 2) {
        fail = 51;
    }
    for (UINT i = 1; i < 9; i++) {
        hash[0] = (char)('a' + i);
        scene_actor_shader(&psobj, 1, hash);
    }
    if (actor_shader_count != 8 || scene_lines != before + 16) {
        fail = 52;
    }
    scene_lines = 15998;
    scene_geometry(&dev, 82, 0, 0, 0, 0, 0);
    scene_actor(&dev, &psobj, "unknown");
    if (reads != 6 || scene_lines != 15998) {
        fail = 47;
    }
    scene_finish("limit");
    if (scene_lines != 15999 || scene_active) {
        fail = 48;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
