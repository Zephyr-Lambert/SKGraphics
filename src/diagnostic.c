/* SKGraphics 2.0 (in development). Injector code plus MIT-licensed SMAA; no game assets included.
 * Win32/i386 ABI, freestanding C. Windowed full-frame supersampling, with bounded diagnostics.
 */
typedef unsigned int UINT;
typedef int INT;
typedef unsigned short WCHAR;
typedef void *PVOID;
#define API __attribute__((stdcall))
extern UINT API GetPrivateProfileStringW(const WCHAR *, const WCHAR *, const WCHAR *, WCHAR *, UINT,
                                         const WCHAR *);
extern UINT API GetPrivateProfileSectionNamesW(WCHAR *, UINT, const WCHAR *);
extern UINT API GetPrivateProfileSectionW(const WCHAR *, WCHAR *, UINT, const WCHAR *);
extern UINT API GetFileAttributesW(const WCHAR *);
extern INT API GetFileAttributesExW(const WCHAR *, INT, PVOID);
extern INT API WritePrivateProfileStringW(const WCHAR *, const WCHAR *, const WCHAR *,
                                          const WCHAR *);
extern INT API CreateDirectoryW(const WCHAR *, PVOID);
extern INT API DeleteFileW(const WCHAR *);
extern void API InitializeCriticalSection(PVOID);
extern void API EnterCriticalSection(PVOID);
extern void API LeaveCriticalSection(PVOID);
extern PVOID API GetModuleHandleW(const WCHAR *);
extern PVOID API GetCurrentProcess(void);
extern INT API FlushInstructionCache(PVOID, const void *, UINT);
extern UINT API GetSystemDirectoryW(WCHAR *, UINT);
extern PVOID API LoadLibraryW(const WCHAR *);
extern PVOID API GetProcAddress(PVOID, const char *);
extern UINT API GetModuleFileNameW(PVOID, WCHAR *, UINT);
extern PVOID API CreateFileW(const WCHAR *, UINT, UINT, PVOID, UINT, UINT, PVOID);
extern INT API WriteFile(PVOID, const void *, UINT, UINT *, PVOID);
extern INT API VirtualProtect(PVOID, UINT, UINT, UINT *);
extern INT API CloseHandle(PVOID);
extern UINT API GetPrivateProfileIntW(const WCHAR *, const WCHAR *, INT, const WCHAR *);
static PVOID self_module, backend_module, log_file, d3dx_module;

static PVOID d3dx_library(void) {
    static const WCHAR dll[] = {'d', '3', 'd', 'x', '9', '_', '4', '3', '.', 'd', 'l', 'l', 0};
    if (!d3dx_module) {
        d3dx_module = LoadLibraryW(dll);
    }
    return d3dx_module;
}

static UINT logging_enabled;
static UINT load_reshade, character_preview, ao_exclude_characters;
static UINT log_events, log_limit = 4000, frame_count;
static INT initialized;
static UINT render_enabled = 1, render_width = 3840, render_height = 2160;
static UINT aa_method = 1, aa_span = 8, aa_relative = 125, aa_minimum = 31, aa_strength = 100;
static UINT smaa_quality = 2;
static UINT shadow_enabled, shadow_size;
static UINT shadow_probe_enabled = 0;
static UINT background_probe = 0, ao_exclude_backgrounds = 0;
static UINT circle_preview = 0, actor_shadows = 0;
static UINT cpu_shadow_test = 0, shadow_coverage = 0, crowd_shadow_test = 0;
static UINT scene_probe_enabled = 0, ao_probe_enabled = 0, actor_shadow_probe = 0;
static UINT ao_depth_view = 0, ao_depth_scale = 100;
static UINT ao_enabled = 0, ao_strength = 100, ao_radius = 30, ao_quality = 2, ao_bias = 10,
            ao_max_distance = 0;
static UINT bloom_enabled = 0, bloom_strength = 35, bloom_threshold = 750, bloom_knee = 100,
            bloom_radius = 100, bloom_composite = 0, bloom_brightness = 0;
static UINT shadow_filter = 1, shadow_filter_radius = 200;
static UINT outline_enabled = 1, outline_thickness = 100;
static UINT outline_color_override = 0, outline_red = 0, outline_green = 0, outline_blue = 0,
            outline_opacity = 100;
static UINT texture_dump_enabled, texture_replace_enabled, texture_own_size,
    texture_dump_limit = 2000, texture_dump_mb = 512;
static void textures_install(void);
static UINT movie_enabled, movie_percent;
static void movie_install(void);
static void category_read_options(void);
static WCHAR path[32768];
/* The INI path is kept separately: path[] is reused for the log and backend paths. */
static WCHAR ini_path[32768];
static char log_line[1024];
static UINT log_used;

static void log_text(const char *v) {
    while (v && *v && log_used < 1020) {
        log_line[log_used++] = *v++;
    }
}

static void log_number(UINT v) {
    char b[11];
    UINT j = 0;
    do {
        b[j++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (j && log_used < 1020) {
        log_line[log_used++] = b[--j];
    }
}

static void log_hex_number(UINT v) {
    log_text("0x");
    for (INT j = 7; j >= 0; j--) {
        UINT d = (v >> (j * 4)) & 15;
        if (log_used < 1020) {
            log_line[log_used++] = "0123456789abcdef"[d];
        }
    }
}

static void log_field(const char *key, UINT value) {
    log_text(" ");
    log_text(key);
    log_text("=");
    log_number(value);
}

static void log_hex_field(const char *key, UINT value) {
    log_text(" ");
    log_text(key);
    log_text("=");
    log_hex_number(value);
}

static void log_begin(const char *name) {
    log_used = 0;
    log_text(name);
    log_field("frame", frame_count);
}

static void log_emit(void) {
    UINT written;
    if (!log_file || log_events >= log_limit) {
        return;
    }
    log_text("\r\n");
    WriteFile(log_file, log_line, log_used, &written, 0);
    log_events++;
    if (log_events == log_limit) {
        const char msg[] = "Log event limit reached.\r\n";
        WriteFile(log_file, msg, sizeof(msg) - 1, &written, 0);
    }
}

static void path_set_suffix(UINT pos, const char *a) {
    if (pos >= 32768) {
        return;
    }
    while (*a && pos < 32767) {
        path[pos++] = (WCHAR)*a++;
    }
    path[pos] = 0;
}

static UINT config_int(const char *section, const char *key, UINT def) {
    WCHAR a[64], b[64];
    UINT i = 0;
    for (; section[i] && i < 63; i++) {
        a[i] = section[i];
    }
    a[i] = 0;
    i = 0;
    for (; key[i] && i < 63; i++) {
        b[i] = key[i];
    }
    b[i] = 0;
    return GetPrivateProfileIntW(a, b, (INT)def, ini_path);
}

/* Live reload. Settings read here may change while the game runs; everything
 * else in initialize() is read once at startup. Bloom and AO can only be
 * switched on live if they were on at startup, because the game must be set
 * up for them while it loads (shader tracking, AO depth buffer). */
static UINT live_reload = 1, settings_loaded, bloom_available, ao_available;
static UINT live_restart_wanted, actor_collect_only;
/* 1 = bloom/AO keep game state with a D3D state block (1.x behavior)
 * instead of saving it by hand. Troubleshooting only (see bloom.h). */
static UINT bloom_state_block;
static UINT menu_dirty; /* in-game menu has unsaved changes */
static UINT menu_enabled = 1, menu_vk = 0x78; /* in-game menu, F9 */
static UINT menu_controller = 1;              /* D-pad and button combo */
static void menu_pending_load(void);
static UINT live_stamp[3], live_pending[3], live_has_stamp, live_waiting;
static INT live_read_stamp(UINT *out);

/* Internal actor-shadow modes follow the ActorShadows selector. */
static void actor_shadows_derive(void) {
    cpu_shadow_test = actor_shadows ? (actor_collect_only ? 2 : 1) : 0;
    crowd_shadow_test = actor_shadows == 2;
}

static void config_read_live(void) {
    movie_percent = config_int("Movies", "Volume", 100);
    if (movie_percent > 100) {
        movie_percent = 100;
    }
    aa_method = config_int("Antialiasing", "Method", 0);
    if (aa_method > 2) {
        aa_method = 0;
    }
    aa_span = config_int("Antialiasing", "Span", 8);
    if (aa_span < 1) {
        aa_span = 1;
    }
    if (aa_span > 16) {
        aa_span = 16;
    }
    aa_relative = config_int("Antialiasing", "EdgeThreshold", 125);
    if (aa_relative > 1000) {
        aa_relative = 1000;
    }
    aa_minimum = config_int("Antialiasing", "MinimumThreshold", 31);
    if (aa_minimum > 1000) {
        aa_minimum = 1000;
    }
    aa_strength = config_int("Antialiasing", "Strength", 100);
    if (aa_strength > 100) {
        aa_strength = 100;
    }
    live_restart_wanted = 0;
    UINT bloom_wanted = config_int("Bloom", "Enabled", 0) != 0;
    if (settings_loaded && bloom_wanted && !bloom_available) {
        live_restart_wanted |= 1;
        bloom_wanted = 0;
    }
    bloom_enabled = bloom_wanted;
    bloom_strength = config_int("Bloom", "Strength", 35);
    if (bloom_strength > 200) {
        bloom_strength = 200;
    }
    bloom_threshold = config_int("Bloom", "Threshold", 750);
    if (bloom_threshold > 1000) {
        bloom_threshold = 1000;
    }
    bloom_knee = config_int("Bloom", "Knee", 100);
    if (bloom_knee > 500) {
        bloom_knee = 500;
    }
    bloom_radius = config_int("Bloom", "Radius", 100);
    if (bloom_radius < 25) {
        bloom_radius = 25;
    }
    if (bloom_radius > 400) {
        bloom_radius = 400;
    }
    /* 0 = add (original), 1 = screen, 2 = tone-mapped. */
    bloom_composite = config_int("Bloom", "Composite", 0);
    if (bloom_composite > 2) {
        bloom_composite = 0;
    }
    /* 0 = brightest color channel (original), 1 = luminance. */
    bloom_brightness = config_int("Bloom", "Brightness", 0) != 0;
    bloom_state_block = config_int("Diagnostics", "BloomStateBlock", 0) != 0;
    /* Wider light-space shadow area: 0 = original, 1 = 2x, 2 = 3x. */
    shadow_coverage = config_int("Shadows", "Coverage", 0);
    if (shadow_coverage > 2) {
        shadow_coverage = 0;
    }
    /* 0 = player only (original), 1 = also CPU playable characters, 2 = also crowd enemies. */
    actor_shadows = config_int("Shadows", "ActorShadows", 0);
    if (actor_shadows > 2) {
        actor_shadows = 0;
    }
    /* Troubleshooting: collect extra shadow draws but never draw them,
     * to separate collection cost from drawing cost. */
    actor_collect_only = config_int("Diagnostics", "ActorShadowCollectOnly", 0) != 0;
    actor_shadows_derive();
    ao_exclude_backgrounds =
        config_int("AmbientOcclusion", "ExcludeTransformationBackgrounds", 0) != 0;
    ao_exclude_characters = config_int("AmbientOcclusion", "ExcludeCharacters", 0);
    UINT ao_wanted = config_int("AmbientOcclusion", "Enabled", 0);
    if (settings_loaded && ao_wanted && !ao_available) {
        live_restart_wanted |= 2;
        ao_wanted = 0;
    }
    ao_enabled = ao_wanted;
    ao_strength = config_int("AmbientOcclusion", "Strength", 100);
    if (ao_strength > 300) {
        ao_strength = 300;
    }
    ao_radius = config_int("AmbientOcclusion", "Radius", 30);
    if (ao_radius < 1) {
        ao_radius = 1;
    }
    if (ao_radius > 200) {
        ao_radius = 200;
    }
    ao_quality = config_int("AmbientOcclusion", "Quality", 2);
    if (ao_quality < 1) {
        ao_quality = 1;
    }
    if (ao_quality > 3) {
        ao_quality = 3;
    }
    ao_bias = config_int("AmbientOcclusion", "Bias", 10);
    if (ao_bias > 50) {
        ao_bias = 50;
    }
    ao_max_distance = config_int("AmbientOcclusion", "MaxDistance", 0);
    if (ao_max_distance > 100000) {
        ao_max_distance = 100000;
    }
}

/* Settings that only apply at startup. Raw INI values are remembered so a
 * live edit can be reported as needing a restart instead of silently ignored. */
static const char *const restart_keys[][2] = {{"Rendering", "Enabled"},
                                              {"Rendering", "Width"},
                                              {"Rendering", "Height"},
                                              {"Antialiasing", "SMAAQuality"},
                                              {"Shadows", "Enabled"},
                                              {"Shadows", "Resolution"},
                                              {"Shadows", "Filter"},
                                              {"Shadows", "FilterRadius"},
                                              {"Outlines", "Enabled"},
                                              {"Outlines", "Thickness"},
                                              {"Outlines", "ColorOverride"},
                                              {"Outlines", "Red"},
                                              {"Outlines", "Green"},
                                              {"Outlines", "Blue"},
                                              {"Outlines", "Opacity"},
                                              {"Textures", "Dump"},
                                              {"Textures", "Replace"},
                                              {"Textures", "UseReplacementSize"},
                                              {"Movies", "Enabled"},
                                              {"Compatibility", "LoadReShade"},
                                              {"AmbientOcclusion", "DepthView"},
                                              {"Diagnostics", "Logging"},
                                              {"Compatibility", "Menu"},
                                              {"Compatibility", "MenuKey"},
                                              {"Compatibility", "MenuController"}};
#define RESTART_KEY_COUNT (sizeof(restart_keys) / sizeof(restart_keys[0]))
#define RESTART_KEY_MISSING 0x7ffffffeu
static UINT restart_raw[RESTART_KEY_COUNT];

static void restart_keys_remember(void) {
    for (UINT i = 0; i < RESTART_KEY_COUNT; i++) {
        restart_raw[i] = config_int(restart_keys[i][0], restart_keys[i][1], RESTART_KEY_MISSING);
    }
}

static void initialize(void) {
    if (initialized) {
        return;
    }
    initialized = 1;
    UINT k = GetModuleFileNameW(self_module, path, 32768);
    if (!k || k >= 32768) {
        return;
    }
    if (k > 32700) {
        return;
    }
    while (k && path[k - 1] != '\\' && path[k - 1] != '/') {
        k--;
    }
    path_set_suffix(k, "SKGraphics.ini");
    for (UINT i = 0; i <= k + 14 && i < 32768; i++) {
        ini_path[i] = path[i];
    }
    static const WCHAR section[] = {'D', 'i', 'a', 'g', 'n', 'o', 's', 't', 'i', 'c', 's', 0};
    static const WCHAR key[] = {'M', 'a', 'x', 'L', 'o', 'g', 'E', 'v', 'e', 'n', 't', 's', 0};
    log_limit = GetPrivateProfileIntW(section, key, 4000, path);
    if (log_limit < 100) {
        log_limit = 100;
    }
    if (log_limit > 20000) {
        log_limit = 20000;
    }
    logging_enabled = config_int("Diagnostics", "Logging", 0) != 0;
    movie_enabled = config_int("Movies", "Enabled", 0) != 0;
    character_preview = config_int("Diagnostics", "CharacterPreview", 0);
    load_reshade = config_int("Compatibility", "LoadReShade", 0);
    render_enabled = config_int("Rendering", "Enabled", 0);
    render_width = config_int("Rendering", "Width", 3840);
    render_height = config_int("Rendering", "Height", 2160);
    smaa_quality = config_int("Antialiasing", "SMAAQuality", 2);
    if (smaa_quality > 3) {
        smaa_quality = 2;
    }
    shadow_enabled = config_int("Shadows", "Enabled", 0);
    shadow_size = config_int("Shadows", "Resolution", 4096);
    background_probe = config_int("Diagnostics", "BackgroundProbe", 0) != 0;
    scene_probe_enabled = config_int("Diagnostics", "SceneProbe", 0);
    circle_preview = config_int("Diagnostics", "CirclePreview", 0);
    actor_shadow_probe = config_int("Diagnostics", "ActorShadowProbe", 0) != 0;
    if (background_probe) {
        actor_shadow_probe = 1;
    }
    if (actor_shadow_probe) {
        scene_probe_enabled = 1;
    }
    ao_probe_enabled = config_int("Diagnostics", "AOProbe", 0);
    settings_loaded = 0;
    config_read_live();
    bloom_available = bloom_enabled;
    ao_available = ao_enabled;
    settings_loaded = 1;
    restart_keys_remember();
    live_reload = config_int("Compatibility", "LiveReload", 0) != 0;
    live_has_stamp = live_reload && live_read_stamp(live_stamp);
    menu_enabled = config_int("Compatibility", "Menu", 1) != 0;
    menu_controller = config_int("Compatibility", "MenuController", 1) != 0;
    {
        UINT f = config_int("Compatibility", "MenuKey", 9);
        menu_vk = 0x6f + (f >= 1 && f <= 12 ? f : 9); /* F1 = 0x70 */
    }
    ao_depth_view = config_int("AmbientOcclusion", "DepthView", 0);
    ao_depth_scale = config_int("AmbientOcclusion", "DepthScale", 100);
    if (ao_depth_scale < 1) {
        ao_depth_scale = 1;
    }
    if (ao_depth_scale > 100000) {
        ao_depth_scale = 100000;
    }
    shadow_probe_enabled = config_int("Diagnostics", "ShadowProbe", 0);
    shadow_filter = config_int("Shadows", "Filter", 0);
    if (shadow_filter > 2) {
        shadow_filter = 0;
    }
    shadow_filter_radius = config_int("Shadows", "FilterRadius", 200);
    if (shadow_filter_radius < 25) {
        shadow_filter_radius = 25;
    }
    if (shadow_filter_radius > 800) {
        shadow_filter_radius = 800;
    }
    outline_enabled = config_int("Outlines", "Enabled", 1);
    outline_thickness = config_int("Outlines", "Thickness", 100);
    if (outline_thickness < 25) {
        outline_thickness = 25;
    }
    if (outline_thickness > 400) {
        outline_thickness = 400;
    }
    outline_color_override = config_int("Outlines", "ColorOverride", 0);
    outline_red = config_int("Outlines", "Red", 0);
    if (outline_red > 255) {
        outline_red = 255;
    }
    outline_green = config_int("Outlines", "Green", 0);
    if (outline_green > 255) {
        outline_green = 255;
    }
    outline_blue = config_int("Outlines", "Blue", 0);
    if (outline_blue > 255) {
        outline_blue = 255;
    }
    outline_opacity = config_int("Outlines", "Opacity", 100);
    if (outline_opacity > 100) {
        outline_opacity = 100;
    }
    texture_dump_enabled = config_int("Textures", "Dump", 0);
    texture_replace_enabled = config_int("Textures", "Replace", 0);
    texture_own_size = config_int("Textures", "UseReplacementSize", 0);
    texture_dump_limit = config_int("Textures", "DumpLimit", 2000);
    if (texture_dump_limit > 10000) {
        texture_dump_limit = 10000;
    }
    texture_dump_mb = config_int("Textures", "MaxDumpMiB", 512);
    if (texture_dump_mb > 2048) {
        texture_dump_mb = 2048;
    }
    if (!logging_enabled) {
        scene_probe_enabled = ao_probe_enabled = shadow_probe_enabled = 0;
    }
    category_read_options();
    path_set_suffix(k, "SKGraphics.log");
    if (logging_enabled) {
        log_file = CreateFileW(path, 0x40000000, 1, 0, 2, 0x80, 0);
        if (log_file == (PVOID)-1) {
            log_file = 0;
        }
    }
    log_begin("SKGraphics 2.0");
    log_field("resolutionOverride", render_enabled);
    if (render_enabled) {
        log_field("configuredWidth", render_width);
        log_field("configuredHeight", render_height);
    }
    log_field("aaMethod", aa_method);
    log_field("smaaQuality", smaa_quality);
    log_emit();
    /* Load ReShade below our wrapper so it sees physical dimensions and its
  * internal device calls do not pass through the game's scaling hooks. */
    if (load_reshade) {
        path_set_suffix(k, "ReShade\\d3d9.dll");
        backend_module = LoadLibraryW(path);
        if (backend_module == self_module) {
            backend_module = 0;
        }
        if (backend_module && !GetProcAddress(backend_module, "Direct3DCreate9")) {
            backend_module = 0;
        }
        log_begin(backend_module ? "Backend loaded from ReShade subdirectory"
                                 : "ERROR: could not load ReShade backend; check ReShade/d3d9.dll");
        log_emit();
    } else {
        k = GetSystemDirectoryW(path, 32768);
        if (!k || k > 32750) {
            return;
        }
        path_set_suffix(k, "\\d3d9.dll");
        backend_module = LoadLibraryW(path);
        if (backend_module == self_module) {
            backend_module = 0;
        }
        log_begin(backend_module ? "Backend loaded from Windows system directory"
                                 : "ERROR: could not load system D3D9");
        log_emit();
    }
    movie_install();
    textures_install();
}

/* Patch each backend vtable once; retain originals by vtable, not by object.
 * AA-owned COM resources are released before reset or a new device creation.
 */
typedef struct {
    PVOID *vt;
    PVOID original[119];
    UINT count;
    UINT hooked[4];
} HookTable;

static HookTable tables[16];
static UINT table_count;

static HookTable *hook_table_find(PVOID o) {
    if (!o) {
        return 0;
    }
    PVOID *v = *(PVOID **)o;
    for (UINT i = 0; i < table_count; i++) {
        if (tables[i].vt == v) {
            return &tables[i];
        }
    }
    return 0;
}

static PVOID original_method(PVOID o, UINT slot) {
    HookTable *t = hook_table_find(o);
    return t ? t->original[slot] : 0;
}

static HookTable *hook_table_record(PVOID o, UINT count) {
    HookTable *t = hook_table_find(o);
    if (t) {
        return t;
    }
    if (table_count == 16) {
        return 0;
    }
    t = &tables[table_count++];
    t->vt = *(PVOID **)o;
    t->count = count;
    for (UINT i = 0; i < count; i++) {
        t->original[i] = t->vt[i];
    }
    return t;
}

/* Do not overwrite a wrapper installed above us on a later factory call.
 * Its saved trampoline may already point to our hook. Updating original[]
 * to that wrapper would create recursion; preserve both links instead.
 * Mark only successful installs, so a protection failure remains retryable. */
static void hook_install(HookTable *t, UINT slot, PVOID fn) {
    UINT old, unused;
    if (!t || slot >= t->count) {
        return;
    }
    UINT mask = 1u << (slot & 31);
    if (t->hooked[slot >> 5] & mask) {
        return;
    }
    if (VirtualProtect(t->vt + slot, 4, 0x40, &old)) {
        PVOID previous = t->vt[slot];
        t->vt[slot] = fn;
        if (VirtualProtect(t->vt + slot, 4, old, &unused)) {
            t->hooked[slot >> 5] |= mask;
        } else {
            t->vt[slot] = previous;
            VirtualProtect(t->vt + slot, 4, old, &unused);
            log_begin("ERROR: vtable hook rolled back after protection restoration failure");
            log_field("slot", slot);
            log_emit();
        }
    } else {
        log_begin("ERROR: vtable hook failed");
        log_field("slot", slot);
        log_emit();
    }
}

typedef struct {
    UINT w, h, format, count, ms, quality, swap;
    PVOID window;
    INT windowed, auto_depth;
    UINT depth, flags, refresh, interval;
} PresentParams;

typedef struct {
    UINT format, type, usage, pool, ms, quality, w, h;
} SurfaceDesc;

typedef struct {
    UINT x, y, w, h;
    float minz, maxz;
} Viewport;

typedef struct {
    INT left, top, right, bottom;
} Rect;

typedef struct {
    PVOID obj;
    UINT lw, lh, rw, rh;
    INT active, main;
    PVOID cm_tex, cm_surf;
    UINT cm_ready, cm_failed, cm_w, cm_h, cm_draws;
    PVOID ao_depth, ao_ps, ao_tex, ao_surf, ao_shaders[2];
    UINT ao_w, ao_h, ao_valid, ao_projection_warned, ao_failed;
    float ao_proj[8];
    UINT ao_passes, ao_missing;
    PVOID bl_tex[2], bl_surf[2], bl_ps[6], bl_scene, bl_bgtex, bl_bgsurf, bl_copytex, bl_copysurf;
    UINT bl_copyw, bl_copyh;
    UINT bl_bgw, bl_bgh, bl_early_ui, bl_mask;
    UINT bl_w, bl_h, bl_done, bl_failed, bl_passes;
    INT shadow_pass, shadow_sampled;
    UINT shadow_frame;
    PVOID probe_color, probe_depth;
    UINT probe_checks;
    PVOID sm_tex[2], sm_surface[2], sm_shader[3], sm_area, sm_search;
    UINT aa_cached_method;
    PVOID aa_tex, aa_surface, aa_shader;
    UINT aa_w, aa_h, aa_format, aa_passes;
    INT aa_busy, aa_failed;
} DeviceState;

static DeviceState states[8];
static UINT state_count;

static DeviceState *device_state(PVOID o) {
    for (UINT i = 0; i < state_count; i++) {
        if (states[i].obj == o) {
            return &states[i];
        }
    }
    return 0;
}

static DeviceState *device_state_new(PVOID o) {
    DeviceState *t = device_state(o);
    if (t) {
        return t;
    }
    for (UINT i = 0; i < state_count; i++) {
        if (!states[i].obj) {
            states[i].obj = o;
            return &states[i];
        }
    }
    if (state_count == 8) {
        return 0;
    }
    t = &states[state_count++];
    t->obj = o;
    return t;
}

static void select_size(DeviceState *t, PresentParams *p) {
    t->active = 0;
    t->main = 1;
    t->lw = p->w;
    t->lh = p->h;
    t->rw = p->w;
    t->rh = p->h;
    if (!render_enabled) {
        return;
    }
    if (!p->windowed || !p->w || !p->h || p->w > 16384 || p->h > 16384 || p->ms ||
        render_width < 320 || render_height < 180 || render_width > 16384 ||
        render_height > 16384) {
        log_begin("Resolution override disabled: unsupported mode or dimensions");
        log_emit();
        return;
    }
    /* The game always draws a 16:9 picture; on a screen of another shape
     * (borderless on 16:10 or ultrawide) it adds bars around it. When the
     * shapes differ, the configured size is enlarged on one axis to the
     * output's shape, so the scale is the same on both axes (no stretching)
     * and the picture inside the bars keeps the configured resolution. */
    UINT rw = render_width, rh = render_height; /* products stay below 2^29 */
    if (rw * p->h != rh * p->w) {
        if (rw * p->h > rh * p->w) {
            rh = (rw * p->h + p->w / 2) / p->w; /* output taller: add height */
        } else {
            rw = (rh * p->w + p->h / 2) / p->h; /* output wider: add width */
        }
        if (rw > 16384 || rh > 16384) {
            log_begin("Resolution override disabled: fitted size too large");
            log_field("width", rw);
            log_field("height", rh);
            log_emit();
            return;
        }
        log_begin("Rendering size fitted to output shape");
        log_field("outputWidth", p->w);
        log_field("outputHeight", p->h);
        log_field("width", rw);
        log_field("height", rh);
        log_emit();
    }
    t->rw = rw;
    t->rh = rh;
    t->active = (t->rw != t->lw || t->rh != t->lh);
}

static UINT scale(UINT v, UINT to, UINT from) {
    return (v * to + from / 2) / from;
}

static INT main_surface(DeviceState *t, PVOID target) {
    if (!t || !target) {
        return 0;
    }
    SurfaceDesc d = {0};
    PVOID *v = *(PVOID **)target;
    INT hr = ((INT(API *)(PVOID, SurfaceDesc *))v[12])(target, &d);
    return hr >= 0 && d.w == t->rw && d.h == t->rh;
}

static void resize(DeviceState *t, UINT *w, UINT *h) {
    if (t && t->active && *w == t->lw && *h == t->lh) {
        *w = t->rw;
        *h = t->rh;
    }
}

static const Rect *rectscale(DeviceState *t, const Rect *r, Rect *out) {
    if (!r || !t || !t->active) {
        return r;
    }
    *out = *r;
    out->left = (INT)scale((UINT)r->left, t->rw, t->lw);
    out->right = (INT)scale((UINT)r->right, t->rw, t->lw);
    out->top = (INT)scale((UINT)r->top, t->rh, t->lh);
    out->bottom = (INT)scale((UINT)r->bottom, t->rh, t->lh);
    return out;
}

static void log_present_params(const char *name, PresentParams *p) {
    log_begin(name);
    if (p) {
        log_field("width", p->w);
        log_field("height", p->h);
        log_field("format", p->format);
        log_field("windowed", p->windowed);
        log_field("autoDepth", p->auto_depth);
        log_field("depthFormat", p->depth);
        log_field("multisample", p->ms);
    }
    log_emit();
}

/* Unique bind/viewport signatures avoid logging thousands of identical passes. */
static UINT log_signatures[512][8], log_signature_count;

static INT log_signature_is_new(UINT a, UINT b, UINT c, UINT d, UINT e, UINT f, UINT g, UINT h) {
    if (!log_file || log_events >= log_limit) {
        return 0;
    }
    UINT z[8] = {a, b, c, d, e, f, g, h};
    for (UINT i = 0; i < log_signature_count; i++) {
        UINT j = 0;
        while (j < 8 && log_signatures[i][j] == z[j]) {
            j++;
        }
        if (j == 8) {
            return 0;
        }
    }
    if (log_signature_count == 512) {
        return 0;
    }
    for (UINT j = 0; j < 8; j++) {
        log_signatures[log_signature_count][j] = z[j];
    }
    log_signature_count++;
    return 1;
}

static void log_surface(const char *name, UINT kind, UINT slot, PVOID o, INT hr) {
    if (!log_file || log_events >= log_limit) {
        return;
    }
    SurfaceDesc d = {0};
    INT ok = -1;
    if (o) {
        PVOID *v = *(PVOID **)o;
        ok = ((INT(API *)(PVOID, SurfaceDesc *))v[12])(o, &d);
    }
    if (!log_file) {
        return;
    }
    if (log_signature_is_new(kind, slot, d.w, d.h, d.format, d.ms, (UINT)hr, (UINT)ok)) {
        log_begin(name);
        log_field("slot", slot);
        log_field("width", d.w);
        log_field("height", d.h);
        log_field("format", d.format);
        log_field("multisample", d.ms);
        log_hex_field("result", hr);
        log_emit();
    }
}

#include "aa_pass.h"
#include "shadows.h"
#include "movie_volume.h"
#include "texture_categories.h"
#include "textures.h"
#include "shadow_probe.h"
#include "shadow_filter.h"
#include "outlines.h"
#include "scene_probe.h"
static void character_cleanup(PVOID);
#include "ao_depth.h"
#include "transformation_background.h"
#include "bloom.h"
#include "character_preview.h"
#include "character_mask.h"
static void coverage_scale(float *);
#include "cpu_shadow.h"
#include "circle_preview.h"
#include "shadow_coverage.h"
static void menu_cleanup(void);

static INT API reset(PVOID o, PresentParams *p) {
    coverage_reset();
    tb_cleanup();
    circle_cleanup();
    cs_cleanup();
    menu_cleanup();
    if (scene_device == o) {
        scene_finish("Reset");
    }
    log_present_params("Reset requested", p);
    DeviceState *t = device_state(o);
    if (!p || !t) {
        return ((INT(API *)(PVOID, PresentParams *))original_method(o, 16))(o, p);
    }
    bloom_cleanup(t);
    t->bl_failed = 0;
    t->ao_failed = 0;
    t->cm_failed = 0;
    probe_cleanup(t);
    aa_cleanup(t);
    t->aa_failed = 0;
    DeviceState previous = *t;
    select_size(t, p);
    PresentParams request = *p;
    request.w = t->rw;
    request.h = t->rh;
    INT hr = ((INT(API *)(PVOID, PresentParams *))original_method(o, 16))(o, &request);
    log_begin("Reset result");
    log_hex_field("result", hr);
    log_emit();
    if (hr >= 0) {
        log_signature_count = 0;
        log_present_params("Reset actual", &request);
        *p = request;
        p->w = t->lw;
        p->h = t->lh;
    } else {
        *t = previous;
    }
    return hr;
}

/* Polls the INI's write time and size every 30 frames (about twice a second).
 * A change is applied once two polls agree, so a half-written save from an
 * editor is never read. Runs between frames on the render thread. */

static INT live_read_stamp(UINT *out) {
    UINT data[9];
    if (!GetFileAttributesExW(ini_path, 0, data)) {
        return 0;
    }
    out[0] = data[5];
    out[1] = data[6];
    out[2] = data[8];
    return 1;
}

/* Call after any live change to the shadow selectors (INI reload or menu). */
static void live_shadows_changed(UINT old_mode, UINT old_crowd, UINT old_coverage) {
    if (cpu_shadow_test != old_mode || crowd_shadow_test != old_crowd ||
        shadow_coverage != old_coverage) {
        /* Drop captured shadow draws so nothing recorded under old settings is
         * replayed. The crowd shader dump stays once per device session. */
        UINT reported = cs_crowd_reported;
        cs_cleanup();
        cs_crowd_reported = reported;
        coverage_reset();
    }
}

static void live_apply(void) {
    UINT old_mode = cpu_shadow_test, old_crowd = crowd_shadow_test, old_coverage = shadow_coverage;
    config_read_live();
    live_shadows_changed(old_mode, old_crowd, old_coverage);
    menu_pending_load();
    menu_dirty = 0;
    log_begin("Settings reloaded");
    log_field("bloom", bloom_enabled);
    log_field("bloomStrength", bloom_strength);
    log_field("bloomThreshold", bloom_threshold);
    log_field("bloomKnee", bloom_knee);
    log_field("bloomRadius", bloom_radius);
    log_field("bloomComposite", bloom_composite);
    log_field("bloomBrightness", bloom_brightness);
    log_field("ao", ao_enabled);
    log_field("aoStrength", ao_strength);
    log_field("aoRadius", ao_radius);
    log_field("aoQuality", ao_quality);
    log_field("aaMethod", aa_method);
    log_field("actorShadows", actor_shadows);
    log_field("coverage", shadow_coverage);
    log_field("movieVolume", movie_percent);
    log_emit();
    if (live_restart_wanted & 1) {
        log_begin("Restart needed");
        log_text(" setting=Bloom.Enabled reason=off-at-startup");
        log_emit();
    }
    if (live_restart_wanted & 2) {
        log_begin("Restart needed");
        log_text(" setting=AmbientOcclusion.Enabled reason=off-at-startup");
        log_emit();
    }
    for (UINT i = 0; i < RESTART_KEY_COUNT; i++) {
        UINT now = config_int(restart_keys[i][0], restart_keys[i][1], RESTART_KEY_MISSING);
        if (now != restart_raw[i]) {
            restart_raw[i] = now; /* report each change once */
            log_begin("Restart needed");
            log_text(" setting=");
            log_text(restart_keys[i][0]);
            log_text(".");
            log_text(restart_keys[i][1]);
            log_emit();
        }
    }
}

static void live_poll(void) {
    if (!live_reload || frame_count % 30) {
        return;
    }
    UINT now[3];
    if (!live_read_stamp(now)) {
        return;
    }
    if (!live_has_stamp) {
        live_has_stamp = 1;
        for (UINT i = 0; i < 3; i++) {
            live_stamp[i] = now[i];
        }
        return;
    }
    INT same_as_applied =
        now[0] == live_stamp[0] && now[1] == live_stamp[1] && now[2] == live_stamp[2];
    INT same_as_pending =
        now[0] == live_pending[0] && now[1] == live_pending[1] && now[2] == live_pending[2];
    if (same_as_applied) {
        live_waiting = 0;
        return;
    }
    if (live_waiting && same_as_pending) {
        live_waiting = 0;
        for (UINT i = 0; i < 3; i++) {
            live_stamp[i] = now[i];
        }
        live_apply();
        return;
    }
    live_waiting = 1;
    for (UINT i = 0; i < 3; i++) {
        live_pending[i] = now[i];
    }
}

#include "menu.h"

static INT API present(PVOID o, PVOID a, PVOID b, PVOID w, PVOID dirty) {
    DeviceState *t = device_state(o);
    scene_present(o);
    bloom_frame_end(t);
    aa_apply(t);
    menu_input();
    menu_draw(t);
    Rect r;
    const Rect *src = rectscale(t, (const Rect *)a, &r);
    INT hr = ((INT(API *)(PVOID, const Rect *, PVOID, PVOID, PVOID))original_method(o, 17))(
        o, src, b, w, dirty);
    cs_present(o, hr);
    frame_count++;
    live_poll();
    if (frame_count == 1 || frame_count == 120 || frame_count % 600 == 0) {
        log_begin("Present");
        log_hex_field("result", hr);
        if (t) {
            log_field("logicalWidth", t->lw);
            log_field("logicalHeight", t->lh);
            log_field("renderWidth", t->rw);
            log_field("renderHeight", t->rh);
            /* Feature counters appear only when the feature is active or has done work. */
            if (movie_installed) {
                log_field("movieVolumeCalls", movie_calls);
            }
            if (shadow_filter || sf_inspected) {
                log_field("shadowShadersInspected", sf_inspected);
                log_field("shadowShadersApplied", sf_matches);
                log_field("shadowShaderReadFailures", sf_read_failures);
            }
            if (ol_pixels || ol_vertices) {
                log_field("outlinePixelsReplaced", ol_pixels);
                log_field("outlineVerticesReplaced", ol_vertices);
            }
        }
        log_emit();
    }
    return hr;
}

static INT API texture(PVOID o, UINT w, UINT h, UINT levels, UINT usage, UINT fmt, UINT pool,
                       PVOID *out, PVOID shared) {
    if (usage & 3) {
        resize(device_state(o), &w, &h);
    }
    DeviceState *t = device_state(o);
    if ((ao_depth_view || ao_available) && shadow_image && t && w == t->rw && h == t->rh &&
        !(w == shadow_actual && h == shadow_actual) && usage == 2 && fmt == 75 && pool == 0 &&
        levels == 1 && !shared && out) {
        PVOID candidate = 0;
        INT test =
            ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *, PVOID))original_method(
                o, 23))(o, w, h, levels, usage, 0x5a544e49, pool, &candidate, shared);
        log_begin("AO INTZ depth allocation");
        log_field("width", w);
        log_field("height", h);
        log_hex_field("result", test);
        log_emit();
        if (test >= 0 && candidate) {
            *out = candidate;
            return test;
        }
        com_release(candidate);
    }
    INT hr =
        ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *, PVOID))original_method(
            o, 23))(o, w, h, levels, usage, fmt, pool, out, shared);
    /* Scene loads create dozens of identical targets; log each distinct one once. */
    if ((usage & 3) && log_signature_is_new(10, w, h, levels, usage, fmt, pool, (UINT)hr)) {
        log_begin("CreateTexture render/depth target");
        log_field("width", w);
        log_field("height", h);
        log_field("levels", levels);
        log_hex_field("usage", usage);
        log_field("format", fmt);
        log_hex_field("result", hr);
        log_emit();
    }
    return hr;
}

static INT API rt(PVOID o, UINT w, UINT h, UINT fmt, UINT ms, UINT quality, INT lockable,
                  PVOID *out, PVOID shared) {
    resize(device_state(o), &w, &h);
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, INT, PVOID *, PVOID))original_method(
        o, 28))(o, w, h, fmt, ms, quality, lockable, out, shared);
    log_begin("CreateRenderTarget");
    log_field("width", w);
    log_field("height", h);
    log_field("format", fmt);
    log_field("multisample", ms);
    log_hex_field("result", hr);
    log_emit();
    return hr;
}

static INT API depth(PVOID o, UINT w, UINT h, UINT fmt, UINT ms, UINT quality, INT discard,
                     PVOID *out, PVOID shared) {
    resize(device_state(o), &w, &h);
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, INT, PVOID *, PVOID))original_method(
        o, 29))(o, w, h, fmt, ms, quality, discard, out, shared);
    log_begin("CreateDepthStencilSurface");
    log_field("width", w);
    log_field("height", h);
    log_field("format", fmt);
    log_field("multisample", ms);
    log_hex_field("result", hr);
    log_emit();
    return hr;
}

static INT API setrt(PVOID o, UINT slot, PVOID target) {
    if (slot == 0) {
        cs_flush(o);
    }
    INT hr = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, slot, target);
    log_surface("SetRenderTarget", 1, slot, target, hr);
    DeviceState *t = device_state(o);
    if (t && slot == 0 && hr >= 0) {
        t->main = main_surface(t, target);
        t->shadow_pass = shadow_binding(__builtin_return_address(0));
        if (t->shadow_pass) {
            probe_surface(t, target, 0);
        }
    }
    return hr;
}

static INT API setdepth(PVOID o, PVOID target) {
    cs_flush(o);
    INT hr = ((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, target);
    log_surface("SetDepthStencilSurface", 2, 0, target, hr);
    DeviceState *t = device_state(o);
    if (hr >= 0 && t && t->shadow_pass) {
        probe_surface(t, target, 1);
    }
    return hr;
}

static INT API viewport(PVOID o, const Viewport *v) {
    DeviceState *t = device_state(o);
    Viewport adjusted;
    if (v && t && t->active && t->main) {
        adjusted = *v;
        adjusted.x = scale(v->x, t->rw, t->lw);
        adjusted.y = scale(v->y, t->rh, t->lh);
        adjusted.w = scale(v->w, t->rw, t->lw);
        adjusted.h = scale(v->h, t->rh, t->lh);
        v = &adjusted;
    }
    INT hr = ((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, v);
    if (v && log_signature_is_new(3, v->x, v->y, v->w, v->h, 0, 0, (UINT)hr)) {
        log_begin("SetViewport actual");
        log_field("x", v->x);
        log_field("y", v->y);
        log_field("width", v->w);
        log_field("height", v->h);
        log_hex_field("result", hr);
        log_emit();
    }
    return hr;
}

static INT API getviewport(PVOID o, Viewport *v) {
    INT hr = ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, v);
    DeviceState *t = device_state(o);
    if (hr >= 0 && v && t && t->active && t->main) {
        v->x = scale(v->x, t->lw, t->rw);
        v->y = scale(v->y, t->lh, t->rh);
        v->w = scale(v->w, t->lw, t->rw);
        v->h = scale(v->h, t->lh, t->rh);
    }
    return hr;
}

static INT API scissor(PVOID o, const Rect *r) {
    DeviceState *t = device_state(o);
    Rect out;
    const Rect *p = (t && t->main) ? rectscale(t, r, &out) : r;
    return ((INT(API *)(PVOID, const Rect *))original_method(o, 75))(o, p);
}

static INT API getscissor(PVOID o, Rect *r) {
    INT hr = ((INT(API *)(PVOID, Rect *))original_method(o, 76))(o, r);
    DeviceState *t = device_state(o);
    if (hr >= 0 && r && t && t->active && t->main) {
        r->left = scale(r->left, t->lw, t->rw);
        r->right = scale(r->right, t->lw, t->rw);
        r->top = scale(r->top, t->lh, t->rh);
        r->bottom = scale(r->bottom, t->lh, t->rh);
    }
    return hr;
}

static INT API stretch(PVOID o, PVOID src, PVOID sr, PVOID dst, PVOID dr, UINT filter) {
    DeviceState *t = device_state(o);
    Rect a, b;
    const Rect *srect = main_surface(t, src) ? rectscale(t, sr, &a) : sr;
    const Rect *drect = main_surface(t, dst) ? rectscale(t, dr, &b) : dr;
    INT hr = ((INT(API *)(PVOID, PVOID, const Rect *, PVOID, const Rect *, UINT))original_method(
        o, 34))(o, src, srect, dst, drect, filter);
    if (scene_recording(o)) {
        scene_begin("SceneProbe StretchRect");
        scene_surface(src, "source");
        scene_surface(dst, "destination");
        log_hex_field("result", hr);
        scene_emit();
    }
    log_surface("StretchRect source", 4, filter, src, hr);
    log_surface("StretchRect destination", 5, filter, dst, hr);
    return hr;
}

static UINT draw_sample_frame;
static INT draw_sampled;

static void draw_sample(PVOID o, INT hr) {
    if (!log_file || log_events >= log_limit) {
        return;
    }
    DeviceState *t = device_state(o);
    if (!t || !t->main) {
        return;
    }
    if (hr >= 0 && draw_sampled && frame_count - draw_sample_frame < 120) {
        return;
    }
    draw_sampled = 1;
    draw_sample_frame = frame_count;
    Viewport v = {0};
    INT ok = ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &v);
    /* Sampled every 120 frames, but only logged when something differs. */
    if (!log_signature_is_new(11, v.w, v.h, (UINT)hr, (UINT)ok, 0, 0, 0)) {
        return;
    }
    log_begin("Main draw actual viewport");
    log_field("width", v.w);
    log_field("height", v.h);
    log_hex_field("drawResult", hr);
    log_hex_field("viewportResult", ok);
    log_emit();
}

static INT API draw(PVOID o, UINT type, UINT start, UINT count) {
    bloom_before_draw(o, __builtin_return_address(0));
    scene_draw(o, 81, type, count, __builtin_return_address(0));
    scene_geometry(o, 81, 0, 0, count, start, 0);
    probe_draw(o);
    PVOID saved = character_before(o);
    CoverageDraw coverage;
    coverage_begin(o, &coverage);
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 81))(o, type, start, count);
    coverage_end(o, &coverage);
    character_after(o, saved);
    CharacterMaskState mask;
    if (hr >= 0 && cm_begin(o, &mask)) {
        INT replay =
            ((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 81))(o, type, start, count);
        cm_end(o, &mask, replay);
    }
    draw_sample(o, hr);
    shadow_draw_sample(o, hr);
    return hr;
}

static INT API indexed(PVOID o, UINT type, INT base, UINT min, UINT count, UINT start,
                       UINT primitives) {
    bloom_before_draw(o, __builtin_return_address(0));
    scene_draw(o, 82, type, primitives, __builtin_return_address(0));
    scene_geometry(o, 82, base, min, count, start, 0);
    probe_draw(o);
    UINT circle_skip = 0;
    PVOID saved = circle_before(o, type, count, primitives, &circle_skip);
    if (circle_skip) {
        return 0;
    }
    if (!saved) {
        saved = character_before(o);
    }
    CoverageDraw coverage;
    coverage_begin(o, &coverage);
    INT hr = ((INT(API *)(PVOID, UINT, INT, UINT, UINT, UINT, UINT))original_method(o, 82))(
        o, type, base, min, count, start, primitives);
    coverage_end(o, &coverage);
    if (hr >= 0) {
        cs_indexed(o, type, base, min, count, start, primitives);
    }
    character_after(o, saved);
    CharacterMaskState mask;
    if (hr >= 0 && cm_begin(o, &mask)) {
        INT replay = ((INT(API *)(PVOID, UINT, INT, UINT, UINT, UINT, UINT))original_method(o, 82))(
            o, type, base, min, count, start, primitives);
        cm_end(o, &mask, replay);
    }
    draw_sample(o, hr);
    shadow_draw_sample(o, hr);
    return hr;
}

static INT API drawup(PVOID o, UINT type, UINT count, const void *v, UINT stride) {
    bloom_before_draw(o, __builtin_return_address(0));
    scene_draw(o, 83, type, count, __builtin_return_address(0));
    scene_geometry(o, 83, 0, 0, 0, 0, stride);
    probe_draw(o);
    PVOID saved = character_before(o);
    CoverageDraw coverage;
    coverage_begin(o, &coverage);
    INT hr = ((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
        o, type, count, v, stride);
    coverage_end(o, &coverage);
    character_after(o, saved);
    CharacterMaskState mask;
    if (hr >= 0 && cm_begin(o, &mask)) {
        INT replay = ((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
            o, type, count, v, stride);
        cm_end(o, &mask, replay);
    }
    return hr;
}

static INT API indexedup(PVOID o, UINT type, UINT min, UINT count, UINT primitives,
                         const void *indices, UINT format, const void *vertices, UINT stride) {
    bloom_before_draw(o, __builtin_return_address(0));
    scene_draw(o, 84, type, primitives, __builtin_return_address(0));
    scene_geometry(o, 84, 0, min, count, 0, stride);
    probe_draw(o);
    PVOID saved = character_before(o);
    CoverageDraw coverage;
    coverage_begin(o, &coverage);
    INT hr = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, const void *, UINT, const void *,
                          UINT))original_method(o, 84))(o, type, min, count, primitives, indices,
                                                        format, vertices, stride);
    coverage_end(o, &coverage);
    character_after(o, saved);
    CharacterMaskState mask;
    if (hr >= 0 && cm_begin(o, &mask)) {
        INT replay = ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, const void *, UINT, const void *,
                                  UINT))original_method(o, 84))(o, type, min, count, primitives,
                                                                indices, format, vertices, stride);
        cm_end(o, &mask, replay);
    }
    return hr;
}

static INT API device(PVOID o, UINT adapter, UINT type, PVOID window, UINT behavior,
                      PresentParams *p, PVOID *out) {
    /* This game's primary device is the supported path. Release cached AA
    * resources from previous device sessions before creating another device. */
    coverage_reset();
    tb_cleanup();
    circle_cleanup();
    cs_cleanup();
    menu_cleanup();
    scene_finish("CreateDevice");
    for (UINT i = 0; i < state_count; i++) {
        bloom_cleanup(&states[i]);
        probe_cleanup(&states[i]);
        aa_cleanup(&states[i]);
    }
    log_present_params("CreateDevice requested", p);
    DeviceState candidate = {0};
    PresentParams request;
    if (p) {
        select_size(&candidate, p);
        request = *p;
        request.w = candidate.rw;
        request.h = candidate.rh;
    }
    INT hr = ((INT(API *)(PVOID, UINT, UINT, PVOID, UINT, PresentParams *, PVOID *))original_method(
        o, 16))(o, adapter, type, window, behavior, p ? &request : p, out);
    if (hr < 0 && p && candidate.active) {
        log_begin("High-resolution CreateDevice failed; retrying original dimensions");
        log_hex_field("result", hr);
        log_emit();
        candidate.active = 0;
        candidate.rw = candidate.lw;
        candidate.rh = candidate.lh;
        request = *p;
        hr = ((INT(API *)(PVOID, UINT, UINT, PVOID, UINT, PresentParams *, PVOID *))original_method(
            o, 16))(o, adapter, type, window, behavior, &request, out);
    }
    if (hr >= 0 && p) {
        log_present_params("CreateDevice actual", &request);
        *p = request;
        p->w = candidate.lw;
        p->h = candidate.lh;
    }

    log_begin("CreateDevice result");
    log_hex_field("result", hr);
    log_emit();
    if (hr >= 0 && out && *out) {
        /* Menu keys are only read while one of the game's windows has focus. */
        menu_windows[0] = window;
        menu_windows[1] = p ? p->window : 0;
        DeviceState *st = device_state_new(*out);
        if (st) {
            aa_cleanup(st);
            *st = candidate;
            st->obj = *out;
        }
        HookTable *t = hook_table_record(*out, 119);
        if (t) {
            hook_install(t, 41, scene_beginscene);
            hook_install(t, 42, scene_endscene);
            hook_install(t, 43, scene_clear);
            hook_install(t, 16, reset);
            hook_install(t, 17, present);
            hook_install(t, 23, texture);
            hook_install(t, 28, rt);
            hook_install(t, 29, depth);
            hook_install(t, 34, stretch);
            hook_install(t, 37, setrt);
            hook_install(t, 39, setdepth);
            hook_install(t, 47, viewport);
            hook_install(t, 48, getviewport);
            hook_install(t, 75, scissor);
            hook_install(t, 76, getscissor);
            hook_install(t, 81, draw);
            hook_install(t, 82, indexed);
            hook_install(t, 83, drawup);
            hook_install(t, 84, indexedup);
            hook_install(t, 106, outline_pixel_shader);
            hook_install(t, 91, bloom_vertex_shader);
            PVOID back = 0;
            PVOID *v = *(PVOID **)*out;
            INT result =
                ((INT(API *)(PVOID, UINT, UINT, UINT, PVOID *))v[18])(*out, 0, 0, 0, &back);
            if (result >= 0 && back) {
                log_surface("Initial backbuffer", 6, 0, back, result);
                PVOID *b = *(PVOID **)back;
                ((UINT(API *)(PVOID))b[2])(back);
            }
            /* Retry after normal game startup if its loaded code was not ready in init. */
            if (texture_deferred && !shadow_attempted) {
                textures_install();
            }
            shadow_setup(*out);
            log_begin("AO configuration");
            log_field("enabled", ao_enabled);
            log_field("strength", ao_strength);
            log_field("radius", ao_radius);
            log_field("quality", ao_quality);
            log_field("bias", ao_bias);
            log_field("maxDistance", ao_max_distance);
            log_field("depthView", ao_depth_view);
            log_field("excludeCharacters", ao_exclude_characters);
            log_emit();
            log_begin("Bloom configuration");
            log_field("enabled", bloom_enabled);
            log_field("strength", bloom_strength);
            log_field("threshold", bloom_threshold);
            log_field("knee", bloom_knee);
            log_field("radius", bloom_radius);
            log_field("composite", bloom_composite);
            log_field("brightness", bloom_brightness);
            log_emit();
            log_begin("Outline configuration");
            log_field("enabled", outline_enabled);
            log_field("thicknessPercent", outline_thickness);
            log_field("colorOverride", outline_color_override);
            log_field("red", outline_red);
            log_field("green", outline_green);
            log_field("blue", outline_blue);
            log_field("opacity", outline_opacity);
            log_emit();
            log_present_params("CreateDevice effective", p);
        }
    }
    return hr;
}

PVOID API Direct3DCreate9(UINT sdk) {
    initialize();
    if (!backend_module) {
        return 0;
    }
    PVOID fn = GetProcAddress(backend_module, "Direct3DCreate9");
    if (!fn) {
        return 0;
    }
    PVOID o = ((PVOID(API *)(UINT))fn)(sdk);
    log_begin("Direct3DCreate9");
    log_field("sdk", sdk);
    log_field("success", o != 0);
    log_emit();
    if (o) {
        hook_install(hook_table_record(o, 17), 16, device);
    }
    return o;
}

/* Optional performance markers used by middleware are transparently forwarded. */
INT API D3DPERF_BeginEvent(UINT color, const WCHAR *name) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_BeginEvent") : 0;
    return f ? ((INT(API *)(UINT, const WCHAR *))f)(color, name) : -1;
}

INT API D3DPERF_EndEvent(void) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_EndEvent") : 0;
    return f ? ((INT(API *)(void))f)() : -1;
}

void API D3DPERF_SetMarker(UINT color, const WCHAR *name) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_SetMarker") : 0;
    if (f) {
        ((void(API *)(UINT, const WCHAR *))f)(color, name);
    }
}

void API D3DPERF_SetRegion(UINT color, const WCHAR *name) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_SetRegion") : 0;
    if (f) {
        ((void(API *)(UINT, const WCHAR *))f)(color, name);
    }
}

UINT API D3DPERF_GetStatus(void) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_GetStatus") : 0;
    return f ? ((UINT(API *)(void))f)() : 0;
}

INT API D3DPERF_QueryRepeatFrame(void) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_QueryRepeatFrame") : 0;
    return f ? ((INT(API *)(void))f)() : 0;
}

void API D3DPERF_SetOptions(UINT opts) {
    initialize();
    PVOID f = backend_module ? GetProcAddress(backend_module, "D3DPERF_SetOptions") : 0;
    if (f) {
        ((void(API *)(UINT))f)(opts);
    }
}

INT API DllMain(PVOID m, UINT reason, PVOID reserved) {
    (void)reserved;
    if (reason == 1) {
        self_module = m;
    }
    if (reason == 0) {
        if (log_file) {
            CloseHandle(log_file);
        }
        if (scene_log) {
            CloseHandle(scene_log);
        }
    }
    return 1;
}
