/* In-game settings menu.
 *
 * Opened and closed with an F key ([Compatibility] MenuKey, default F9).
 * While open: 1 / 2 select the previous / next setting, - / = lower / raise
 * its value (hold to repeat), and - / = on an action row runs it. The list
 * scrolls when it does not fit. Restart-only settings are shown grayed with
 * "(restart)"; their edits are saved to the INI and apply at the next start.
 * A controller works too (XInput): D-pad up/down select, left/right change,
 * and LB + RB + Back + Start together open or close the menu.
 * Input is read with GetAsyncKeyState / XInputGetState only while the game
 * window is in the foreground; nothing is blocked, so the game still sees
 * these keys and buttons.
 *
 * Drawing happens at Present, after anti-aliasing, on the back buffer, using
 * the fixed-function pipeline inside its own BeginScene/EndScene. The game's
 * state is captured in a state block and restored, as in aa_apply. Any
 * failure disables the menu until the next device reset.
 */
#include "menu_font.h"

static short(API *menu_key)(INT);
static UINT(API *menu_xinput)(UINT, void *);
static UINT menu_pad_wait[4]; /* frames before retrying a disconnected pad */
/* Other controllers (for example PlayStation pads the game reads directly)
 * through the Windows joystick API in winmm.dll. */
static UINT(API *menu_joy_state)(UINT, void *);
static UINT(API *menu_joy_caps)(UINT, void *, UINT);
static UINT menu_joy_wait[16], menu_joy_layout[16]; /* layout: 0 unknown, 1 Xbox, 2 Sony */
static PVOID(API *menu_foreground)(void);
static PVOID menu_windows[2];
static UINT menu_ready, menu_open, menu_selected, menu_failed, menu_hold[5];
static const char *menu_message;
static UINT menu_message_frames;
static PVOID menu_font_texture;

enum { MENU_HEADER, MENU_NOTE, MENU_NUMBER, MENU_CHOICE, MENU_LIST, MENU_ACTION, MENU_PRESET };

enum { MENU_SAVE = 1, MENU_DISCARD = 2, MENU_APPLY_PRESET = 3 };

/* A menu row. Live rows point at the running setting (value). Rows without a
 * value are restart-only: they edit a pending copy of the INI value, which is
 * saved to the INI and takes effect at the next start. A row with
 * "available" is live only while *available is non-zero (Bloom/AO were on at
 * startup, movie volume was installed); otherwise it behaves as restart-only. */
typedef struct {
    UINT kind;
    const char *label, *section, *key;
    UINT *value;
    UINT min, max, step, def; /* def = INI default, used for pending values */
    const char *const *names; /* MENU_CHOICE: one name per value from 0 */
    const UINT *list; /* MENU_LIST: allowed values, ascending */
    UINT list_count, action;
    UINT *available;
} MenuItem;

#define M_HEAD(text)                                                                               \
    { .kind = MENU_HEADER, .label = text }
#define M_NUM(l, s, k, v, lo, hi, st, d)                                                           \
    {                                                                                              \
        .kind = MENU_NUMBER, .label = l, .section = s, .key = k, .value = v, .min = lo, .max = hi, \
        .step = st, .def = d                                                                       \
    }
#define M_CHOICE(l, s, k, v, lo, hi, n, d)                                                         \
    {                                                                                              \
        .kind = MENU_CHOICE, .label = l, .section = s, .key = k, .value = v, .min = lo, .max = hi, \
        .step = 1, .def = d, .names = n                                                            \
    }
#define M_NOTE(text)                                                                               \
    { .kind = MENU_NOTE, .label = text }
#define M_ACTION(l, a)                                                                             \
    { .kind = MENU_ACTION, .label = l, .action = a }

static const char *const menu_onoff[] = {"Off", "On"};
static const char *const menu_composite[] = {"Add", "Screen", "Tone-mapped"};
static const char *const menu_brightness[] = {"Brightest channel", "Luminance"};
static const char *const menu_quality[] = {"", "Low (8)", "Medium (16)", "High (32)"};
static const char *const menu_aa[] = {"Off", "FXAA-style", "SMAA 1x (experimental)"};
static const char *const menu_smaa[] = {"Low", "Medium", "High", "Ultra"};
static const char *const menu_actors[] = {"Player only", "+ CPU characters", "+ Crowd enemies"};
static const char *const menu_coverage[] = {"1x (original)", "2x", "3x"};
static const char *const menu_filter[] = {"Off", "3x3", "5x5"};
static const UINT menu_shadow_sizes[] = {1024, 2048, 4096, 8192, 12288, 16384};

static const MenuItem menu_items[] = {
    M_HEAD("Presets"),
    M_NOTE("Presets change most graphics settings and the"),
    M_NOTE("internal resolution. Restart to apply them."),
    {.kind = MENU_PRESET, .label = "Preset"},
    M_ACTION("Apply selected preset", MENU_APPLY_PRESET),
    M_HEAD("Bloom"),
    {.kind = MENU_CHOICE,
     .label = "Enabled",
     .section = "Bloom",
     .key = "Enabled",
     .value = &bloom_enabled,
     .max = 1,
     .step = 1,
     .names = menu_onoff,
     .available = &bloom_available},
    M_NUM("Strength", "Bloom", "Strength", &bloom_strength, 0, 200, 5, 35),
    M_NUM("Threshold", "Bloom", "Threshold", &bloom_threshold, 0, 1000, 25, 750),
    M_NUM("Threshold softness", "Bloom", "Knee", &bloom_knee, 0, 500, 10, 100),
    M_NUM("Radius", "Bloom", "Radius", &bloom_radius, 25, 400, 25, 100),
    M_CHOICE("Blend mode", "Bloom", "Composite", &bloom_composite, 0, 2, menu_composite, 0),
    M_CHOICE("Brightness measure", "Bloom", "Brightness", &bloom_brightness, 0, 1, menu_brightness,
             0),
    M_HEAD("Ambient occlusion"),
    {.kind = MENU_CHOICE,
     .label = "Enabled (Very demanding)",
     .section = "AmbientOcclusion",
     .key = "Enabled",
     .value = &ao_enabled,
     .max = 1,
     .step = 1,
     .names = menu_onoff,
     .available = &ao_available},
    M_NUM("Strength", "AmbientOcclusion", "Strength", &ao_strength, 0, 300, 10, 100),
    M_NUM("Radius", "AmbientOcclusion", "Radius", &ao_radius, 1, 200, 1, 30),
    M_CHOICE("Quality", "AmbientOcclusion", "Quality", &ao_quality, 1, 3, menu_quality, 2),
    M_NUM("Bias", "AmbientOcclusion", "Bias", &ao_bias, 0, 50, 1, 10),
    M_NUM("Max distance", "AmbientOcclusion", "MaxDistance", &ao_max_distance, 0, 100000, 250, 0),
    M_CHOICE("Exclude characters", "AmbientOcclusion", "ExcludeCharacters", &ao_exclude_characters,
             0, 1, menu_onoff, 0),
    M_CHOICE("Exclude transform. BGs", "AmbientOcclusion", "ExcludeTransformationBackgrounds",
             &ao_exclude_backgrounds, 0, 1, menu_onoff, 0),
    M_HEAD("Anti-aliasing"),
    M_CHOICE("Method", "Antialiasing", "Method", &aa_method, 0, 2, menu_aa, 0),
    M_NUM("Strength", "Antialiasing", "Strength", &aa_strength, 0, 100, 5, 100),
    M_NUM("FXAA span", "Antialiasing", "Span", &aa_span, 1, 16, 1, 8),
    M_NUM("FXAA edge threshold", "Antialiasing", "EdgeThreshold", &aa_relative, 0, 1000, 5, 125),
    M_NUM("FXAA min. threshold", "Antialiasing", "MinimumThreshold", &aa_minimum, 0, 1000, 1, 31),
    M_CHOICE("SMAA quality", "Antialiasing", "SMAAQuality", 0, 0, 3, menu_smaa, 2),
    M_HEAD("Shadows"),
    M_CHOICE("Character shadows", "Shadows", "ActorShadows", &actor_shadows, 0, 2, menu_actors, 0),
    M_CHOICE("Shadow area", "Shadows", "Coverage", &shadow_coverage, 0, 2, menu_coverage, 0),
    M_CHOICE("Custom shadow size", "Shadows", "Enabled", 0, 0, 1, menu_onoff, 0),
    {.kind = MENU_LIST,
     .label = "Shadow map size",
     .section = "Shadows",
     .key = "Resolution",
     .def = 4096,
     .list = menu_shadow_sizes,
     .list_count = 6},
    M_CHOICE("Shadow softening", "Shadows", "Filter", 0, 0, 2, menu_filter, 0),
    M_NUM("Softening radius", "Shadows", "FilterRadius", 0, 25, 800, 25, 200),
    M_HEAD("Outlines"),
    M_CHOICE("Show outlines", "Outlines", "Enabled", 0, 0, 1, menu_onoff, 1),
    M_NUM("Thickness", "Outlines", "Thickness", 0, 25, 400, 5, 100),
    M_CHOICE("Custom color", "Outlines", "ColorOverride", 0, 0, 1, menu_onoff, 0),
    M_NUM("Red", "Outlines", "Red", 0, 0, 255, 5, 0),
    M_NUM("Green", "Outlines", "Green", 0, 0, 255, 5, 0),
    M_NUM("Blue", "Outlines", "Blue", 0, 0, 255, 5, 0),
    M_HEAD("Movies"),
    M_CHOICE("Volume control", "Movies", "Enabled", 0, 0, 1, menu_onoff, 0),
    {.kind = MENU_NUMBER,
     .label = "Volume",
     .section = "Movies",
     .key = "Volume",
     .value = &movie_percent,
     .max = 100,
     .step = 5,
     .def = 100,
     .available = &movie_installed},
    M_NOTE("Volume applies from the next video played."),
    M_HEAD(""),
    M_ACTION("Save to SKGraphics.ini", MENU_SAVE),
    M_ACTION("Discard changes", MENU_DISCARD),
};
#define MENU_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

/* Values of rows that are not live, as they will be written to the INI. */
static UINT menu_pending[MENU_COUNT];
static void menu_presets_load(void);
static UINT menu_scroll;

static void menu_say(const char *message) {
    menu_message = message;
    menu_message_frames = 180;
}

static UINT menu_live(const MenuItem *m) {
    return m->value && !(m->available && !*m->available);
}

static UINT menu_get(UINT i) {
    const MenuItem *m = &menu_items[i];
    return menu_live(m) ? *m->value : menu_pending[i];
}

static UINT menu_clamp(const MenuItem *m, UINT v) {
    if (m->kind == MENU_LIST) {
        for (UINT j = 0; j < m->list_count; j++) {
            if (m->list[j] == v) {
                return v;
            }
        }
        return m->def;
    }
    return v < m->min ? m->min : v > m->max ? m->max : v;
}

/* Reads restart-only values from the INI: at first use, after a live reload
 * and when changes are discarded. */
static void menu_pending_load(void) {
    for (UINT i = 0; i < MENU_COUNT; i++) {
        const MenuItem *m = &menu_items[i];
        if (m->section) {
            menu_pending[i] = menu_clamp(m, config_int(m->section, m->key, m->def));
        }
    }
}

/* ---- Saving ------------------------------------------------------------ */

static void menu_wide(WCHAR *out, const char *in, UINT cap) {
    UINT i = 0;
    for (; in[i] && i + 1 < cap; i++) {
        out[i] = (WCHAR)in[i];
    }
    out[i] = 0;
}

static UINT menu_save(void) {
    UINT failures = 0;
    for (UINT i = 0; i < MENU_COUNT; i++) {
        const MenuItem *m = &menu_items[i];
        if (!m->section) {
            continue;
        }
        WCHAR section[64], key[64], value[12];
        menu_wide(section, m->section, 64);
        menu_wide(key, m->key, 64);
        char digits[12];
        UINT v = menu_get(i), n = 0;
        do {
            digits[n++] = (char)('0' + v % 10);
            v /= 10;
        } while (v && n < 11);
        for (UINT j = 0; j < n; j++) {
            value[j] = (WCHAR)digits[n - 1 - j];
        }
        value[n] = 0;
        if (!WritePrivateProfileStringW(section, key, value, ini_path)) {
            failures++;
        }
    }
    log_begin("Menu settings saved");
    log_field("failures", failures);
    log_emit();
    return failures == 0;
}

/* ---- Input ------------------------------------------------------------- */

static void menu_init(void) {
    if (menu_ready) {
        return;
    }
    menu_ready = 1;
    menu_pending_load();
    menu_presets_load();
    static const WCHAR dll[] = {'u', 's', 'e', 'r', '3', '2', '.', 'd', 'l', 'l', 0};
    PVOID lib = LoadLibraryW(dll);
    if (lib) {
        menu_key = (short(API *)(INT))GetProcAddress(lib, "GetAsyncKeyState");
        menu_foreground = (PVOID(API *)(void))GetProcAddress(lib, "GetForegroundWindow");
    }
    if (menu_controller) {
        /* xinput1_4 (Windows 8+), then 1_3 (DirectX runtime), then 9_1_0
         * (Windows Vista+). Whichever loads first is used. */
        static const WCHAR names[3][16] = {
            {'x', 'i', 'n', 'p', 'u', 't', '1', '_', '4', '.', 'd', 'l', 'l', 0},
            {'x', 'i', 'n', 'p', 'u', 't', '1', '_', '3', '.', 'd', 'l', 'l', 0},
            {'x', 'i', 'n', 'p', 'u', 't', '9', '_', '1', '_', '0', '.', 'd', 'l', 'l', 0}};
        for (UINT i = 0; i < 3 && !menu_xinput; i++) {
            PVOID x = LoadLibraryW(names[i]);
            if (x) {
                menu_xinput = (UINT(API *)(UINT, void *))GetProcAddress(x, "XInputGetState");
            }
        }
    }
    if (menu_controller) {
        static const WCHAR winmm[] = {'w', 'i', 'n', 'm', 'm', '.', 'd', 'l', 'l', 0};
        PVOID w = LoadLibraryW(winmm);
        if (w) {
            menu_joy_state = (UINT(API *)(UINT, void *))GetProcAddress(w, "joyGetPosEx");
            menu_joy_caps = (UINT(API *)(UINT, void *, UINT))GetProcAddress(w, "joyGetDevCapsW");
        }
    }
    log_begin(menu_key ? "In-game menu ready" : "In-game menu unavailable: no keyboard access");
    log_field("functionKey", menu_vk - 0x6f);
    log_field("controller", menu_xinput != 0);
    log_field("joystick", menu_joy_state && menu_joy_caps);
    log_emit();
}

/* Buttons held on any connected controller (XInput wButtons bits). A pad
 * that reports "not connected" is skipped for about 2 s, because polling
 * absent pads every frame is slow on some systems. */
#define PAD_UP 0x0001
#define PAD_DOWN 0x0002
#define PAD_LEFT 0x0004
#define PAD_RIGHT 0x0008
#define PAD_OPEN 0x0330 /* Start + Back + LB + RB */

/* Joystick API pads, translated to the XInput bits above. The D-pad is the
 * point-of-view hat (hundredths of a degree clockwise from up; 65535 when
 * centered). Shoulder buttons are 5 and 6 on both layouts; Back/Start are 7
 * and 8 on Xbox-style pads, Share/Options 9 and 10 on Sony pads (vendor ID
 * 0x054c). */
static UINT menu_joy_buttons(void) {
    if (!menu_joy_state || !menu_joy_caps) {
        return 0;
    }
    UINT buttons = 0;
    for (UINT i = 0; i < 16; i++) {
        if (menu_joy_wait[i]) {
            menu_joy_wait[i]--;
            continue;
        }
        UINT info[13] = {52, 0xc0}; /* size; flags: buttons + hat */
        if (menu_joy_state(i, info)) {
            menu_joy_wait[i] = 120;
            menu_joy_layout[i] = 0;
            continue;
        }
        if (!menu_joy_layout[i]) {
            unsigned short caps[364] = {0}; /* JOYCAPSW, 728 bytes */
            menu_joy_layout[i] = !menu_joy_caps(i, caps, sizeof(caps)) && caps[0] == 0x054c ? 2 : 1;
        }
        UINT b = info[8], hat = info[10];
        if (hat < 36000) {
            if (hat >= 31500 || hat <= 4500) {
                buttons |= PAD_UP;
            }
            if (hat >= 4500 && hat <= 13500) {
                buttons |= PAD_RIGHT;
            }
            if (hat >= 13500 && hat <= 22500) {
                buttons |= PAD_DOWN;
            }
            if (hat >= 22500 && hat <= 31500) {
                buttons |= PAD_LEFT;
            }
        }
        UINT shoulders = (b >> 4) & 3, menu_pair = menu_joy_layout[i] == 2 ? (b >> 8) & 3 : (b >> 6) & 3;
        if (shoulders == 3 && menu_pair == 3) {
            buttons |= PAD_OPEN;
        }
    }
    return buttons;
}

static UINT menu_pad_buttons(void) {
    if (!menu_xinput) {
        return menu_joy_buttons();
    }
    UINT buttons = 0;
    for (UINT i = 0; i < 4; i++) {
        if (menu_pad_wait[i]) {
            menu_pad_wait[i]--;
            continue;
        }
        UINT state[4] = {0}; /* packet number, buttons, triggers, sticks */
        if (menu_xinput(i, state)) {
            menu_pad_wait[i] = 120;
            continue;
        }
        buttons |= state[1] & 0xffff;
    }
    return buttons | menu_joy_buttons();
}

static UINT menu_key_down(UINT vk) {
    return (((UINT)(unsigned short)menu_key((INT)vk)) & 0x8000) != 0;
}

/* Edge detection with hold-to-repeat after about 1/3 s (counted in frames). */
static UINT menu_pressed(UINT slot, UINT down, UINT repeat) {
    if (!down) {
        menu_hold[slot] = 0;
        return 0;
    }
    UINT held = ++menu_hold[slot];
    return held == 1 || (repeat && held >= 20 && (held - 20) % 3 == 0);
}

static UINT menu_next_value(const MenuItem *m, UINT value, INT direction) {
    if (m->kind == MENU_LIST) {
        UINT j = 0;
        while (j + 1 < m->list_count && m->list[j] < value) {
            j++;
        }
        if (direction < 0) {
            return j ? m->list[j - 1] : m->list[0];
        }
        return j + 1 < m->list_count ? m->list[j + 1] : m->list[j];
    }
    if (direction < 0) {
        return value > m->min + m->step ? value - m->step : m->min;
    }
    return value + m->step < m->max ? value + m->step : m->max;
}

/* ---- Presets ------------------------------------------------------------
 * SKGraphics_Presets.ini beside the DLL holds [Preset1]..[Preset8]. Each has
 * Name=... and lines of Section.Key=Value. Applying a preset writes those
 * values into SKGraphics.ini; only graphics sections may be changed. */
#define MENU_PRESETS 8
#define MENU_PRESET_NAME 43
static char menu_preset_names[MENU_PRESETS][MENU_PRESET_NAME + 1];
static UINT menu_preset_count, menu_preset_choice;
static WCHAR menu_preset_path[32768], menu_preset_lines[8192];
static const char *const menu_preset_sections[] = {
    "Rendering", "Antialiasing", "Shadows", "Bloom", "AmbientOcclusion", "Outlines", "Movies"};

static void menu_preset_section(WCHAR *out, UINT index) {
    const char *base = "Preset";
    UINT i = 0;
    for (; base[i]; i++) {
        out[i] = (WCHAR)base[i];
    }
    out[i++] = (WCHAR)('1' + index);
    out[i] = 0;
}

static void menu_presets_load(void) {
    menu_preset_count = 0;
    /* Same folder and stem as the INI: SKGraphics.ini -> SKGraphics_Presets.ini */
    UINT n = 0;
    while (n < 32700 && ini_path[n]) {
        n++;
    }
    if (n < 4 || n >= 32700) {
        return;
    }
    n -= 4; /* drop ".ini" */
    for (UINT i = 0; i < n; i++) {
        menu_preset_path[i] = ini_path[i];
    }
    const char *suffix = "_Presets.ini";
    for (UINT i = 0; suffix[i]; i++) {
        menu_preset_path[n++] = (WCHAR)suffix[i];
    }
    menu_preset_path[n] = 0;
    static const WCHAR name_key[] = {'N', 'a', 'm', 'e', 0}, empty[] = {0};
    for (UINT p = 0; p < MENU_PRESETS; p++) {
        WCHAR section[8], name[MENU_PRESET_NAME + 1];
        menu_preset_section(section, p);
        if (!GetPrivateProfileStringW(section, name_key, empty, name, MENU_PRESET_NAME + 1,
                                      menu_preset_path)) {
            break;
        }
        UINT j = 0;
        for (; name[j] && j < MENU_PRESET_NAME; j++) {
            menu_preset_names[p][j] = name[j] >= ' ' && name[j] < 127 ? (char)name[j] : '?';
        }
        menu_preset_names[p][j] = 0;
        menu_preset_count++;
    }
    if (menu_preset_choice >= menu_preset_count) {
        menu_preset_choice = 0;
    }
}

static UINT menu_preset_allowed(const WCHAR *section, UINT length) {
    for (UINT i = 0; i < sizeof(menu_preset_sections) / sizeof(menu_preset_sections[0]); i++) {
        const char *s = menu_preset_sections[i];
        UINT j = 0;
        while (j < length && s[j] && section[j] == (WCHAR)s[j]) {
            j++;
        }
        if (j == length && !s[j]) {
            return 1;
        }
    }
    return 0;
}

static UINT menu_space(WCHAR c) {
    return c == ' ' || c == '\t';
}

/* Writes the chosen preset into SKGraphics.ini. Returns values written. */
static UINT menu_preset_apply(UINT *rejected) {
    WCHAR section[8];
    menu_preset_section(section, menu_preset_choice);
    UINT total = GetPrivateProfileSectionW(section, menu_preset_lines, 8192, menu_preset_path);
    UINT written = 0;
    *rejected = 0;
    if (total >= 8190) {
        total = 0; /* truncated section: refuse rather than apply part of it */
        (*rejected)++;
    }
    for (UINT at = 0; at < total;) {
        WCHAR *line = &menu_preset_lines[at];
        UINT len = 0;
        while (at + len < total && line[len]) {
            len++;
        }
        at += len + 1;
        UINT a = 0;
        while (a < len && menu_space(line[a])) {
            a++;
        }
        if (a == len || line[a] == ';' || line[a] == '#') {
            continue;
        }
        UINT dot = a, eq = a;
        while (dot < len && line[dot] != '.' && line[dot] != '=') {
            dot++;
        }
        while (eq < len && line[eq] != '=') {
            eq++;
        }
        if (eq == len) {
            continue;
        }
        if (dot >= eq) {
            continue; /* Name=... or another key without a section */
        }
        UINT key_end = eq;
        while (key_end > dot + 1 && menu_space(line[key_end - 1])) {
            key_end--;
        }
        UINT v = eq + 1, v_end = len;
        while (v < len && menu_space(line[v])) {
            v++;
        }
        while (v_end > v && menu_space(line[v_end - 1])) {
            v_end--;
        }
        if (!menu_preset_allowed(line + a, dot - a) || key_end == dot + 1 || v == v_end) {
            (*rejected)++;
            continue;
        }
        line[dot] = 0;
        line[key_end] = 0;
        line[v_end] = 0;
        if (WritePrivateProfileStringW(line + a, line + dot + 1, line + v, ini_path)) {
            written++;
        } else {
            (*rejected)++;
        }
    }
    log_begin("Preset applied");
    log_text(" name=");
    log_text(menu_preset_names[menu_preset_choice]);
    log_field("written", written);
    log_field("rejected", *rejected);
    log_emit();
    return written;
}

static void menu_reload_all(void) {
    UINT old_mode = cpu_shadow_test, old_crowd = crowd_shadow_test, old_coverage = shadow_coverage;
    config_read_live();
    live_shadows_changed(old_mode, old_crowd, old_coverage);
    menu_pending_load();
    menu_dirty = 0;
}

static void menu_step(INT direction, UINT first_press) {
    const MenuItem *m = &menu_items[menu_selected];
    if (m->kind == MENU_PRESET) {
        if (!first_press || !menu_preset_count) {
            if (first_press) {
                menu_say("No SKGraphics_Presets.ini found");
            }
            return;
        }
        if (direction < 0 && menu_preset_choice) {
            menu_preset_choice--;
        } else if (direction > 0 && menu_preset_choice + 1 < menu_preset_count) {
            menu_preset_choice++;
        }
        return;
    }
    if (m->kind == MENU_ACTION) {
        if (!first_press) {
            return;
        }
        if (m->action == MENU_APPLY_PRESET) {
            UINT rejected = 0;
            if (!menu_preset_count) {
                menu_say("No SKGraphics_Presets.ini found");
            } else if (menu_preset_apply(&rejected)) {
                /* Live settings change now; the rest after a restart. */
                menu_reload_all();
                menu_say(rejected ? "Preset saved with errors - see log"
                                  : "Preset saved - restart the game to apply");
            } else {
                menu_say("Preset not applied - see log");
            }
            return;
        }
        if (m->action == MENU_SAVE) {
            if (menu_save()) {
                menu_dirty = 0;
                menu_say("Saved to SKGraphics.ini");
            } else {
                menu_say("Save failed - see log");
            }
        } else {
            menu_reload_all();
            menu_say("Changes discarded");
        }
        return;
    }
    /* Choices and size lists change one step per press; numbers repeat while held. */
    if (m->kind != MENU_NUMBER && !first_press) {
        return;
    }
    UINT old = menu_get(menu_selected), value = menu_next_value(m, old, direction);
    if (value == old) {
        return;
    }
    menu_dirty = 1;
    if (!menu_live(m)) {
        menu_pending[menu_selected] = value;
        menu_say("Applies after saving and restarting");
        return;
    }
    UINT old_mode = cpu_shadow_test, old_crowd = crowd_shadow_test, old_coverage = shadow_coverage;
    *m->value = value;
    if (m->value == &actor_shadows) {
        actor_shadows_derive();
    }
    live_shadows_changed(old_mode, old_crowd, old_coverage);
}

static void menu_move(INT direction) {
    UINT i = menu_selected;
    for (UINT tries = 0; tries < MENU_COUNT; tries++) {
        i = direction < 0 ? (i ? i - 1 : MENU_COUNT - 1) : (i + 1 < MENU_COUNT ? i + 1 : 0);
        if (menu_items[i].kind > MENU_NOTE) {
            menu_selected = i;
            return;
        }
    }
}

static void menu_input(void) {
    if (!menu_enabled || menu_failed) {
        return;
    }
    menu_init();
    if (!menu_key) {
        return;
    }
    if (menu_message_frames) {
        menu_message_frames--;
    }
    if (menu_foreground) {
        PVOID fg = menu_foreground();
        if (fg != menu_windows[0] && fg != menu_windows[1]) {
            for (UINT i = 0; i < 5; i++) {
                menu_hold[i] = 0;
            }
            return;
        }
    }
    UINT pad = menu_pad_buttons();
    if (menu_pressed(0, menu_key_down(menu_vk) || (pad & PAD_OPEN) == PAD_OPEN, 0)) {
        menu_open = !menu_open;
        if (menu_open && menu_items[menu_selected].kind <= MENU_NOTE) {
            menu_move(1);
        }
    }
    if (!menu_open) {
        return;
    }
    if (menu_pressed(1, menu_key_down(0x31) || (pad & PAD_UP), 1)) { /* 1 */
        menu_move(-1);
    }
    if (menu_pressed(2, menu_key_down(0x32) || (pad & PAD_DOWN), 1)) { /* 2 */
        menu_move(1);
    }
    if (menu_pressed(3, menu_key_down(0xbd) || (pad & PAD_LEFT), 1)) { /* - */
        menu_step(-1, menu_hold[3] == 1);
    }
    if (menu_pressed(4, menu_key_down(0xbb) || (pad & PAD_RIGHT), 1)) { /* = */
        menu_step(1, menu_hold[4] == 1);
    }
}

/* ---- Drawing ----------------------------------------------------------- */

typedef struct {
    float x, y, z, rhw;
    UINT color;
    float u, v;
} MenuVertex;

#define MENU_MAX_QUADS 2048
static MenuVertex menu_vertices[MENU_MAX_QUADS * 6];
static UINT menu_quads;

static void menu_quad(float x, float y, float w, float h, UINT color, float u0, float v0, float u1,
                      float v1) {
    if (menu_quads == MENU_MAX_QUADS) {
        return;
    }
    /* Pre-transformed vertices: -0.5 aligns texels with pixels in D3D9. */
    float x0 = x - 0.5f, y0 = y - 0.5f, x1 = x + w - 0.5f, y1 = y + h - 0.5f;
    MenuVertex *v = &menu_vertices[menu_quads++ * 6];
    MenuVertex a = {x0, y0, 0, 1, color, u0, v0}, b = {x1, y0, 0, 1, color, u1, v0},
               c = {x0, y1, 0, 1, color, u0, v1}, d = {x1, y1, 0, 1, color, u1, v1};
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = c;
    v[4] = b;
    v[5] = d;
}

static void menu_box(float x, float y, float w, float h, UINT color) {
    /* Sample the middle of the solid cell (127) so edges never bleed. */
    float u = (15 * 8 + 4) / 128.0f, v = (7 * 8 + 4) / 64.0f;
    menu_quad(x, y, w, h, color, u, v, u, v);
}

/* Draws text and returns the x position after it. */
static float menu_text(float x, float y, UINT scale, const char *s, UINT color) {
    for (; *s; s++, x += 8.0f * (float)scale) {
        UINT c = (unsigned char)*s;
        if (c <= ' ' || c > 126) {
            continue;
        }
        float u = (float)((c % 16) * 8) / 128.0f, v = (float)((c / 16) * 8) / 64.0f;
        menu_quad(x, y, 8.0f * (float)scale, 8.0f * (float)scale, color, u, v, u + 8 / 128.0f,
                  v + 8 / 64.0f);
    }
    return x;
}

static void menu_number(char *out, UINT value) {
    char digits[12];
    UINT n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && n < 11);
    for (UINT i = 0; i < n; i++) {
        out[i] = digits[n - 1 - i];
    }
    out[n] = 0;
}

#define MENU_COLUMNS 52
#define MENU_VALUE_COLUMN 28
#define MENU_FIXED_LINES 6 /* title, hint, INI note (1.5), gap, status, margins */

/* Rows that fit on screen; the list scrolls when it does not all fit. */
static UINT menu_visible_rows(UINT height, UINT s) {
    INT rows = (INT)((height - 32 * s) / (10 * s)) - MENU_FIXED_LINES;
    if (rows < 5) {
        rows = 5;
    }
    return (UINT)rows < MENU_COUNT ? (UINT)rows : MENU_COUNT;
}

/* Keeps the selection on screen, also showing its section header when possible. */
static void menu_follow(UINT visible) {
    UINT top = menu_selected;
    while (top && menu_items[top - 1].kind <= MENU_NOTE) {
        top--; /* also show the header and notes above */
    }
    if (top < menu_scroll) {
        menu_scroll = top;
    }
    if (menu_selected >= menu_scroll + visible) {
        menu_scroll = menu_selected + 1 - visible;
    }
    if (menu_scroll + visible > MENU_COUNT) {
        menu_scroll = MENU_COUNT - visible;
    }
}

/* Builds every quad for the current menu state. Returns the quad count. */
static UINT menu_layout(UINT height) {
    menu_quads = 0;
    UINT s = (height + 270) / 540;
    if (s < 1) {
        s = 1;
    }
    UINT visible = menu_visible_rows(height, s);
    menu_follow(visible);
    float cw = 8.0f * (float)s, lh = 10.0f * (float)s, x = 16.0f * (float)s, y = 16.0f * (float)s;
    menu_box(x, y, cw * (MENU_COLUMNS + 2), lh * ((float)visible + MENU_FIXED_LINES), 0xd8101418);
    x += cw;
    y += lh * 0.5f;
    menu_text(x, y, s, "SKGraphics 2.0 settings", 0xffffd27f);
    y += lh;
    char hint[48] = "1/2 select  -/= change  F__ close";
    UINT fkey = menu_vk - 0x6f;
    hint[25] = fkey >= 10 ? (char)('0' + fkey / 10) : (char)('0' + fkey);
    hint[26] = fkey >= 10 ? (char)('0' + fkey % 10) : ' ';
    menu_text(x, y, s, hint, 0xffa0a0a0);
    y += lh;
    menu_text(x, y, s, "More settings are in SKGraphics.ini", 0xffa0a0a0);
    y += lh * 1.5f;
    for (UINT row = 0; row < visible; row++, y += lh) {
        UINT i = menu_scroll + row;
        const MenuItem *m = &menu_items[i];
        /* More rows above / below: a small marker at the right edge. */
        if ((row == 0 && menu_scroll) || (row + 1 == visible && i + 1 < MENU_COUNT)) {
            menu_text(x + cw * (MENU_COLUMNS - 1), y, s, row == 0 && menu_scroll ? "^" : "v",
                      0xffa0a0a0);
        }
        if (m->kind == MENU_HEADER) {
            menu_text(x, y, s, m->label, 0xff7fc8ff);
            continue;
        }
        if (m->kind == MENU_NOTE) {
            menu_text(x, y, s, m->label, 0xffa0a0a0);
            continue;
        }
        UINT selected = i == menu_selected, restart = m->section && !menu_live(m);
        if (selected) {
            menu_box(x - cw * 0.5f, y - (float)s, cw * (MENU_COLUMNS - 1.5f), lh, 0xff2e4a6b);
        }
        UINT color = restart    ? (selected ? 0xffb8b8b8 : 0xff8a8a8a)
                     : selected ? 0xffffffff
                                : 0xffe0e0e0;
        if (m->kind == MENU_PRESET) {
            /* The name is long, so it uses the whole row: < name > */
            if (!menu_preset_count) {
                menu_text(x + cw, y, s, "No SKGraphics_Presets.ini found", 0xff8a8a8a);
                continue;
            }
            float px = menu_text(x + cw, y, s, menu_preset_choice ? "<" : " ", color);
            px = menu_text(px + cw, y, s, menu_preset_names[menu_preset_choice], color);
            menu_text(px + cw, y, s, menu_preset_choice + 1 < menu_preset_count ? ">" : " ", color);
            continue;
        }
        menu_text(x + cw, y, s, m->label, color);
        if (m->kind == MENU_ACTION) {
            continue;
        }
        UINT v = menu_get(i);
        char number[24];
        const char *text = number;
        if (m->kind == MENU_CHOICE) {
            text = m->names[v];
        } else {
            menu_number(number, v);
        }
        float end = menu_text(x + cw * MENU_VALUE_COLUMN, y, s, text, color);
        if (restart) {
            menu_text(end + cw, y, s, "(restart)", color);
        }
    }
    y += lh * 0.5f;
    if (menu_message_frames && menu_message) {
        menu_text(x, y, s, menu_message, 0xff8ce08c);
    } else if (menu_dirty) {
        menu_text(x, y, s, "Unsaved changes", 0xffffb070);
    }
    return menu_quads;
}

static void menu_cleanup(void) {
    com_release(menu_font_texture);
    menu_font_texture = 0;
    menu_failed = 0;
}

static INT menu_font_create(PVOID o) {
    if (menu_font_texture) {
        return 1;
    }
    /* 128x64 A8R8G8B8 atlas, 16x8 cells. MANAGED pool survives device Reset. */
    INT hr =
        ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *, PVOID))original_method(
            o, 23))(o, 128, 64, 1, 0, 21, 1, &menu_font_texture, 0);
    if (hr < 0 || !menu_font_texture) {
        menu_font_texture = 0;
        return 0;
    }
    PVOID *v = *(PVOID **)menu_font_texture;

    struct {
        INT pitch;
        unsigned char *bits;
    } locked = {0, 0};

    if (((INT(API *)(PVOID, UINT, PVOID, const void *, UINT))v[19])(menu_font_texture, 0, &locked,
                                                                    0, 0) < 0) {
        menu_cleanup();
        return 0;
    }
    if (!locked.bits || locked.pitch < 128 * 4) {
        ((INT(API *)(PVOID, UINT))v[20])(menu_font_texture, 0);
        menu_cleanup();
        return 0;
    }
    for (UINT y = 0; y < 64; y++) {
        UINT *row = (UINT *)(locked.bits + y * (UINT)locked.pitch);
        for (UINT x = 0; x < 128; x++) {
            UINT c = (y / 8) * 16 + x / 8;
            row[x] = (menu_font[c][y % 8] >> (x % 8)) & 1 ? 0xffffffff : 0x00ffffff;
        }
    }
    return ((INT(API *)(PVOID, UINT))v[20])(menu_font_texture, 0) >= 0;
}

static void menu_draw(DeviceState *t) {
    if (!menu_open || menu_failed || !t || t->aa_busy) {
        return;
    }
    PVOID o = t->obj;
    INT hr = 0, restore = 0, begun = 0, modified = 0;
    const char *step = "cooperative level";
    PVOID back = 0, saved[4] = {0}, depthbuf = 0, block = 0;
    Viewport oldvp = {0};
    SurfaceDesc desc = {0};
    hr = ((INT(API *)(PVOID))original_method(o, 3))(o);
    if (hr < 0) {
        return; /* device lost: try again later, not a failure */
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
    if (hr < 0 || !desc.w || !desc.h) {
        goto failed;
    }
    step = "font";
    if (!menu_font_create(o)) {
        hr = (INT)0x80004005;
        goto failed;
    }
    UINT quads = menu_layout(desc.h);
    step = "state capture";
    hr = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 59))(o, 1, &block);
    if (hr < 0 || !block) {
        goto failed;
    }
    for (UINT i = 0; i < 4; i++) {
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, i, &saved[i]);
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
#define MENU_DO(expr)                                                                              \
    do {                                                                                           \
        hr = (expr);                                                                               \
        if (hr < 0)                                                                                \
            goto failed;                                                                           \
    } while (0)
    step = "setup";
    MENU_DO(((INT(API *)(PVOID, PVOID))original_method(o, 39))(o, 0));
    for (UINT i = 1; i < 4; i++) {
        if (saved[i]) {
            MENU_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, i, 0));
        }
    }
    MENU_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, back));
    {
        Viewport vp = {0, 0, desc.w, desc.h, 0.0f, 1.0f};
        MENU_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &vp));
    }
    MENU_DO(((INT(API *)(PVOID, PVOID))original_method(o, 92))(o, 0)); /* no vertex shader */
    MENU_DO(((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, 0)); /* no pixel shader */
    MENU_DO(((INT(API *)(PVOID, UINT))original_method(o, 89))(o, 0x144)); /* XYZRHW|DIFFUSE|TEX1 */
    MENU_DO(((INT(API *)(PVOID, UINT, UINT))original_method(o, 102))(o, 0, 1));
    MENU_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, 0, menu_font_texture));
    {
        /* Stage 0: color and alpha = texture * vertex color. Stage 1 off. */
        static const UINT ts[][3] = {{0, 1, 4}, {0, 2, 2},  {0, 3, 0},  {0, 4, 4}, {0, 5, 2},
                                     {0, 6, 0}, {0, 11, 0}, {0, 24, 0}, {1, 1, 1}, {1, 4, 1}};
        for (UINT i = 0; i < sizeof(ts) / sizeof(ts[0]); i++) {
            MENU_DO(((INT(API *)(PVOID, UINT, UINT, UINT))original_method(o, 67))(
                o, ts[i][0], ts[i][1], ts[i][2]));
        }
    }
    {
        /* Alpha blending on; depth, stencil, fog, lighting, culling, scissor off. */
        static const UINT rs[][2] = {{7, 0},   {14, 0},   {15, 0},           {27, 1},  {19, 5},
                                     {20, 6},  {171, 1},  {28, 0},           {52, 0},  {137, 0},
                                     {174, 0}, {194, 0},  {206, 0},          {152, 0}, {22, 1},
                                     {8, 3},   {168, 15}, {162, 0xffffffff}, {128, 0}};
        for (UINT i = 0; i < sizeof(rs) / sizeof(rs[0]); i++) {
            MENU_DO(aa_rs(o, rs[i][0], rs[i][1]));
        }
    }
    {
        /* Point sampling keeps the 8x8 font sharp at whole-number scales. */
        static const UINT ss[][2] = {{1, 3}, {2, 3}, {5, 1}, {6, 1}, {7, 0}, {11, 0}};
        for (UINT i = 0; i < sizeof(ss) / sizeof(ss[0]); i++) {
            MENU_DO(aa_ss(o, ss[i][0], ss[i][1]));
        }
    }
    step = "draw";
    if (quads) {
        MENU_DO(((INT(API *)(PVOID, UINT, UINT, const void *, UINT))original_method(o, 83))(
            o, 4, quads * 2, menu_vertices, sizeof(MenuVertex)));
    }
    goto done;
failed:
    log_begin("In-game menu disabled after failure");
    log_text(" step=");
    log_text(step);
    log_hex_field("result", hr);
    log_emit();
    menu_failed = 1;
    menu_open = 0;
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
        log_begin("In-game menu state restoration failed");
        log_hex_field("result", restore);
        log_emit();
        menu_failed = 1;
        menu_open = 0;
    }
    com_release(block);
    for (UINT i = 0; i < 4; i++) {
        com_release(saved[i]);
    }
    com_release(depthbuf);
    com_release(back);
#undef MENU_DO
}
