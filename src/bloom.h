/* Experimental pre-UI LDR bloom, restricted to observed GOG 01.06 shaders.
 * Injects inside the existing scene. Never calls StretchRect or BeginScene.
 */
#include "bloom_source.h"
#define BL_SHADERS 6
static PVOID bl_bytecode[BL_SHADERS];
static UINT bl_compile_failed;

typedef struct {
    PVOID object;
    UINT role, character, background;
} BloomShader;

static BloomShader bl_ids[512];
static UINT bl_id_count;

static UINT bloom_role(const char *hash) {
    static const char fade_vs[] =
        "96d63e16bc9eefac691744b252bd935495c97d58027f4656be52b09506657e04";
    UINT f = 0;
    while (f < 64 && hash[f] && hash[f] == fade_vs[f]) {
        f++;
    }
    if (f == 64) {
        return 4;
    }
    static const char copy_vs[] =
        "36a760bcff380249fc5de7524044ad7dfe8d0483ba82df0e27c06bf0809925f5";
    UINT k = 0;
    while (k < 64 && hash[k] && hash[k] == copy_vs[k]) {
        k++;
    }
    if (k == 64) {
        return 3;
    }
    static const char *ids[] = {"15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7",
                                "a71e412f97d8faffbf6cea5a0782b0a8428c03753132d208516ec27e2c076394",
                                "ce7435051f0887ea339b5478ec68952257b3c8812bb714648859f07c8a78d004",
                                "37ddb1d1007174af6cee0b44c6e2d00179f767d7daf731cd68152b25c3de58f2",
                                "887e6b2524463b1f1a0110fa587ec2b0b4a98c25d6bbff5aa767ee1073f047c4",
                                "e12a27770f809981713b650c3df251c060562e750addd859807e65ec7f06fec0",
                                "36c01153a95b7492c259e56f352607d7ded0002ee58f4f1880926d4cdaafc4b3",
                                "f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1",
                                "32b581c0942e5d140a997a4827665e3897967c04be2e26611af3fb077ef7d5f7",
                                "527fcbce9cb0d09a9fe5566a01b770bd986ed9a6b475b3eed9bc4a967a2028e6"};
    for (UINT i = 0; i < 10; i++) {
        UINT j = 0;
        while (j < 64 && hash[j] && hash[j] == ids[i][j]) {
            j++;
        }
        if (j == 64) {
            return i < 8 ? 1 : 2;
        }
    }
    return 0;
}

static UINT character_role(const char *hash) {
    static const char *ids[] = {"15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7",
                                "a71e412f97d8faffbf6cea5a0782b0a8428c03753132d208516ec27e2c076394",
                                "f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1",
                                "411df34c3be010b04f9b5904a010c66e7fb3a0154175f60ccf7285063f88b4b6"};
    for (UINT i = 0; i < 4; i++) {
        if (sh_equal((const unsigned char *)hash, (const unsigned char *)ids[i], 64)) {
            return i == 3 ? 2 : 1;
        }
    }
    return 0;
}

static INT API bloom_vertex_shader(PVOID o, const UINT *code, PVOID *out) {
    INT hr = outline_vertex_shader(o, code, out);
    if (hr < 0 || !out || !*out ||
        (!bloom_available && !ao_depth_view && !ao_available && !character_preview)) {
        return hr;
    }
    /* Track ALL creations, including role zero, so reused object addresses
  * cannot inherit the classification of an earlier released shader. */
    UINT i = 0;
    while (i < bl_id_count && bl_ids[i].object != *out) {
        i++;
    }
    if (i == 512) {
        log_begin("Bloom shader table full; disabling bloom conservatively");
        log_emit();
        bloom_enabled = bloom_available = 0;
        ao_depth_view = 0;
        ao_enabled = ao_available = 0;
        character_preview = 0;
        return hr;
    }
    if (i == bl_id_count) {
        bl_id_count++;
    }
    char hash[65];
    scene_shader(*out, hash);
    bl_ids[i].object = *out;
    bl_ids[i].role = bloom_role(hash);
    bl_ids[i].character = character_role(hash);
    bl_ids[i].background = tb_identify(hash, tb_shader_ids);
    return hr;
}

static void bloom_frame_end(DeviceState *t) {
    if (!t) {
        return;
    }
    t->cm_ready = 0;
    com_release(t->ao_depth);
    t->ao_depth = 0;
    t->ao_valid = 0;
    com_release(t->bl_scene);
    t->bl_scene = 0;
    t->bl_done = 0;
    t->bl_early_ui = 0;
    t->bl_mask = 0;
}

static void bloom_cleanup(DeviceState *t) {
    if (!t) {
        return;
    }
    character_cleanup(t->obj);
    bloom_frame_end(t);
    ao_resources_release(t);
    com_release(t->ao_ps);
    t->ao_ps = 0;
    for (UINT i = 0; i < 2; i++) {
        com_release(t->bl_surf[i]);
        com_release(t->bl_tex[i]);
        t->bl_surf[i] = t->bl_tex[i] = 0;
    }
    for (UINT i = 0; i < BL_SHADERS; i++) {
        com_release(t->bl_ps[i]);
        t->bl_ps[i] = 0;
    }
    t->bl_w = t->bl_h = 0;
    com_release(t->bl_bgsurf);
    com_release(t->bl_bgtex);
    t->bl_bgsurf = t->bl_bgtex = 0;
    t->bl_bgw = t->bl_bgh = 0;
    com_release(t->bl_copysurf);
    com_release(t->bl_copytex);
    t->bl_copysurf = t->bl_copytex = 0;
    t->bl_copyw = t->bl_copyh = 0;
}

static INT bloom_compile(void) {
    UINT ready = 1;
    for (UINT i = 0; i < BL_SHADERS; i++) {
        ready &= bl_bytecode[i] != 0;
    }
    if (ready) {
        return 1;
    }
    if (bl_compile_failed) {
        return 0;
    }

    static const char *entries[] = {"extract",   "blur",           "composite",
                                    "copyScene", "extractChanged", "compositeToneMapped"};
    PVOID mod = d3dx_library();
    PVOID fn = mod ? GetProcAddress(mod, "D3DXCompileShader") : 0;
    if (!fn) {
        log_begin("Bloom compiler unavailable");
        log_emit();
        bl_compile_failed = 1;
        return 0;
    }
    for (UINT i = 0; i < BL_SHADERS; i++) {
        PVOID err = 0;
        INT hr = ((INT(API *)(const char *, UINT, PVOID, PVOID, const char *, const char *, UINT,
                              PVOID *, PVOID *, PVOID *))fn)(bloom_source, sizeof(bloom_source) - 1,
                                                             0, 0, entries[i], "ps_3_0", 0,
                                                             &bl_bytecode[i], &err, 0);
        log_begin("Bloom shader compilation");
        log_field("pass", i);
        log_hex_field("result", hr);
        log_emit();
        if (hr < 0 || !bl_bytecode[i]) {
            bl_compile_failed = 1;
            if (err) {
                PVOID *v = *(PVOID **)err;
                char *msg = ((PVOID(API *)(PVOID))v[3])(err);
                log_begin("Bloom compiler message: ");
                if (msg) {
                    log_text(msg);
                }
                log_emit();
            }
        }
        com_release(err);
        if (bl_compile_failed) {
            for (UINT j = 0; j < BL_SHADERS; j++) {
                com_release(bl_bytecode[j]);
                bl_bytecode[j] = 0;
            }
            return 0;
        }
    }
    return 1;
}

static INT bloom_resources(DeviceState *t, UINT w, UINT h) {
    if (t->bl_w == w && t->bl_h == h && t->bl_tex[0] && t->bl_tex[1]) {
        return 1;
    }
    /* Do not clear this frame's scene marker while replacing GPU resources. */
    for (UINT i = 0; i < 2; i++) {
        com_release(t->bl_surf[i]);
        com_release(t->bl_tex[i]);
        t->bl_surf[i] = t->bl_tex[i] = 0;
    }
    for (UINT i = 0; i < BL_SHADERS; i++) {
        com_release(t->bl_ps[i]);
        t->bl_ps[i] = 0;
    }
    t->bl_w = t->bl_h = 0;
    if (!bloom_compile()) {
        return 0;
    }
    for (UINT i = 0; i < BL_SHADERS; i++) {
        PVOID *v = *(PVOID **)bl_bytecode[i];
        PVOID code = ((PVOID(API *)(PVOID))v[3])(bl_bytecode[i]);
        if (((INT(API *)(PVOID, const UINT *, PVOID *))original_method(t->obj, 106))(
                t->obj, code, &t->bl_ps[i]) < 0 ||
            !t->bl_ps[i]) {
            return 0;
        }
    }
    for (UINT i = 0; i < 2; i++) {
        if (!aa_target(t->obj, w, h, 21, &t->bl_tex[i], &t->bl_surf[i])) {
            return 0;
        }
    }
    t->bl_w = w;
    t->bl_h = h;
    log_begin("Bloom resources");
    log_field("width", w);
    log_field("height", h);
    log_emit();
    return 1;
}

/* Bloom and AO save and restore by hand every state they change, instead of
 * using a D3DSBT_ALL state block: on a native Windows AMD driver, capturing and
 * applying a state block mid-frame (even with nothing drawn in between) made
 * the scene flash colors. DXVK did not. [Diagnostics] BloomStateBlock=1 brings
 * the state block back for troubleshooting. */
static const UINT bl_rs_keys[] = {7,   14, 15, 27,  28,  52,  137, 174, 194, 206,
                                  152, 22, 8,  168, 162, 128, 19,  20,  171};
static const UINT bl_ss_keys[] = {1, 2, 5, 6, 7, 11, 8, 9, 10};
#define BL_RS_COUNT (sizeof(bl_rs_keys) / sizeof(bl_rs_keys[0]))
#define BL_SS_COUNT (sizeof(bl_ss_keys) / sizeof(bl_ss_keys[0]))

typedef struct {
    PVOID tex[20], vs, ps, decl, vb, ib;
    UINT fvf, freq, vb_offset, vb_stride, tss[2], rs[BL_RS_COUNT], ss[3][BL_SS_COUNT];
    float c[16];
} BloomSaved;

static UINT bl_tex_slot(UINT i) {
    return i < 16 ? i : 257 + (i - 16);
}

static void bloom_state_save(PVOID o, BloomSaved *s) {
    for (UINT i = 0; i < 20; i++) {
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, bl_tex_slot(i), &s->tex[i]);
    }
    ((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &s->vs);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &s->ps);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 88))(o, &s->decl);
    ((INT(API *)(PVOID, UINT *))original_method(o, 90))(o, &s->fvf);
    ((INT(API *)(PVOID, UINT, PVOID *, UINT *, UINT *))original_method(o, 101))(
        o, 0, &s->vb, &s->vb_offset, &s->vb_stride);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 105))(o, &s->ib);
    ((INT(API *)(PVOID, UINT, UINT *))original_method(o, 103))(o, 0, &s->freq);
    ((INT(API *)(PVOID, UINT, UINT, UINT *))original_method(o, 66))(o, 0, 11, &s->tss[0]);
    ((INT(API *)(PVOID, UINT, UINT, UINT *))original_method(o, 66))(o, 0, 24, &s->tss[1]);
    for (UINT i = 0; i < BL_RS_COUNT; i++) {
        ((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, bl_rs_keys[i], &s->rs[i]);
    }
    for (UINT sm = 0; sm < 3; sm++) {
        for (UINT i = 0; i < BL_SS_COUNT; i++) {
            ((INT(API *)(PVOID, UINT, UINT, UINT *))original_method(o, 68))(o, sm, bl_ss_keys[i],
                                                                            &s->ss[sm][i]);
        }
    }
    ((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 110))(o, 0, s->c, 4);
}

static INT bloom_state_restore(PVOID o, BloomSaved *s) {
    INT worst = 0, hr;
#define BL_KEEP(expr)                                                                              \
    do {                                                                                           \
        hr = (expr);                                                                               \
        if (hr < 0)                                                                                \
            worst = hr;                                                                            \
    } while (0)
    for (UINT i = 0; i < 20; i++) {
        BL_KEEP(
            ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, bl_tex_slot(i), s->tex[i]));
    }
    BL_KEEP(((INT(API *)(PVOID, PVOID))original_method(o, 92))(o, s->vs));
    BL_KEEP(((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, s->ps));
    if (s->fvf) {
        BL_KEEP(((INT(API *)(PVOID, UINT))original_method(o, 89))(o, s->fvf));
    } else if (s->decl) {
        BL_KEEP(((INT(API *)(PVOID, PVOID))original_method(o, 87))(o, s->decl));
    }
    BL_KEEP(((INT(API *)(PVOID, UINT, PVOID, UINT, UINT))original_method(o, 100))(
        o, 0, s->vb, s->vb_offset, s->vb_stride));
    BL_KEEP(((INT(API *)(PVOID, PVOID))original_method(o, 104))(o, s->ib));
    BL_KEEP(((INT(API *)(PVOID, UINT, UINT))original_method(o, 102))(o, 0, s->freq ? s->freq : 1));
    BL_KEEP(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 11, s->tss[0]));
    BL_KEEP(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 24, s->tss[1]));
    for (UINT i = 0; i < BL_RS_COUNT; i++) {
        BL_KEEP(aa_rs(o, bl_rs_keys[i], s->rs[i]));
    }
    for (UINT sm = 0; sm < 3; sm++) {
        for (UINT i = 0; i < BL_SS_COUNT; i++) {
            BL_KEEP(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(
                o, sm, bl_ss_keys[i], s->ss[sm][i]));
        }
    }
    BL_KEEP(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 109))(o, 0, s->c, 4));
#undef BL_KEEP
    return worst;
}

static void bloom_state_release(BloomSaved *s) {
    for (UINT i = 0; i < 20; i++) {
        com_release(s->tex[i]);
    }
    com_release(s->vs);
    com_release(s->ps);
    com_release(s->decl);
    com_release(s->vb);
    com_release(s->ib);
}

static void bloom_run(DeviceState *t, PVOID target, UINT capture) {
    PVOID o = t->obj, source = 0, block = 0, saved[4] = {0}, depthbuf = 0;
    static BloomSaved by_hand;
    UINT hand = 0; /* Declared first: every early "goto done" must find it 0. */
    SurfaceDesc desc = {0};
    Viewport oldvp = {0};
    INT hr = 0, modified = 0, restore = 0;
    const char *step = "source";
    PVOID *v = *(PVOID **)target;
    if (((INT(API *)(PVOID, SurfaceDesc *))v[12])(target, &desc) < 0 || desc.ms || !desc.w ||
        !desc.h) {
        return;
    }
    if (capture >= 2) {
        if (!t->ao_depth) {
            goto done;
        }
        v = *(PVOID **)t->ao_depth;
    }
    hr = ((INT(API *)(PVOID, const void *, PVOID *))v[11])(capture >= 2 ? t->ao_depth : target,
                                                           probe_texture_iid, &source);
    if (hr < 0 || !source) {
        goto done;
    }
    hr = ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &oldvp);
    if (hr < 0) {
        goto failed;
    }
    /* Partial viewports and additional MRT outputs are not verified paths. */
    if (oldvp.x || oldvp.y || oldvp.w != desc.w || oldvp.h != desc.h) {
        goto done;
    }
    for (UINT i = 0; i < 4; i++) {
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, i, &saved[i]);
    }
    if (!saved[0] || saved[1] || saved[2] || saved[3]) {
        goto done;
    }
    ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &depthbuf);
    step = "resources";
    if (!(capture == 3   ? ao_resources(t, (desc.w + 1) / 2, (desc.h + 1) / 2)
          : capture == 2 ? ao_depth_shader(t)
                         : bloom_resources(t, (desc.w + 3) / 4, (desc.h + 3) / 4))) {
        hr = (INT)0x80004005;
        goto failed;
    }
    if (capture == 1 && (!t->bl_bgtex || t->bl_bgw != desc.w || t->bl_bgh != desc.h)) {
        com_release(t->bl_bgsurf);
        com_release(t->bl_bgtex);
        t->bl_bgsurf = t->bl_bgtex = 0;
        step = "background resources";
        if (!aa_target(o, desc.w, desc.h, 21, &t->bl_bgtex, &t->bl_bgsurf)) {
            hr = (INT)0x80004005;
            goto failed;
        }
        t->bl_bgw = desc.w;
        t->bl_bgh = desc.h;
    }
    /* Tone-mapped composite reads the scene, which cannot be sampled while it
     * is also the render target, so it works from a full-resolution copy. */
    if (capture == 0 && bloom_composite == 2 &&
        (!t->bl_copytex || t->bl_copyw != desc.w || t->bl_copyh != desc.h)) {
        com_release(t->bl_copysurf);
        com_release(t->bl_copytex);
        t->bl_copysurf = t->bl_copytex = 0;
        step = "scene copy resources";
        if (!aa_target(o, desc.w, desc.h, 21, &t->bl_copytex, &t->bl_copysurf)) {
            hr = (INT)0x80004005;
            goto failed;
        }
        t->bl_copyw = desc.w;
        t->bl_copyh = desc.h;
    }
    step = "state capture";
    if (!bloom_state_block) {
        by_hand = (BloomSaved){0};
        bloom_state_save(o, &by_hand);
        hand = 1;
    } else {
        hr = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 59))(o, 1, &block);
        if (hr < 0 || !block) {
            goto failed;
        }
    }
    modified = 1;
#define BL_DO(expr)                                                                                \
    do {                                                                                           \
        hr = (expr);                                                                               \
        if (hr < 0)                                                                                \
            goto failed;                                                                           \
    } while (0)
    step = "setup";
    BL_DO(((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, 0));
    for (UINT slot = 0; slot < 16; slot++) {
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, slot, 0));
    }
    for (UINT slot = 257; slot < 261; slot++) {
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, slot, 0));
    }
    BL_DO(((INT(API *)(PVOID, PVOID))original_method(o, 92))(o, 0));
    BL_DO(((INT(API *)(PVOID, UINT))original_method(o, 89))(o, 0x104));
    BL_DO(((INT(API *)(PVOID, UINT, UINT))original_method(o, 102))(o, 0, 1));
    BL_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 11, 0));
    BL_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 24, 0));
    {
        static const UINT rs[][2] = {
            {7, 0},   {14, 0},  {15, 0},  {27, 0}, {28, 0}, {52, 0},   {137, 0},          {174, 0},
            {194, 0}, {206, 0}, {152, 0}, {22, 1}, {8, 3},  {168, 15}, {162, 0xffffffff}, {128, 0}};
        for (UINT i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) {
            BL_DO(aa_rs(o, rs[i][0], rs[i][1]));
        }
    }
    {
        static const UINT ss[][2] = {{1, 3},  {2, 3}, {5, 2}, {6, 2}, {7, 0},
                                     {11, 0}, {8, 0}, {9, 0}, {10, 1}};
        for (UINT sampler = 0; sampler < 3; sampler++) {
            for (UINT i = 0; i < sizeof(ss) / sizeof(ss[0]); i++) {
                BL_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(
                    o, sampler, ss[i][0], ss[i][1]));
            }
        }
    }
    /* Bloom (capture 0) steps: 0 extract, 1 horizontal blur, 2 vertical blur,
     * 3 scene copy (tone-mapped composite only), 4 composite. Other capture
     * modes keep their own pass numbering. */
    UINT bloom_steps = bloom_composite == 2 ? 5u : 4u;
    for (UINT pass = 0; pass < (capture == 3 ? 2u : capture ? 1u : bloom_steps); pass++) {
        UINT kind = capture ? pass : (bloom_steps == 4 && pass == 3 ? 4u : pass);
        step = kind == 0   ? "extract"
               : kind == 1 ? "horizontal blur"
               : kind == 2 ? "vertical blur"
               : kind == 3 ? "scene copy"
                           : "composite";
        UINT w = capture == 3             ? (pass ? desc.w : t->ao_w)
                 : (capture || kind >= 3) ? desc.w
                                          : t->bl_w,
             h = capture == 3             ? (pass ? desc.h : t->ao_h)
                 : (capture || kind >= 3) ? desc.h
                                          : t->bl_h;
        PVOID dst = capture == 3   ? (pass ? target : t->ao_surf)
                    : capture == 2 ? target
                    : capture == 1 ? t->bl_bgsurf
                    : kind == 4    ? target
                    : kind == 3    ? t->bl_copysurf
                                   : t->bl_surf[kind == 1 ? 1 : 0];
        PVOID src =
            (capture == 3 || kind == 0 || kind == 3) ? source : t->bl_tex[kind == 2 ? 1 : 0];
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 0, 0));
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, dst));
        Viewport vp = {0, 0, w, h, 0, 1};
        BL_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &vp));
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 0, src));
        BL_DO(((INT(API *)(PVOID, PVOID))original_method(o, 107))(
            o, (capture == 3   ? t->ao_shaders[pass]
                : capture == 2 ? t->ao_ps
                               : t->bl_ps[capture == 1 ? 3
                                          : kind == 0  ? (t->bl_mask ? 4 : 0)
                                          : kind == 3  ? 3
                                          : kind == 4  ? (bloom_composite == 2 ? 5 : 2)
                                                       : 1])));
        BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(
            o, 1,
            capture == 3 && pass == 1                         ? t->ao_tex
            : (!capture && kind == 0 && t->bl_mask)           ? t->bl_bgtex
            : (!capture && kind == 4 && bloom_composite == 2) ? t->bl_copytex
                                                              : 0));
        /* c0 = per-pass parameters, c1.x = brightness measure (0 brightest channel, 1 luminance). */
        float c[8] = {0, 0, 0, 0, (float)bloom_brightness, 0, 0, 0},
              radius = (float)bloom_radius / 100.0f * (float)desc.h / 1080.0f;
        if (kind == 0) {
            c[0] = 0.25f / (float)w;
            c[1] = 0.25f / (float)h;
            c[2] = (float)bloom_threshold / 1000.0f;
            c[3] = (float)bloom_knee / 1000.0f;
        }
        if (capture == 2) {
            c[0] = (float)ao_depth_scale;
            BL_DO(aa_rs(o, 168, 7));
            for (UINT key = 5; key <= 6; key++) {
                BL_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(o, 0, key, 1));
            }
        }
        if (kind == 1) {
            c[0] = radius / (float)w;
        }
        if (kind == 2) {
            c[1] = radius / (float)h;
        }
        if (kind == 3) {
            BL_DO(aa_rs(o, 27, 0));
        }
        if (kind == 4) {
            c[0] = (float)bloom_strength / 100.0f;
            /* 0 add: scene + bloom. 1 screen: scene + bloom * (1 - scene).
             * 2 tone-mapped: the shader writes the final color, no blending. */
            BL_DO(aa_rs(o, 27, bloom_composite == 2 ? 0 : 1));
            BL_DO(aa_rs(o, 19, bloom_composite == 1 ? 10 : 2));
            BL_DO(aa_rs(o, 20, 2));
            BL_DO(aa_rs(o, 171, 1));
            BL_DO(aa_rs(o, 168, 7));
        }
        if (capture == 3) {
            float ac[16];
            for (UINT i = 0; i < 8; i++) {
                ac[i] = t->ao_proj[i];
            }
            ac[8] = (float)ao_radius;
            ac[9] = (float)ao_strength / 100.0f;
            ac[10] = (float)ao_bias / 100.0f;
            ac[11] = (float)(ao_quality == 1 ? 8 : ao_quality == 2 ? 16 : 32);
            ac[12] = 1.0f / (float)t->ao_w;
            ac[13] = 1.0f / (float)t->ao_h;
            ac[14] = (float)ao_max_distance;
            ac[15] =
                ((ao_exclude_characters || ao_exclude_backgrounds) && t->cm_ready && !t->cm_failed)
                    ? 1.0f
                    : 0;
            BL_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(
                o, 2, pass == 1 && ac[15] > 0 ? t->cm_tex : 0));
            for (UINT sampler = 0; sampler < 3; sampler++) {
                for (UINT key = 5; key <= 6; key++) {
                    BL_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(o, sampler,
                                                                                        key, 1));
                }
            }
            if (pass == 1) {
                BL_DO(aa_rs(o, 168, 7));
                BL_DO(aa_rs(o, 27, 1));
                BL_DO(aa_rs(o, 19, 1));
                BL_DO(aa_rs(o, 20, 3));
                BL_DO(aa_rs(o, 171, 1));
            }
            BL_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 109))(o, 0, ac,
                                                                                          4));
        } else {
            BL_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 109))(o, 0, c,
                                                                                          2));
        }

        struct Vertex {
            float x, y, z, rhw, u, v;
        } quad[4] = {{-.5f, -.5f, 0, 1, 0, 0},
                     {(float)w - .5f, -.5f, 0, 1, 1, 0},
                     {-.5f, (float)h - .5f, 0, 1, 0, 1},
                     {(float)w - .5f, (float)h - .5f, 0, 1, 1, 1}};
        BL_DO(((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
            o, 5, 2, quad, sizeof(quad[0])));
    }
    if (capture == 3) {
        t->ao_passes++;
        if (t->ao_passes == 1 || t->ao_passes % 600 == 0) {
            log_begin("AO pre-UI pass");
            log_field("passes", t->ao_passes);
            log_field("radius", ao_radius);
            log_field("strength", ao_strength);
            log_field("quality", ao_quality);
            log_emit();
        }
        goto done;
    }
    if (capture == 2) {
        t->ao_passes++;
        if (t->ao_passes == 1 || t->ao_passes % 600 == 0) {
            log_begin("AO depth preview pass");
            log_field("passes", t->ao_passes);
            log_field("scale", ao_depth_scale);
            log_emit();
        }
        goto done;
    }
    if (capture == 1) {
        t->bl_mask = 1;
        goto done;
    }
    t->bl_passes++;
    if (t->bl_passes == 1 || t->bl_passes % 600 == 0) {
        log_begin("Bloom pre-UI pass");
        log_field("width", desc.w);
        log_field("height", desc.h);
        log_field("strength", bloom_strength);
        log_field("threshold", bloom_threshold);
        log_field("knee", bloom_knee);
        log_field("radius", bloom_radius);
        log_field("composite", bloom_composite);
        log_field("brightness", bloom_brightness);
        log_field("passes", t->bl_passes);
        log_field("backgroundMask", t->bl_mask);
        log_emit();
    }
    goto done;
failed:
    log_begin(capture >= 2 ? "AO pass disabled after failure" : "Bloom disabled after failure");
    log_text(" step=");
    log_text(step);
    log_hex_field("result", hr);
    log_emit();
    if (capture >= 2) {
        t->ao_failed = 1;
    } else {
        t->bl_failed = 1;
    }
done:
    if (modified) {
        INT result = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 0, 0);
        if (result < 0) {
            restore = result;
        }
        result = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 1, 0);
        if (result < 0) {
            restore = result;
        }
        result = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 2, 0);
        if (result < 0) {
            restore = result;
        }
        result = ((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, 0);
        if (result < 0) {
            restore = result;
        }
        result = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, saved[0]);
        if (result < 0) {
            restore = result;
        }
        result = ((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, depthbuf);
        if (result < 0) {
            restore = result;
        }
        if (block) {
            PVOID *b = *(PVOID **)block;
            result = ((INT(API *)(PVOID))b[5])(block);
            if (result < 0) {
                restore = result;
            }
        }
        if (hand) {
            result = bloom_state_restore(o, &by_hand);
            if (result < 0) {
                restore = result;
            }
            bloom_state_release(&by_hand);
            hand = 0;
        }
        result = ((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &oldvp);
        if (result < 0) {
            restore = result;
        }
    }
    if (restore < 0) {
        log_begin("Bloom state restoration failed");
        log_hex_field("result", restore);
        log_emit();
        t->bl_failed = 1;
    }
    com_release(block);
    com_release(source);
    com_release(depthbuf);
    for (UINT i = 0; i < 4; i++) {
        com_release(saved[i]);
    }
    if (t->bl_failed) {
        bloom_cleanup(t);
    } else if (t->ao_failed) {
        ao_resources_release(t);
        com_release(t->ao_ps);
        t->ao_ps = 0;
    }
#undef BL_DO
}

static void bloom_apply(DeviceState *t, PVOID target) {
    if ((ao_depth_view || ao_enabled) && !t->ao_failed) {
        if (t->ao_depth) {
            if (ao_depth_view) {
                bloom_run(t, target, 2);
            } else if (t->ao_valid && ao_strength &&
                       (!(ao_exclude_characters || ao_exclude_backgrounds) ||
                        (t->cm_ready && !t->cm_failed))) {
                bloom_run(t, target, 3);
            }
        } else if (!t->ao_missing) {
            log_begin("AO skipped: no matching INTZ scene depth");
            log_emit();
            t->ao_missing = 1;
        }
    }
    if (!ao_depth_view && !t->bl_failed && bloom_enabled && bloom_strength) {
        bloom_run(t, target, 0);
    }
}

static void bloom_before_draw(PVOID o, PVOID caller) {
    DeviceState *t = device_state(o);
    if ((!ao_depth_view && !ao_enabled && (!bloom_enabled || !bloom_strength)) || !shadow_image ||
        !t || !t->main || t->bl_done || t->bl_failed) {
        return;
    }
    PVOID vs = 0;
    UINT role = 0;
    if (((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &vs) >= 0 && vs) {
        for (UINT i = 0; i < bl_id_count; i++) {
            if (bl_ids[i].object == vs) {
                role = bl_ids[i].role;
                break;
            }
        }
    }
    com_release(vs);
    if (!role) {
        return;
    }
    PVOID target = 0;
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &target) < 0 || !target) {
        com_release(target);
        return;
    }
    if (role == 1 && !t->bl_scene) {
        if (t->bl_early_ui && !ao_depth_view && bloom_enabled && bloom_strength) {
            bloom_run(t, target, 1);
        }
        if (!t->bl_failed) {
            if (ao_depth_view || ao_enabled) {
                ao_depth_retain(t);
            }
            if (ao_enabled && !ao_depth_view) {
                ao_camera(t);
            }
            t->bl_scene = target;
            return;
        }
    }
    if (role == 2) {
        if (t->bl_scene) {
            t->bl_done = 1;
            if (target == t->bl_scene) {
                bloom_apply(t, target);
            }
        } else {
            t->bl_early_ui = 1;
        }
    }
    if (role == 4 && target == t->bl_scene &&
        (unsigned long)caller - (unsigned long)shadow_image == game_build->fade_rva) {
        PVOID ps = 0;
        char hash[65];
        ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps);
        scene_shader(ps, hash);
        com_release(ps);
        static const char fade_ps[] =
            "06f603a6391741e8fd3813d835e1b65be8880258f34625561eaad339ae0b8fb8";
        UINT k = 0;
        while (k < 64 && hash[k] && hash[k] == fade_ps[k]) {
            k++;
        }
        if (k == 64) {
            t->bl_done = 1;
            bloom_apply(t, target);
        }
    }
    /* Exact simple-quad path observed after 3D effects in all three animation
  * captures. Process the scene BEFORE a verified full-size copy so later
  * compositing cannot restore an unprocessed snapshot over our result. */
    UINT caller_rva = (UINT)((unsigned long)caller - (unsigned long)shadow_image);
    if (role == 3 && t->bl_scene &&
        (caller_rva == game_build->copy_rva || caller_rva == game_build->scene_copy_rva)) {
        PVOID ps = 0;
        char hash[65];
        ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps);
        scene_shader(ps, hash);
        com_release(ps);
        static const char copy_ps[] =
            "5f86420a561c69d2dd4f76d37b8edc1e21c811d277cb365959f23d47d16cb377";
        UINT k = 0;
        while (k < 64 && hash[k] && hash[k] == copy_ps[k]) {
            k++;
        }
        if (k == 64) {
            INT boundary = caller_rva == game_build->copy_rva && target == t->bl_scene;
            if (!boundary && target != t->bl_scene) {
                PVOID back = 0, tex = 0, source = 0;
                PVOID *v = *(PVOID **)t->bl_scene;
                ((INT(API *)(PVOID, UINT, UINT, UINT, PVOID *))original_method(o, 18))(o, 0, 0, 0,
                                                                                       &back);
                ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, 0, &tex);
                ((INT(API *)(PVOID, const void *, PVOID *))v[11])(t->bl_scene, probe_texture_iid,
                                                                  &source);
                boundary = source && tex == source &&
                           ((caller_rva == game_build->copy_rva && back && target == back) ||
                            (caller_rva == game_build->scene_copy_rva && main_surface(t, target)));
                com_release(back);
                com_release(tex);
                com_release(source);
            }
            if (boundary) {
                t->bl_done = 1;
                /* Logged once per distinct boundary type. */
                if (log_signature_is_new(
                        13, caller_rva == game_build->copy_rva && target != t->bl_scene,
                        caller_rva == game_build->scene_copy_rva, 0, 0, 0, 0, 0)) {
                    log_begin("Bloom HUD-free boundary");
                    log_field("finalCopy",
                              caller_rva == game_build->copy_rva && target != t->bl_scene);
                    log_field("sceneCopy", caller_rva == game_build->scene_copy_rva);
                    log_emit();
                }
                bloom_apply(t, t->bl_scene);
            }
        }
    }
    com_release(target);
}
