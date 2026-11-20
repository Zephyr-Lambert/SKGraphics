/* Read-only, bounded runtime evidence for the next shadow-filter implementation.
 * Map identity comes from the validated CShadowMap::Begin call, not dimensions.
 * GetContainer references are released before Reset/device replacement.
 */
static const UINT probe_texture_iid[4] = {0x85c31227, 0x4f003de5, 0x1af13a9b, 0xb5188cc3};
static UINT probe_code[4096], probe_hashes[32], probe_sizes[32], probe_codes;
static UINT probe_pairs[128][4], probe_pairs_count;

static void probe_cleanup(DeviceState *t) {
    if (!t) {
        return;
    }
    com_release(t->probe_color);
    com_release(t->probe_depth);
    t->probe_color = t->probe_depth = 0;
    t->shadow_pass = 0;
}

static void probe_surface(DeviceState *t, PVOID surface, UINT depth) {
    if (!shadow_probe_enabled || !surface || !t) {
        return;
    }
    PVOID tex = 0;
    PVOID *v = *(PVOID **)surface;
    INT hr = ((INT(API *)(PVOID, const void *, PVOID *))v[11])(surface, probe_texture_iid, &tex);
    PVOID *dst = depth ? &t->probe_depth : &t->probe_color;
    if (hr < 0 || !tex) {
        com_release(tex);
        return;
    }
    if (*dst == tex) {
        com_release(tex);
        return;
    }
    com_release(*dst);
    *dst = tex;
    SurfaceDesc d = {0};
    v = *(PVOID **)tex;
    hr = ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(tex, 0, &d);
    log_begin("ShadowProbe map identified");
    log_field("depth", depth);
    log_field("width", d.w);
    log_field("height", d.h);
    log_field("format", d.format);
    log_hex_field("result", hr);
    log_emit();
}

static UINT probe_shader(PVOID shader, UINT vertex) {
    if (!shader) {
        return 0;
    }
    UINT bytes = 0;
    PVOID *v = *(PVOID **)shader;
    INT hr = ((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, 0, &bytes);
    if (hr < 0 || bytes < 8 || bytes > sizeof(probe_code) || bytes % 4) {
        return 0xffffffff;
    }
    UINT size = bytes;
    hr = ((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, probe_code, &size);
    if (hr < 0 || size != bytes) {
        return 0xffffffff;
    }
    UINT hash = 2166136261;
    for (UINT i = 0; i < bytes / 4; i++) {
        hash ^= probe_code[i];
        hash *= 16777619;
    }
    for (UINT i = 0; i < probe_codes; i++) {
        if (probe_hashes[i] == hash && probe_sizes[i] == bytes) {
            return i + 1;
        }
    }
    if (probe_codes == 32) {
        return 0xffffffff;
    }
    UINT id = ++probe_codes;
    probe_hashes[id - 1] = hash;
    probe_sizes[id - 1] = bytes;
    log_begin("ShadowProbe shader");
    log_field("id", id);
    log_field("vertex", vertex);
    log_field("bytes", bytes);
    log_hex_field("hash", hash);
    log_emit();
    for (UINT i = 0; i < bytes / 4; i += 24) {
        log_begin("ShadowProbe code");
        log_field("id", id);
        log_field("word", i);
        for (UINT j = i; j < bytes / 4 && j < i + 24; j++) {
            log_text(" ");
            log_hex_number(probe_code[j]);
        }
        log_emit();
    }
    return id;
}

static void probe_draw(PVOID o) {
    DeviceState *t = device_state(o);
    /* One sampled frame per 60 presents; never retain shaders or change state.
  * A hard query cap also bounds unusual apps that draw without presenting. */
    if (!shadow_probe_enabled || !t || t->aa_busy || (!t->probe_color && !t->probe_depth) ||
        frame_count % 60 || t->probe_checks >= 100000 || log_events + 400 >= log_limit) {
        return;
    }
    t->probe_checks++;
    UINT color = 0, depth = 0;
    for (UINT stage = 0; stage < 16; stage++) {
        PVOID tex = 0;
        INT hr = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, stage, &tex);
        if (hr >= 0 && tex) {
            if (tex == t->probe_color) {
                color |= 1u << stage;
            }
            if (tex == t->probe_depth) {
                depth |= 1u << stage;
            }
        }
        com_release(tex);
    }
    if (!t->shadow_pass && !color && !depth) {
        return;
    }
    PVOID ps = 0, vs = 0;
    INT hp = ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps);
    INT hv = ((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &vs);
    UINT pi = hp >= 0 ? probe_shader(ps, 0) : 0xffffffff,
         vi = hv >= 0 ? probe_shader(vs, 1) : 0xffffffff;
    com_release(ps);
    com_release(vs);
    UINT role = (t->shadow_pass ? 1u : 0u) | (color << 1);
    UINT found = 0;
    for (UINT i = 0; i < probe_pairs_count; i++) {
        if (probe_pairs[i][0] == pi && probe_pairs[i][1] == vi && probe_pairs[i][2] == role &&
            probe_pairs[i][3] == depth) {
            found = 1;
            break;
        }
    }
    if (found || probe_pairs_count == 128) {
        return;
    }
    UINT *p = probe_pairs[probe_pairs_count++];
    p[0] = pi;
    p[1] = vi;
    p[2] = role;
    p[3] = depth;
    log_begin("ShadowProbe draw");
    log_field("pixelShader", pi);
    log_field("vertexShader", vi);
    log_field("writer", t->shadow_pass);
    log_hex_field("colorSamplerMask", color);
    log_hex_field("depthSamplerMask", depth);
    log_emit();
    const UINT rs[] = {7, 14, 15, 19, 20, 22, 27, 168, 174, 175, 195};
    for (UINT i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) {
        UINT value = 0;
        INT hr = ((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, rs[i], &value);
        log_begin("ShadowProbe renderState");
        log_field("type", rs[i]);
        log_hex_field("value", value);
        log_hex_field("result", hr);
        log_emit();
    }
    for (UINT stage = 0; stage < 16; stage++) {
        if ((color | depth) & (1u << stage)) {
            log_begin("ShadowProbe sampler");
            log_field("stage", stage);
            const UINT types[] = {1, 2, 5, 6, 7, 10, 11};
            const char *names[] = {"addressU", "addressV",      "mag", "min",
                                   "mip",      "maxAnisotropy", "sRGB"};
            for (UINT j = 0; j < 7; j++) {
                UINT value = 0;
                INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT *))original_method(o, 68))(
                    o, stage, types[j], &value);
                log_field(names[j], hr >= 0 ? value : 0xffffffff);
            }
            log_emit();
        }
    }
}
