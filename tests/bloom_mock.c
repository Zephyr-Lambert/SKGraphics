#define AA_TEST_ENTRY aa_tests_unused
#include "aa_mock.c"
static PVOID bt[19], bs[13], btex[3], bsurf[3], scene_texture = objtable;
static PVOID input, game_vs;
static UINT write_mask = 15, blend_enabled, srcblend, dstblend, blendop, pass_no, fail_pass,
            fail_alloc, alloc_no;
static float params[8];
/* Composite-mode scenario: record each pass instead of the 4-pass checks. */
static UINT tone_mode, tone_count;
static PVOID tone_rt[8], tone_in[8], tone_mask[8];
static UINT tone_blend[8], tone_src[8], tone_w[8];
static float tone_params[8][8];
static UINT captured;
static PVOID mask_input;

static INT API bm_create(PVOID o, UINT w, UINT h, UINT levels, UINT usage, UINT fmt, UINT pool,
                         PVOID *out, PVOID shared) {
    (void)o;
    (void)shared;
    UINT i = alloc_no++;
    if (tone_mode) {
        if (levels != 1 || usage != 1 || fmt != 21 || pool || i > 2) {
            fail = 70;
        }
    } else if (w != (i == 2 ? 3840u : 960u) || h != (i == 2 ? 2160u : 540u) || levels != 1 ||
               usage != 1 || fmt != 21 || pool || i > 2) {
        fail = 40;
    }
    if (fail_alloc && i == 1) {
        return -1;
    }
    btex[i] = bt;
    bsurf[i] = bs;
    *out = &btex[i];
    return 0;
}

static INT API bm_surface(PVOID o, UINT level, PVOID *out) {
    if (level) {
        fail = 41;
    }
    *out = o == &btex[0] ? &bsurf[0] : o == &btex[1] ? &bsurf[1] : &bsurf[2];
    return 0;
}

static INT API bm_container(PVOID o, const void *iid, PVOID *out) {
    (void)o;
    (void)iid;
    *out = &scene_texture;
    return 0;
}

static INT API bm_tex(PVOID o, UINT slot, PVOID p) {
    (void)o;
    if (!slot) {
        input = p;
    }
    if (slot == 1) {
        mask_input = p;
    }
    return 0;
}

static INT API bm_rs(PVOID o, UINT key, UINT val) {
    (void)o;
    if (key == 168) {
        write_mask = val;
    }
    if (key == 27) {
        blend_enabled = val;
    }
    if (key == 19) {
        srcblend = val;
    }
    if (key == 20) {
        dstblend = val;
    }
    if (key == 171) {
        blendop = val;
    }
    return 0;
}

static UINT hand_mode, blocks_made, rs_reads;

static INT API bm_constants(PVOID o, UINT start, const float *c, UINT count) {
    (void)o;
    if (start || (count != 2 && !(hand_mode && count == 4))) {
        fail = 42;
    }
    for (UINT i = 0; i < 8; i++) {
        params[i] = c[i];
    }
    return 0;
}

/* Getters for the default by-hand save/restore: every saved render state reads back as 0x55. */
static INT API get_tex(PVOID o, UINT slot, PVOID *out) {
    (void)o;
    (void)slot;
    *out = 0;
    return 0;
}

static INT API get_ptr(PVOID o, PVOID *out) {
    (void)o;
    *out = 0;
    return 0;
}

static INT API get_uint(PVOID o, UINT *out) {
    (void)o;
    *out = 0;
    return 0;
}

static INT API get_stream(PVOID o, UINT s, PVOID *vb, UINT *off, UINT *stride) {
    (void)o;
    (void)s;
    *vb = 0;
    *off = *stride = 0;
    return 0;
}

static INT API get_freq(PVOID o, UINT s, UINT *out) {
    (void)o;
    (void)s;
    *out = 1;
    return 0;
}

static INT API get_two(PVOID o, UINT a, UINT b, UINT *out) {
    (void)o;
    (void)a;
    (void)b;
    *out = 0;
    return 0;
}

static INT API get_rs(PVOID o, UINT key, UINT *out) {
    (void)o;
    (void)key;
    rs_reads++;
    *out = 0x55;
    return 0;
}

static INT API get_consts(PVOID o, UINT start, float *c, UINT count) {
    (void)o;
    (void)start;
    for (UINT i = 0; i < count * 4; i++) {
        c[i] = 0;
    }
    return 0;
}

static INT API set_stream(PVOID o, UINT s, PVOID vb, UINT off, UINT stride) {
    (void)o;
    (void)s;
    (void)vb;
    (void)off;
    (void)stride;
    return 0;
}

static INT API count_block(PVOID o, UINT type, PVOID *out) {
    blocks_made++;
    return stateblock(o, type, out);
}

static INT API bm_apply(PVOID o) {
    (void)o;
    applied++;
    input = (PVOID)123;
    write_mask = 15;
    blend_enabled = 0;
    return 0;
}

static INT API bm_vs(PVOID o, PVOID *out) {
    (void)o;
    *out = game_vs;
    return 0;
}

static INT API bm_quad(PVOID o, UINT type, UINT count, const void *data, UINT stride) {
    (void)o;
    (void)data;
    if (tone_mode) {
        pass_no++;
        if (tone_count < 8) {
            tone_rt[tone_count] = current_rt;
            tone_in[tone_count] = input;
            tone_mask[tone_count] = mask_input;
            tone_blend[tone_count] = blend_enabled;
            tone_src[tone_count] = srcblend;
            tone_w[tone_count] = actualvp.w;
            for (UINT i = 0; i < 8; i++) {
                tone_params[tone_count][i] = params[i];
            }
            tone_count++;
        }
        return 0;
    }
    if (current_rt == &bsurf[2]) {
        if (input != &scene_texture || mask_input || actualvp.w != 3840 || actualvp.h != 2160 ||
            blend_enabled) {
            fail = 61;
        }
        captured++;
        return 0;
    }
    UINT p = pass_no++;
    if (!in_scene || type != 5 || count != 2 || stride != 24 || current_depth) {
        fail = 43;
    }
    PVOID want_rt = p == 3 ? &oldrt : &bsurf[p == 1 ? 1 : 0];
    PVOID want_in = p == 0 ? &scene_texture : &btex[p == 2 ? 1 : 0];
    if (current_rt != want_rt || input != want_in || actualvp.w != (p == 3 ? 3840u : 960u) ||
        actualvp.h != (p == 3 ? 2160u : 540u)) {
        fail = 44;
    }
    if (p < 3 && (blend_enabled || write_mask != 15)) {
        fail = 45;
    }
    if (captured && p == 0 && mask_input != &btex[2]) {
        fail = 65;
    }
    if (p == 3 &&
        (!blend_enabled || write_mask != 7 || srcblend != 2 || dstblend != 2 || blendop != 1)) {
        fail = 46;
    }
    if (p == 0 && (params[2] != .75f || params[3] != .1f || params[4] != 0)) {
        fail = 47;
    }
    if (p == 1 && (params[0] <= 0 || params[1])) {
        fail = 48;
    }
    if (p == 2 && (params[1] <= 0 || params[0])) {
        fail = 49;
    }
    if (p == 3 && params[0] != .35f) {
        fail = 50;
    }
    return fail_pass == p + 1 ? -1 : 0;
}

static void bm_init(void) {
    bloom_enabled = 1;
    /* The pass tests below check restoration through the state block mock;
     * the default by-hand path is tested at the end of the bloom tests. */
    bloom_state_block = 1;
    bt[2] = release_aa;
    bt[18] = bm_surface;
    bs[2] = release_aa;
    objtable[2] = release_aa;
    objtable[3] = bytes;
    objtable[5] = bm_apply;
    sv[2] = release_aa;
    sv[11] = bm_container;
    sv[12] = getdesc;
    dv[23] = bm_create;
    dv[37] = writert;
    dv[38] = readrt;
    dv[39] = writedepth;
    dv[40] = readdepth;
    dv[47] = writevp;
    dv[48] = readvp;
    dv[57] = bm_rs;
    dv[59] = stateblock;
    dv[65] = bm_tex;
    dv[67] = threeuint;
    dv[69] = threeuint;
    dv[83] = bm_quad;
    dv[89] = oneuint;
    dv[92] = oneptr;
    dv[93] = bm_vs;
    dv[102] = twouint;
    dv[106] = mkshader;
    dv[107] = oneptr;
    dv[109] = bm_constants;
    hook_table_record(&dobj, 119);
    for (UINT i = 0; i < BL_SHADERS; i++) {
        bl_bytecode[i] = &byteobj;
    }
    current_rt = &oldrt;
    current_depth = &olddepth;
    actualvp = (Viewport){0, 0, 3840, 2160, 0, 1};
    in_scene = 1;
}
#ifndef BLOOM_TEST_ENTRY
#define BLOOM_TEST_ENTRY _start
#endif
void BLOOM_TEST_ENTRY(void) {
    bm_init();
    DeviceState *t = device_state_new(&dobj);
    t->rw = 3840;
    t->rh = 2160;
    t->main = 1;
    bloom_apply(t, &oldrt);
    if (t->bl_failed || pass_no != 4 || t->bl_passes != 1 || applied != 1 || current_rt != &oldrt ||
        current_depth != &olddepth || actualvp.w != 3840 || actualvp.h != 2160 ||
        input != (PVOID)123 || write_mask != 15 || !in_scene) {
        fail = 51;
    }
    pass_no = 0;
    bloom_apply(t, &oldrt);
    if (alloc_no != 2 || pass_no != 4 || t->bl_passes != 2) {
        fail = 52;
    }
    /* HUD-free final copy: scene source is no longer the active RT. Bloom must
  * write the retained scene, then restore the game's backbuffer binding. */
    pass_no = 0;
    current_rt = &backobj;
    bloom_apply(t, &oldrt);
    if (t->bl_failed || pass_no != 4 || current_rt != &backobj || current_depth != &olddepth ||
        input != (PVOID)123 || actualvp.w != 3840) {
        fail = 60;
    }
    current_rt = &oldrt;
    /* Each injected draw can fail; game state and cached resources must recover. */
    for (UINT f = 1; f <= 4; f++) {
        pass_no = 0;
        fail_pass = f;
        t->bl_failed = 0;
        bloom_apply(t, &oldrt);
        if (!t->bl_failed || t->bl_tex[0] || t->bl_surf[0] || t->bl_ps[0] || pass_no != f ||
            current_rt != &oldrt || current_depth != &olddepth || actualvp.w != 3840 ||
            input != (PVOID)123 || !in_scene) {
            fail = 53;
        }
        alloc_no = 0;
    }
    fail_pass = 0;
    t->bl_failed = 0;
    fail_alloc = 1;
    bloom_apply(t, &oldrt);
    if (!t->bl_failed || t->bl_tex[0] || t->bl_tex[1] || t->bl_ps[2] || current_rt != &oldrt) {
        fail = 54;
    }
    fail_alloc = 0;
    t->bl_failed = 0;
    alloc_no = 0;
    pass_no = 0;
    /* UI-first / paused frames skip. Confirm one trigger per scene frame. */
    bl_ids[0] = (BloomShader){&psobj, 1};
    bl_ids[1] = (BloomShader){&byteobj, 2};
    bl_id_count = 2;
    shadow_image = (unsigned char *)1;
    bloom_enabled = 1;
    game_vs = &byteobj;
    bloom_before_draw(&dobj, 0);
    if (pass_no || t->bl_done || !t->bl_early_ui) {
        fail = 55;
    }
    bloom_frame_end(t);
    game_vs = &psobj;
    bloom_before_draw(&dobj, 0);
    if (t->bl_scene != &oldrt || pass_no) {
        fail = 56;
    }
    game_vs = &byteobj;
    bloom_before_draw(&dobj, 0);
    if (pass_no != 4 || !t->bl_done || t->bl_failed) {
        fail = 57;
    }
    bloom_before_draw(&dobj, 0);
    if (pass_no != 4) {
        fail = 58;
    }
    bloom_frame_end(t);
    if (t->bl_done || t->bl_scene) {
        fail = 59;
    }
    bloom_cleanup(t);
    alloc_no = 0;
    pass_no = 0;
    game_vs = &byteobj;
    bloom_before_draw(&dobj, 0);
    game_vs = &psobj;
    bloom_before_draw(&dobj, 0);
    if (captured != 1 || !t->bl_mask || pass_no || t->bl_failed || current_rt != &oldrt ||
        input != (PVOID)123) {
        fail = 62;
    }
    game_vs = &byteobj;
    bloom_before_draw(&dobj, 0);
    if (pass_no != 4 || t->bl_failed || !t->bl_done) {
        fail = 63;
    }
    bloom_cleanup(t);
    if (t->bl_bgtex || t->bl_bgsurf || t->bl_mask || t->bl_early_ui) {
        fail = 64;
    }

    /* Screen composite: same four passes, final blend is INVDESTCOLOR + ONE. */
    tone_mode = 1;
    for (UINT mode = 1; mode <= 2; mode++) {
        bloom_cleanup(t);
        bloom_composite = mode;
        bloom_brightness = 1;
        alloc_no = pass_no = tone_count = 0;
        bloom_apply(t, &oldrt);
        if (t->bl_failed || tone_count != (mode == 2 ? 5u : 4u)) {
            fail = 71;
            break;
        }
        UINT last = tone_count - 1;
        if (tone_rt[last] != &oldrt || tone_in[last] != &btex[0] || tone_w[last] != 3840 ||
            tone_params[last][0] != .35f || tone_params[0][4] != 1.0f) {
            fail = 72;
        }
        if (mode == 1 && (!tone_blend[last] || tone_src[last] != 10 || tone_mask[last])) {
            fail = 73;
        }
        if (mode == 2) {
            /* Pass 3 copies the full-size scene; composite reads it without blending. */
            if (tone_rt[3] != &bsurf[2] || tone_in[3] != &scene_texture || tone_w[3] != 3840 ||
                tone_blend[3] || tone_blend[last] || tone_mask[last] != &btex[2] ||
                !t->bl_copytex) {
                fail = 74;
            }
        }
    }
    bloom_cleanup(t);
    if (t->bl_copytex || t->bl_copysurf || t->bl_copyw) {
        fail = 75;
    }
    tone_mode = 0;
    bloom_composite = bloom_brightness = 0;

    /* Default (BloomStateBlock=0): no state block; every changed state saved
     * and restored by hand. */
    dv[64] = get_tex;
    dv[88] = get_ptr;
    dv[105] = get_ptr;
    dv[108] = get_ptr;
    dv[90] = get_uint;
    dv[101] = get_stream;
    dv[100] = set_stream;
    dv[104] = oneptr;
    dv[87] = oneptr;
    dv[103] = get_freq;
    dv[66] = get_two;
    dv[68] = get_two;
    dv[58] = get_rs;
    dv[110] = get_consts;
    dv[59] = count_block;
    {
        /* The hook table copied the vtable at first use; refresh its originals. */
        HookTable *table = hook_table_find(&dobj);
        for (UINT i = 0; i < 119; i++) {
            table->original[i] = dv[i];
        }
    }
    bloom_cleanup(t);
    tone_mode = hand_mode = 1;
    bloom_state_block = 0;
    alloc_no = pass_no = tone_count = applied = blocks_made = rs_reads = 0;
    bloom_apply(t, &oldrt);
    if (t->bl_failed || tone_count != 4 || blocks_made || applied || !rs_reads ||
        blend_enabled != 0x55 || current_rt != &oldrt || current_depth != &olddepth ||
        actualvp.w != 3840) {
        fail = 76;
    }
    /* BloomStateBlock=1: the 1.x state block path still works. */
    bloom_state_block = 1;
    alloc_no = pass_no = tone_count = applied = blocks_made = 0;
    bloom_apply(t, &oldrt);
    if (t->bl_failed || tone_count != 4 || blocks_made != 1 || applied != 1 ||
        current_rt != &oldrt) {
        fail = 77;
    }
    bloom_state_block = 0;
    tone_mode = hand_mode = 0;
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
