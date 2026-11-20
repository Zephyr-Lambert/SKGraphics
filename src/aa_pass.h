/* Full-frame AA, before Present. All game state is restored after the pass. */
#include "fxaa_source.h"
#include "smaa_source.h"
#include "smaa/AreaTex.h"
#include "smaa/SearchTex.h"
static PVOID sm_bytecode[3];
static INT sm_compile_failed;
static PVOID aa_bytecode;
static INT aa_compile_failed;

static void com_release(PVOID p) {
    if (p) {
        PVOID *v = *(PVOID **)p;
        ((UINT(API *)(PVOID))v[2])(p);
    }
}

static void aa_cleanup(DeviceState *t) {
    if (!t || t->aa_busy) {
        return;
    }
    t->aa_busy = 1;
    PVOID surf = t->aa_surface, tex = t->aa_tex, ps = t->aa_shader;
    t->aa_surface = 0;
    t->aa_tex = 0;
    t->aa_shader = 0;
    t->aa_w = t->aa_h = 0;
    com_release(surf);
    com_release(tex);
    com_release(ps);
    for (UINT i = 0; i < 2; i++) {
        com_release(t->sm_surface[i]);
        com_release(t->sm_tex[i]);
        t->sm_surface[i] = t->sm_tex[i] = 0;
    }
    for (UINT i = 0; i < 3; i++) {
        com_release(t->sm_shader[i]);
        t->sm_shader[i] = 0;
    }
    com_release(t->sm_area);
    com_release(t->sm_search);
    t->sm_area = t->sm_search = 0;
    t->aa_cached_method = 0;
    t->aa_busy = 0;
}

/* Cache lifetime is tied to Reset / next CreateDevice, not transient device
 * Release calls (GetDevice/Release pairs occur during normal frames).
 * Cached resources may hold the last device references until next creation or
 * process teardown. Do not call COM cleanup from DllMain under loader lock.
 */
static INT aa_compile(void) {
    if (aa_bytecode) {
        return 1;
    }
    if (aa_compile_failed) {
        return 0;
    }

    PVOID mod = d3dx_library();
    PVOID f = mod ? GetProcAddress(mod, "D3DXCompileShader") : 0;
    if (!f) {
        log_begin("AA compiler unavailable; AA disabled");
        log_emit();
        aa_compile_failed = 1;
        return 0;
    }
    PVOID errors = 0;
    INT hr = ((INT(API *)(const char *, UINT, PVOID, PVOID, const char *, const char *, UINT,
                          PVOID *, PVOID *, PVOID *))f)(
        aa_source, sizeof(aa_source) - 1, 0, 0, "main", "ps_3_0", 0, &aa_bytecode, &errors, 0);
    log_begin("AA shader compilation");
    log_hex_field("result", hr);
    log_emit();
    if (hr < 0 || !aa_bytecode) {
        if (errors) {
            PVOID *v = *(PVOID **)errors;
            char *msg = ((PVOID(API *)(PVOID))v[3])(errors);
            log_begin("AA compiler message: ");
            if (msg) {
                log_text(msg);
            }
            log_emit();
        }
        aa_compile_failed = 1;
    }
    com_release(errors);
    if (aa_compile_failed) {
        com_release(aa_bytecode);
        aa_bytecode = 0;
    }
    return !aa_compile_failed;
}

/* Three separately compiled ps_3_0 entry points. Presets are compile-time. */
static INT sm_compile(void) {
    if (sm_bytecode[0] && sm_bytecode[1] && sm_bytecode[2]) {
        return 1;
    }
    if (sm_compile_failed) {
        return 0;
    }

    static const char *presets[] = {"SMAA_PRESET_LOW", "SMAA_PRESET_MEDIUM", "SMAA_PRESET_HIGH",
                                    "SMAA_PRESET_ULTRA"};
    static const char *entries[] = {"edges", "weights", "blend"};

    struct Macro {
        const char *name, *value;
    } defines[2] = {{presets[smaa_quality], "1"}, {0, 0}};

    PVOID mod = d3dx_library();
    PVOID f = mod ? GetProcAddress(mod, "D3DXCompileShader") : 0;
    if (!f) {
        log_begin("SMAA compiler unavailable");
        log_emit();
        sm_compile_failed = 1;
        return 0;
    }
    for (UINT i = 0; i < 3; i++) {
        PVOID errors = 0;
        INT hr = ((INT(API *)(const char *, UINT, PVOID, PVOID, const char *, const char *, UINT,
                              PVOID *, PVOID *, PVOID *))f)(smaa_source, sizeof(smaa_source) - 1,
                                                            defines, 0, entries[i], "ps_3_0", 0,
                                                            &sm_bytecode[i], &errors, 0);
        log_begin("SMAA shader compilation");
        log_field("pass", i);
        log_field("quality", smaa_quality);
        log_hex_field("result", hr);
        log_emit();
        if (hr < 0 || !sm_bytecode[i]) {
            if (errors) {
                PVOID *v = *(PVOID **)errors;
                char *msg = ((PVOID(API *)(PVOID))v[3])(errors);
                log_begin("SMAA compiler message: ");
                if (msg) {
                    log_text(msg);
                }
                log_emit();
            }
            sm_compile_failed = 1;
        }
        com_release(errors);
        if (sm_compile_failed) {
            for (UINT j = 0; j < 3; j++) {
                com_release(sm_bytecode[j]);
                sm_bytecode[j] = 0;
            }
            return 0;
        }
    }
    return 1;
}

static INT aa_target(PVOID o, UINT w, UINT h, UINT format, PVOID *tex, PVOID *surface) {
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *,
                          PVOID))original_method(o, 23))(o, w, h, 1, 1, format, 0, tex, 0);
    if (hr >= 0 && *tex) {
        PVOID *v = *(PVOID **)*tex;
        hr = ((INT(API *)(PVOID, UINT, PVOID *))v[18])(*tex, 0, surface);
    }
    return hr >= 0 && *tex && *surface;
}

/* Expand lookup channels into widely supported A8R8G8B8, respecting row pitch.
 * R/G contain the original area bytes; R contains the search byte.
 * MANAGED lookup textures are immutable after upload and released on Reset.
 */
static INT sm_lookup(PVOID o, UINT w, UINT h, const unsigned char *src, UINT channels, PVOID *out) {
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *,
                          PVOID))original_method(o, 23))(o, w, h, 1, 0, 21, 1, out, 0);
    if (hr < 0 || !*out) {
        return 0;
    }

    struct Locked {
        INT pitch;
        PVOID bits;
    } lock = {0};

    PVOID *v = *(PVOID **)*out;
    hr = ((INT(API *)(PVOID, UINT, PVOID, PVOID, UINT))v[19])(*out, 0, &lock, 0, 0);
    if (hr < 0) {
        return 0;
    }
    if (!lock.bits || lock.pitch < (INT)(w * 4)) {
        ((INT(API *)(PVOID, UINT))v[20])(*out, 0);
        return 0;
    }
    for (UINT y = 0; y < h; y++) {
        unsigned char *row = (unsigned char *)lock.bits + y * lock.pitch;
        for (UINT x = 0; x < w; x++) {
            UINT k = (y * w + x) * channels;
            row[x * 4] = 0;
            row[x * 4 + 1] = channels == 2 ? src[k + 1] : 0;
            row[x * 4 + 2] = src[k];
            row[x * 4 + 3] = 255;
        }
    }
    return ((INT(API *)(PVOID, UINT))v[20])(*out, 0) >= 0;
}

static INT aa_resources(DeviceState *t, SurfaceDesc *d) {
    if (t->aa_tex && t->aa_w == d->w && t->aa_h == d->h && t->aa_format == d->format &&
        t->aa_cached_method == aa_method) {
        return 1;
    }
    t->aa_busy = 0;
    aa_cleanup(t);
    t->aa_busy = 1;
    PVOID o = t->obj;
    INT ok = 1;
    if (aa_method == 2) {
        if (!sm_compile()) {
            return 0;
        }
        for (UINT i = 0; i < 3 && ok; i++) {
            PVOID *v = *(PVOID **)sm_bytecode[i];
            PVOID code = ((PVOID(API *)(PVOID))v[3])(sm_bytecode[i]);
            ok = ((INT(API *)(PVOID, const UINT *, PVOID *))original_method(o, 106))(
                     o, code, &t->sm_shader[i]) >= 0 &&
                 t->sm_shader[i];
        }
        for (UINT i = 0; i < 2 && ok; i++) {
            ok = aa_target(o, d->w, d->h, 21, &t->sm_tex[i], &t->sm_surface[i]);
        }
        if (ok) {
            ok = sm_lookup(o, AREATEX_WIDTH, AREATEX_HEIGHT, areaTexBytes, 2, &t->sm_area);
        }
        if (ok) {
            ok = sm_lookup(o, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, searchTexBytes, 1, &t->sm_search);
        }
    } else {
        if (!aa_compile()) {
            return 0;
        }
        PVOID *v = *(PVOID **)aa_bytecode;
        PVOID code = ((PVOID(API *)(PVOID))v[3])(aa_bytecode);
        ok = ((INT(API *)(PVOID, const UINT *, PVOID *))original_method(o, 106))(
                 o, code, &t->aa_shader) >= 0 &&
             t->aa_shader;
    }
    if (ok) {
        ok = aa_target(o, d->w, d->h, d->format, &t->aa_tex, &t->aa_surface);
    }
    log_begin(aa_method == 2 ? "SMAA resources" : "AA resources");
    log_field("width", d->w);
    log_field("height", d->h);
    log_field("format", d->format);
    log_field("success", ok);
    log_emit();
    if (!ok) {
        return 0;
    }
    t->aa_w = d->w;
    t->aa_h = d->h;
    t->aa_format = d->format;
    t->aa_cached_method = aa_method;
    t->aa_passes = 0; /* log the first pass after a method change */
    return 1;
}

static INT aa_rs(PVOID o, UINT key, UINT value) {
    return ((INT(API *)(PVOID, UINT, UINT))original_method(o, 57))(o, key, value);
}

static INT aa_ss(PVOID o, UINT key, UINT value) {
    return ((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(o, 0, key, value);
}

static void aa_apply(DeviceState *t) {
    if (!aa_method || !aa_strength || !t || t->aa_failed || t->aa_busy) {
        return;
    }
    PVOID o = t->obj;
    INT hr = 0, restore = 0, begun = 0, modified = 0;
    const char *step = "cooperative level";
    PVOID back = 0, saved[4] = {0}, depthbuf = 0, block = 0;
    Viewport oldvp = {0};
    SurfaceDesc desc = {0};
    t->aa_busy = 1;
    hr = ((INT(API *)(PVOID))original_method(o, 3))(o);
    if (hr < 0) {
        goto done;
    }
    step = "backbuffer";
    hr = ((INT(API *)(PVOID, UINT, UINT, UINT, PVOID *))original_method(o, 18))(o, 0, 0, 0, &back);
    if (hr < 0 || !back) {
        goto failed;
    }
    {
        PVOID *v = *(PVOID **)back;
        hr = ((INT(API *)(PVOID, SurfaceDesc *))v[12])(back, &desc);
    }
    if (hr < 0) {
        goto failed;
    }
    step = "resources";
    if (!aa_resources(t, &desc)) {
        hr = (INT)0x80004005;
        goto failed;
    }
    step = "frame copy";
    hr = ((INT(API *)(PVOID, PVOID, PVOID, PVOID, PVOID, UINT))original_method(o, 34))(
        o, back, 0, t->aa_surface, 0, 0);
    if (hr < 0) {
        goto failed;
    }
    step = "state capture";
    hr = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 59))(o, 1, &block);
    if (hr < 0 || !block) {
        goto failed;
    }
    /* MRTs and depth surfaces are not reliably covered by the state block. */
    for (UINT i = 0; i < 4; i++) {
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, i, &saved[i]);
    }
    if (!saved[0]) {
        hr = (INT)0x80004005;
        goto failed;
    }
    ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &depthbuf);
    hr = ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &oldvp);
    if (hr < 0) {
        goto failed;
    }
    step = "begin scene";
    hr = ((INT(API *)(PVOID))original_method(o, 41))(o);
    if (hr < 0) {
        goto failed;
    }
    begun = 1;
    modified = 1;
#define AA_DO(expr)                                                                                \
    do {                                                                                           \
        hr = (expr);                                                                               \
        if (hr < 0)                                                                                \
            goto failed;                                                                           \
    } while (0)
    step = "pass setup";
    AA_DO(((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, 0));
    for (UINT i = 1; i < 4; i++) {
        if (saved[i]) {
            AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, i, 0));
        }
    }
    AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, back));
    {
        Viewport vp = {0, 0, desc.w, desc.h, 0.0f, 1.0f};
        AA_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &vp));
    }
    AA_DO(((INT(API *)(PVOID, PVOID))original_method(o, 92))(o, 0));
    AA_DO(((INT(API *)(PVOID, UINT))original_method(o, 89))(o, 0x104));
    AA_DO(((INT(API *)(PVOID, UINT, UINT))original_method(o, 102))(o, 0, 1));
    if (aa_method == 1) {
        AA_DO(((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, t->aa_shader));
    }
    AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 0, t->aa_tex));
    AA_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 11, 0));
    AA_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(o, 0, 24, 0));
    {
        static const UINT rs[][2] = {
            {7, 0},   {14, 0},  {15, 0},  {27, 0}, {28, 0}, {52, 0},   {137, 0},          {174, 0},
            {194, 0}, {206, 0}, {152, 0}, {22, 1}, {8, 3},  {168, 15}, {162, 0xffffffff}, {128, 0}};
        for (UINT i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) {
            AA_DO(aa_rs(o, rs[i][0], rs[i][1]));
        }
    }
    {
        static const UINT ss[][2] = {{1, 3}, {2, 3}, {5, 2}, {6, 2}, {7, 0}, {11, 0}};
        for (UINT i = 0; i < sizeof(ss) / sizeof(ss[0]); i++) {
            AA_DO(aa_ss(o, ss[i][0], ss[i][1]));
        }
    }

    struct Vertex {
        float x, y, z, rhw, u, v;
    } quad[4] = {{-0.5f, -0.5f, 0, 1, 0, 0},
                 {(float)desc.w - 0.5f, -0.5f, 0, 1, 1, 0},
                 {-0.5f, (float)desc.h - 0.5f, 0, 1, 0, 1},
                 {(float)desc.w - 0.5f, (float)desc.h - 0.5f, 0, 1, 1, 1}};

    if (aa_method == 2) {
        float c[8] = {1.0f / (float)desc.w,
                      1.0f / (float)desc.h,
                      (float)desc.w,
                      (float)desc.h,
                      (float)aa_strength / 100.0f,
                      0,
                      0,
                      0};
        AA_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 109))(o, 0, c, 2));
        for (UINT sampler = 0; sampler < 3; sampler++) {
            static const UINT ss[][2] = {{1, 3},  {2, 3}, {5, 2}, {6, 2}, {7, 0},
                                         {11, 0}, {8, 0}, {9, 0}, {10, 1}};
            for (UINT i = 0; i < sizeof(ss) / sizeof(ss[0]); i++) {
                /* Sampler 2 holds SearchTex, which SMAA requires point-sampled:
                 * bilinear reads corrupt the edge-length search and nearly
                 * cancel the effect. Edges, area and color stay bilinear. */
                UINT value = sampler == 2 && (ss[i][0] == 5 || ss[i][0] == 6) ? 1 : ss[i][1];
                AA_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 69))(
                    o, sampler, ss[i][0], value));
            }
        }
        for (UINT pass = 0; pass < 3; pass++) {
            step = pass == 0   ? "SMAA edge detection"
                   : pass == 1 ? "SMAA blending weights"
                               : "SMAA neighborhood blend";
            /* Unbind previous inputs before making any of them a render target. */
            for (UINT slot = 0; slot < 3; slot++) {
                AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, slot, 0));
            }
            AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(
                o, 0, pass < 2 ? t->sm_surface[pass] : back));
            {
                Viewport vp = {0, 0, desc.w, desc.h, 0, 1};
                AA_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &vp));
            }
            /* Edge detection discards non-edges: clear every frame to avoid stale data. */
            if (pass < 2) {
                AA_DO(((INT(API *)(PVOID, UINT, PVOID, UINT, UINT, float, UINT))original_method(
                    o, 43))(o, 0, 0, 1, 0, 1.0f, 0));
            }
            AA_DO(((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, t->sm_shader[pass]));
            AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(
                o, 0, pass == 1 ? t->sm_tex[0] : t->aa_tex));
            if (pass == 1) {
                AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 1, t->sm_area));
                AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 2, t->sm_search));
            }
            if (pass == 2) {
                AA_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 1, t->sm_tex[1]));
            }
            AA_DO(((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
                o, 5, 2, quad, sizeof(quad[0])));
        }
    } else {
        float c[8] = {1.0f / (float)desc.w,
                      1.0f / (float)desc.h,
                      (float)aa_relative / 1000.0f,
                      (float)aa_minimum / 1000.0f,
                      (float)aa_span,
                      0.125f,
                      0.0078125f,
                      (float)aa_strength / 100.0f};
        AA_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 109))(o, 0, c, 2));
        step = "FXAA draw";
        AA_DO(((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
            o, 5, 2, quad, sizeof(quad[0])));
    }
    t->aa_passes++;
    if (t->aa_passes == 1 || t->aa_passes % 600 == 0) {
        log_begin(aa_method == 2 ? "SMAA 1x pass" : "AA FXAA-style pass");
        log_field("width", desc.w);
        log_field("height", desc.h);
        log_field("passes", t->aa_passes);
        log_hex_field("result", hr);
        log_emit();
    }
    goto done;
failed:
    log_begin("AA disabled after failure");
    log_text(" step=");
    log_text(step);
    log_hex_field("result", hr);
    log_emit();
    t->aa_failed = 1;
done:
    if (begun) {
        INT result = ((INT(API *)(PVOID))original_method(o, 42))(o);
        if (result < 0) {
            restore = result;
        }
    }
    if (modified) {
        INT result = ((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, 0);
        if (result < 0) {
            restore = result;
        }
        for (UINT i = 0; i < 4; i++) {
            if (i == 0 || saved[i]) {
                result = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, i, saved[i]);
                if (result < 0) {
                    restore = result;
                }
            }
        }
        result = ((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, depthbuf);
        if (result < 0) {
            restore = result;
        }
        if (block) {
            PVOID *v = *(PVOID **)block;
            result = ((INT(API *)(PVOID))v[5])(block);
            if (result < 0) {
                restore = result;
            }
        }
        result = ((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &oldvp);
        if (result < 0) {
            restore = result;
        }
    }
    if (restore < 0) {
        log_begin("AA state restoration failed");
        log_hex_field("result", restore);
        log_emit();
        t->aa_failed = 1;
    }
    com_release(block);
    for (UINT i = 0; i < 4; i++) {
        com_release(saved[i]);
    }
    com_release(depthbuf);
    com_release(back);
    t->aa_busy = 0;
    if (t->aa_failed) {
        aa_cleanup(t);
    }
#undef AA_DO
}
