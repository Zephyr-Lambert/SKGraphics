/* Verified GOG/Steam shadow-size patches. All addresses are RVAs in the loaded EXE.
 * Only two PUSH imm32 operands change. The game receives the new dimensions
 * in CShadowMap::Init, stores them and creates both shadow resources itself.
 * Build profiles also select texture imports and scene-boundary call sites.
 * No on-disk EXE writes, detours, shader substitution or blanket RT resizing.
 */
static UINT shadow_enabled = 1, shadow_size = 4096, shadow_actual = 2048;
static INT shadow_attempted, shadow_patched;
static unsigned char *shadow_image;
static const unsigned char shadow_site[] = {0x68, 0,    8, 0,    0,    0x68, 0,    8,   0,
                                            0,    0x6a, 0, 0xe8, 0x4a, 0x73, 0xfc, 0xff};

static UINT sh_u32(const unsigned char *p) {
    return (UINT)p[0] | ((UINT)p[1] << 8) | ((UINT)p[2] << 16) | ((UINT)p[3] << 24);
}

static void sh_put32(unsigned char *p, UINT n) {
    for (UINT i = 0; i < 4; i++) {
        p[i] = (unsigned char)(n >> (8 * i));
    }
}

static INT sh_equal(const unsigned char *a, const unsigned char *b, UINT count) {
    for (UINT i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static INT shadow_valid_size(UINT n) {
    return n == 1024 || n == 2048 || n == 4096 || n == 8192 || n == 12288 || n == 16384;
}

typedef struct {
    const char *name;
    UINT timestamp, image_size, shadow_rva, binding_rva, texture_iat;
    UINT fade_rva, copy_rva, scene_copy_rva;
    const unsigned char *site;
    UINT site_size;
} GameBuild;

static const unsigned char steam_shadow_site[] = {0x68, 0, 8,    0,    0,    0x68, 0, 8,
                                                  0,    0, 0x8b, 0xc8, 0xff, 0x52, 4};
static const GameBuild game_gog = {"GOG 01.06",        0x62f1e445, 0xe4e000, 0x54845, 0x1b985,
                                   0x3463b8,           0xd6ef,     0x18a03,  0xe4bb,  shadow_site,
                                   sizeof(shadow_site)};
static const GameBuild game_steam = {"Steam 01.06",
                                     0x62f1e22e,
                                     0xe63000,
                                     0x1b0cb1,
                                     0x1b0a1e,
                                     0x351374,
                                     0x1a40bb,
                                     0x1ae973,
                                     0x1a4ed1,
                                     steam_shadow_site,
                                     sizeof(steam_shadow_site)};
static const GameBuild *game_build = &game_gog;
static INT game_reported;

static INT shadow_identify(unsigned char *b) {
    if (!b || b[0] != 'M' || b[1] != 'Z') {
        return 0;
    }
    UINT pe = sh_u32(b + 60);
    if (pe < 64 || pe > 0x1000) {
        return 0;
    }
    if (sh_u32(b + pe) != 0x4550 || b[pe + 4] != 0x4c || b[pe + 5] != 1 || b[pe + 24] != 0x0b ||
        b[pe + 25] != 1) {
        return 0;
    }
    const GameBuild *g = 0;
    UINT stamp = sh_u32(b + pe + 8), size = sh_u32(b + pe + 24 + 56);
    if (stamp == game_gog.timestamp && size == game_gog.image_size) {
        g = &game_gog;
    } else if (stamp == game_steam.timestamp && size == game_steam.image_size) {
        g = &game_steam;
    }
    if (!g || !sh_equal(b + g->shadow_rva, g->site, g->site_size)) {
        return 0;
    }
    if (g == &game_steam) {
        /* Loaded code, not protected on-disk bytes. Confirm each mapped D3D call.
   * These sequences contain no absolute addresses and survive relocation. */
        static const unsigned char setrt[] = {0xff, 0x91, 0x94, 0, 0, 0};
        static const unsigned char draw[] = {0xff, 0x91, 0x48, 1, 0, 0};
        if (!sh_equal(b + g->binding_rva - 6, setrt, 6) ||
            !sh_equal(b + g->fade_rva - 6, draw, 6) || !sh_equal(b + g->copy_rva - 6, draw, 6) ||
            !sh_equal(b + g->scene_copy_rva - 6, draw, 6)) {
            return 0;
        }
    }
    game_build = g;
    if (!game_reported) {
        log_begin("Supported executable identified");
        log_text(" build=");
        log_text(g->name);
        log_hex_field("timestamp", stamp);
        log_emit();
        game_reported = 1;
    }
    return 1;
}

static INT shadow_write(unsigned char *b, UINT size) {
    unsigned char *p = b + game_build->shadow_rva;
    UINT old, unused;
    if (!sh_equal(p, game_build->site, game_build->site_size)) {
        return 0;
    }
    if (!VirtualProtect(p, game_build->site_size, 0x40, &old)) {
        log_begin("Shadow patch skipped: page protection failed");
        log_emit();
        return 0;
    }
    sh_put32(p + 1, size);
    sh_put32(p + 6, size);
    if (!FlushInstructionCache(GetCurrentProcess(), p, game_build->site_size)) {
        sh_put32(p + 1, 2048);
        sh_put32(p + 6, 2048);
        FlushInstructionCache(GetCurrentProcess(), p, game_build->site_size);
        if (!VirtualProtect(p, game_build->site_size, old, &unused)) {
            log_begin("Shadow patch ERROR: page protection restoration failed");
            log_emit();
        }
        log_begin(
            "Shadow patch skipped: instruction cache flush failed; original operands restored");
        log_emit();
        return 0;
    }
    if (!VirtualProtect(p, game_build->site_size, old, &unused)) {
        /* The writable mapping remains ours, so undo operands before a retry. */
        sh_put32(p + 1, 2048);
        sh_put32(p + 6, 2048);
        FlushInstructionCache(GetCurrentProcess(), p, game_build->site_size);
        if (!VirtualProtect(p, game_build->site_size, old, &unused)) {
            log_begin("Shadow patch ERROR: page protection restoration failed");
            log_emit();
        }
        log_begin(
            "Shadow patch skipped: could not restore page protection; original operands restored");
        log_emit();
        return 0;
    }
    return 1;
}

static void shadow_setup(PVOID o) {
    if (shadow_attempted) {
        return;
    }
    shadow_attempted = 1;
    log_begin("Shadow configuration");
    log_field("enabled", shadow_enabled);
    log_field("requestedSize", shadow_size);
    log_field("filter", shadow_filter);
    log_field("filterRadiusHundredths", shadow_filter_radius);
    log_emit();
    unsigned char *b = GetModuleHandleW(0);
    if (!shadow_identify(b)) {
        log_begin(
            "Shadow patch/probe skipped: executable signature does not match a supported build");
        log_emit();
        return;
    }
    shadow_image = b;
    if (!shadow_enabled || shadow_size == 2048) {
        return;
    }
    if (!shadow_valid_size(shadow_size)) {
        log_begin("Shadow patch skipped: use 1024, 2048, 4096, 8192, 12288 or 16384");
        log_emit();
        return;
    }
    /* D3DCAPS9: MaxTextureWidth / MaxTextureHeight are DWORDs 22 / 23.
  * Buffer exceeds the complete structure; no version-specific SDK dependency. */
    UINT caps[128] = {0};
    INT hr = ((INT(API *)(PVOID, PVOID))original_method(o, 7))(o, caps);
    if (hr < 0 || caps[22] < shadow_size || caps[23] < shadow_size) {
        log_begin("Shadow patch skipped: device texture limits");
        log_field("maxWidth", caps[22]);
        log_field("maxHeight", caps[23]);
        log_hex_field("result", hr);
        log_emit();
        return;
    }
    /* Preflight BOTH resources together, including their level-zero surfaces.
  * Release before the game performs the real allocations. Not a permanent
  * memory guarantee; later runtime allocation failures are still logged. */
    PVOID tex[2] = {0}, surfaces[2] = {0};
    UINT formats[2] = {75, 22}, usage[2] = {2, 1};
    INT ok = 1;
    for (UINT i = 0; i < 2 && ok; i++) {
        hr =
            ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *, PVOID))original_method(
                o, 23))(o, shadow_size, shadow_size, 1, usage[i], formats[i], 0, &tex[i], 0);
        if (hr >= 0 && tex[i]) {
            PVOID *v = *(PVOID **)tex[i];
            hr = ((INT(API *)(PVOID, UINT, PVOID *))v[18])(tex[i], 0, &surfaces[i]);
        }
        ok = hr >= 0 && tex[i] && surfaces[i];
        log_begin("Shadow allocation preflight");
        log_field("size", shadow_size);
        log_field("format", formats[i]);
        log_hex_field("result", hr);
        log_field("success", ok);
        log_emit();
    }
    for (UINT i = 0; i < 2; i++) {
        com_release(surfaces[i]);
        com_release(tex[i]);
    }
    if (!ok) {
        log_begin("Shadow patch skipped: preflight failed; using original 2048 size");
        log_emit();
        return;
    }
    if (!shadow_write(b, shadow_size)) {
        return;
    }
    shadow_image = b;
    shadow_actual = shadow_size;
    shadow_patched = 1;
    log_begin("Shadow size patch applied");
    log_field("originalSize", 2048);
    log_field("newSize", shadow_actual);
    log_hex_field("siteRVA", game_build->shadow_rva);
    log_emit();
}

static INT shadow_binding(PVOID caller) {
    return shadow_image && caller == (PVOID)(shadow_image + game_build->binding_rva);
}

static void shadow_draw_sample(PVOID o, INT hr) {
    if (!log_file || log_events >= log_limit) {
        return;
    }
    DeviceState *t = device_state(o);
    if (!t || !t->shadow_pass) {
        return;
    }
    if (hr >= 0 && t->shadow_sampled && frame_count - t->shadow_frame < 600) {
        return;
    }
    t->shadow_sampled = 1;
    t->shadow_frame = frame_count;
    Viewport vp = {0};
    Rect rect = {0};
    INT v = ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &vp);
    INT sc = ((INT(API *)(PVOID, Rect *))original_method(o, 76))(o, &rect);
    /* Sampled every 600 frames, but only logged when something differs. */
    if (!log_signature_is_new(12, shadow_actual, vp.w, vp.h, (UINT)rect.right, (UINT)rect.bottom,
                              (UINT)hr, (UINT)(v | sc))) {
        return;
    }
    log_begin("Shadow draw actual");
    log_field("mapSize", shadow_actual);
    log_field("viewportWidth", vp.w);
    log_field("viewportHeight", vp.h);
    log_field("scissorRight", rect.right);
    log_field("scissorBottom", rect.bottom);
    log_hex_field("drawResult", hr);
    log_hex_field("viewportResult", v);
    log_hex_field("scissorResult", sc);
    log_emit();
}
