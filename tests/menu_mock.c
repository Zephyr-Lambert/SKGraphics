/* In-game menu: key handling, value steps, locked switches, saving, and the
 * draw pass against a mock device (scene pairing, state restore, failures).
 * No real keyboard, window or GPU is exercised here. */
#define AA_TEST_ENTRY aa_tests_unused
#include "aa_mock.c"

/* ---- Fake keyboard and focus ---- */
static UINT keys_down[256];
static PVOID focused = (PVOID)0x1234;

static short API fake_key(INT vk) {
    return keys_down[vk & 255] ? (short)0x8000 : 0;
}

static PVOID API fake_foreground(void) {
    return focused;
}

/* Controller: pad 1 connected with these buttons; the others absent. */
static UINT pad_buttons, pad_polls[4];
static UINT API fake_xinput(UINT index, void *out) {
    pad_polls[index & 3]++;
    if (index != 1) {
        return 1167; /* ERROR_DEVICE_NOT_CONNECTED */
    }
    ((UINT *)out)[1] = pad_buttons;
    return 0;
}

static void press(UINT buttons) {
    pad_buttons = buttons;
    menu_input();
    pad_buttons = 0;
    menu_input();
}

/* Joystick API: id 2 a Sony pad, id 5 another pad; the rest absent. */
static UINT joy_buttons[16], joy_hat[16], joy_polls[16], caps_reads;
static UINT API fake_joy_state(UINT id, void *out) {
    joy_polls[id & 15]++;
    if (id != 2 && id != 5) {
        return 167; /* JOYERR_UNPLUGGED */
    }
    UINT *info = out;
    if (info[0] != 52 || (info[1] & 0xc0) != 0xc0) {
        fail = 170;
    }
    info[8] = joy_buttons[id];
    info[10] = joy_hat[id];
    return 0;
}

static UINT API fake_joy_caps(UINT id, void *out, UINT size) {
    caps_reads++;
    if (size != 728) {
        fail = 171;
    }
    ((unsigned short *)out)[0] = id == 2 ? 0x054c : 0x045e;
    return 0;
}

static void joy(UINT id, UINT buttons, UINT hat) {
    joy_buttons[id] = buttons;
    joy_hat[id] = hat;
    menu_input();
    joy_buttons[id] = 0;
    joy_hat[id] = 65535;
    menu_input();
}

/* Hold a key for one frame, then release it for one frame. */
static void tap(UINT vk) {
    keys_down[vk] = 1;
    menu_input();
    keys_down[vk] = 0;
    menu_input();
}

static UINT item_index(UINT *value) {
    for (UINT i = 0; i < MENU_COUNT; i++) {
        if (menu_items[i].value == value) {
            return i;
        }
    }
    fail = 100;
    return 0;
}

static UINT key_index(const char *section, const char *key) {
    for (UINT i = 0; i < MENU_COUNT; i++) {
        if (menu_items[i].section && equal(menu_items[i].section, section) &&
            equal(menu_items[i].key, key)) {
            return i;
        }
    }
    fail = 99;
    return 0;
}

static INT contains(const char *text, const char *part) {
    for (; *text; text++) {
        UINT i = 0;
        while (part[i] && text[i] == part[i]) {
            i++;
        }
        if (!part[i]) {
            return 1;
        }
    }
    return 0;
}

/* ---- Fake SKGraphics_Presets.ini ---- */
static INT wide_is(const WCHAR *w, const char *s) {
    while (*s && *w == (WCHAR)*s) {
        w++;
        s++;
    }
    return !*s && !*w;
}

static UINT presets_present = 1;
static WCHAR last_preset_path[64];

static UINT fake_string(const WCHAR *a, const WCHAR *b, const WCHAR *def, WCHAR *out, UINT cap,
                        const WCHAR *path) {
    (void)def;
    for (UINT i = 0; i < 63 && path[i]; i++) {
        last_preset_path[i] = path[i];
    }
    const char *names[] = {"SKGraphics Lite (2560 x 1440)", "SKGraphics Standard (3840 x 2160)",
                           "SKGraphics Ultra (7680 x 4320)"};
    const char *v = "";
    for (UINT i = 0; i < 3 && presets_present; i++) {
        WCHAR sec[8] = {'P', 'r', 'e', 's', 'e', 't', (WCHAR)('1' + i), 0};
        if (wide_is(a, "") || !wide_is(b, "Name")) {
            continue;
        }
        UINT j = 0;
        while (sec[j] && a[j] == sec[j]) {
            j++;
        }
        if (!sec[j] && !a[j]) {
            v = names[i];
        }
    }
    UINT n = 0;
    for (; v[n] && n + 1 < cap; n++) {
        out[n] = (WCHAR)v[n];
    }
    out[n] = 0;
    return n;
}

static UINT fake_section(const WCHAR *a, WCHAR *out, UINT cap, const WCHAR *path) {
    (void)a;
    (void)path;
    /* Lines are NUL-separated, double NUL at the end, as Windows returns them. */
    static const char lines[] = "Name=Ultra\0Rendering.Width=5120\0 Shadows.Resolution = 12288 \0"
                                "; comment\0Diagnostics.Logging=1\0Bloom.Strength=\0NoDot=5\0";
    UINT n = sizeof(lines) - 1;
    for (UINT i = 0; i < n && i < cap; i++) {
        out[i] = (WCHAR)(unsigned char)lines[i];
    }
    return n - 1; /* length excluding the final terminator */
}

/* ---- Mock font texture ---- */
static PVOID fontv[21];
static PVOID fontobj = fontv;
static UINT font_pixels[64 * 128], font_creates, font_unlocks, draw_calls, draw_prims;
static UINT blend_on;

static INT API font_create(PVOID o, UINT w, UINT h, UINT levels, UINT usage, UINT fmt, UINT pool,
                           PVOID *out, PVOID shared) {
    (void)o;
    (void)shared;
    if (w != 128 || h != 64 || levels != 1 || usage || fmt != 21 || pool != 1) {
        fail = 101;
    }
    font_creates++;
    *out = &fontobj;
    return 0;
}

static INT API font_lock(PVOID o, UINT level, PVOID out, const void *rect, UINT flags) {
    (void)o;
    (void)level;
    (void)rect;
    (void)flags;

    struct {
        INT pitch;
        PVOID bits;
    } *l = out;

    l->pitch = 128 * 4;
    l->bits = font_pixels;
    return 0;
}

static INT API font_unlock(PVOID o, UINT level) {
    (void)o;
    (void)level;
    font_unlocks++;
    return 0;
}

static INT API menu_rs(PVOID o, UINT key, UINT value) {
    (void)o;
    if (key == 27) {
        blend_on = value;
    }
    return 0;
}

static INT API menu_draw_up(PVOID o, UINT type, UINT count, const void *data, UINT stride) {
    (void)o;
    (void)data;
    if (!in_scene || type != 4 || stride != 28 || !count || count % 2 || current_rt != &backobj ||
        current_depth || actualvp.w != 3840 || !blend_on) {
        fail = 102;
    }
    draw_calls++;
    draw_prims += count;
    return force_draw_failure ? (INT)0x8876086c : 0;
}

static void device_setup(void) {
    objtable[2] = release_aa;
    objtable[5] = applyblock;
    fontv[2] = release_aa;
    fontv[19] = font_lock;
    fontv[20] = font_unlock;
    sv[2] = release_aa;
    sv[12] = getdesc;
    dv[2] = release_aa;
    dv[3] = ok0;
    dv[18] = back0;
    dv[23] = font_create;
    dv[37] = writert;
    dv[38] = readrt;
    dv[39] = writedepth;
    dv[40] = readdepth;
    dv[41] = begin0;
    dv[42] = end0;
    dv[47] = writevp;
    dv[48] = readvp;
    dv[57] = menu_rs;
    dv[59] = stateblock;
    dv[65] = uintptr;
    dv[67] = threeuint;
    dv[69] = threeuint;
    dv[83] = menu_draw_up;
    dv[89] = oneuint;
    dv[92] = oneptr;
    dv[102] = twouint;
    dv[107] = oneptr;
    hook_table_record(&dobj, 119);
}

void _start(void) {
    /* Startup state: bloom on at startup, AO off at startup. */
    menu_ready = 1;
    menu_key = fake_key;
    menu_foreground = fake_foreground;
    menu_windows[0] = focused;
    bloom_enabled = bloom_available = 1;
    bloom_strength = 35;
    bloom_composite = 0;
    ao_enabled = ao_available = 0;
    actor_shadows = 0;
    actor_shadows_derive();
    movie_installed = 0;
    menu_pending_load(); /* INI defaults: no INI in this test */

    /* F9 opens; selection starts on the first selectable row (the preset). */
    tap(0x78);
    if (!menu_open || menu_items[menu_selected].kind != MENU_PRESET) {
        fail = 103;
    }
    menu_selected = item_index(&bloom_enabled);

    /* 2 moves down to Strength; = raises by its step; - lowers. */
    tap(0x32);
    if (menu_items[menu_selected].value != &bloom_strength) {
        fail = 104;
    }
    tap(0xbb);
    if (bloom_strength != 40 || !menu_dirty) {
        fail = 105;
    }
    tap(0xbd);
    tap(0xbd);
    if (bloom_strength != 30) {
        fail = 106;
    }

    /* Holding = repeats after 20 frames, every 3 frames, and stops at the maximum. */
    keys_down[0xbb] = 1;
    for (UINT i = 0; i < 200; i++) {
        menu_input();
    }
    keys_down[0xbb] = 0;
    menu_input();
    if (bloom_strength != 200) {
        fail = 107;
    }
    keys_down[0xbb] = 1;
    for (UINT i = 0; i < 22; i++) {
        menu_input(); /* 1 press + 1 repeat at frame 20 = 2 steps */
    }
    keys_down[0xbb] = 0;
    menu_input();

    /* Choices change once per press, even when held. */
    menu_selected = item_index(&bloom_composite);
    keys_down[0xbb] = 1;
    for (UINT i = 0; i < 60; i++) {
        menu_input();
    }
    keys_down[0xbb] = 0;
    menu_input();
    if (bloom_composite != 1) {
        fail = 108;
    }

    /* AO was off at startup: its switch edits the saved value only. */
    menu_selected = item_index(&ao_enabled);
    tap(0xbb);
    if (ao_enabled || menu_get(menu_selected) != 1 || !contains(menu_message, "restart")) {
        fail = 109;
    }

    /* Restart-only size list: steps through allowed sizes, clamps, no repeat. */
    menu_selected = key_index("Shadows", "Resolution");
    if (menu_get(menu_selected) != 4096) {
        fail = 125;
    }
    tap(0xbb);
    if (menu_get(menu_selected) != 8192 || shadow_size != 4096) {
        fail = 126; /* running value untouched */
    }
    keys_down[0xbb] = 1;
    for (UINT i = 0; i < 60; i++) {
        menu_input();
    }
    keys_down[0xbb] = 0;
    menu_input();
    if (menu_get(menu_selected) != 12288) {
        fail = 132; /* held for 60 frames = one step only */
    }
    tap(0xbb);
    tap(0xbb);
    if (menu_get(menu_selected) != 16384) {
        fail = 127;
    }
    tap(0xbd);
    if (menu_get(menu_selected) != 12288) {
        fail = 128;
    }

    /* Bloom was on at startup: it can be switched off and on. */
    menu_selected = item_index(&bloom_enabled);
    tap(0xbd);
    if (bloom_enabled) {
        fail = 110;
    }
    tap(0xbb);
    if (!bloom_enabled) {
        fail = 111;
    }

    /* Actor shadows: derived modes follow, captured draws are dropped. */
    menu_selected = item_index(&actor_shadows);
    cs_device = &dobj;
    tap(0xbb);
    tap(0xbb);
    if (actor_shadows != 2 || cpu_shadow_test != 1 || !crowd_shadow_test || cs_device) {
        fail = 112;
    }

    /* 1 moves up past headers and notes and wraps from the top to the last item. */
    for (UINT i = 0; i < MENU_COUNT; i++) {
        if (menu_items[i].kind == MENU_PRESET) {
            menu_selected = i;
        }
    }
    tap(0x31);
    if (menu_selected != MENU_COUNT - 1) {
        fail = 113;
    }

    /* Keys are ignored while another window has focus. */
    menu_selected = item_index(&bloom_strength);
    UINT before = bloom_strength;
    focused = (PVOID)0x9999;
    tap(0xbd);
    focused = menu_windows[0];
    if (bloom_strength != before) {
        fail = 114;
    }

    /* Save writes live and restart-only keys, and clears unsaved state. */
    for (UINT i = 0; i < MENU_COUNT; i++) {
        if (menu_items[i].kind == MENU_ACTION && menu_items[i].action == MENU_SAVE) {
            menu_selected = i;
        }
    }
    tap(0xbb);
    if (menu_dirty || !contains(ini_writes, "Bloom.Strength=200\n") ||
        !contains(ini_writes, "Bloom.Composite=1\n") ||
        !contains(ini_writes, "Shadows.ActorShadows=2\n") ||
        !contains(ini_writes, "AmbientOcclusion.Enabled=1\n") ||
        !contains(ini_writes, "Shadows.Resolution=12288\n") ||
        !contains(ini_writes, "Outlines.Enabled=1\n") ||
        !contains(ini_writes, "Movies.Volume=100\n") || contains(ini_writes, "Rendering.") ||
        contains(ini_writes, "Diagnostics.")) {
        fail = 115;
    }
    /* A failed write is reported, not hidden. */
    ini_write_fail = 1;
    tap(0xbb);
    ini_write_fail = 0;
    if (!contains(menu_message, "failed")) {
        fail = 116;
    }

    /* Discard re-reads the INI (all defaults here). */
    menu_dirty = 1;
    menu_selected++;
    tap(0xbb);
    if (menu_dirty || bloom_strength != 35 || actor_shadows ||
        menu_get(key_index("Shadows", "Resolution")) != 4096 ||
        menu_get(item_index(&ao_enabled)) != 0) {
        fail = 117;
    }

    /* Scrolling: at 1440 the list does not fit; the selection stays visible. */
    UINT rows = menu_visible_rows(1440, 3);
    if (rows >= MENU_COUNT || menu_visible_rows(720, 1) != MENU_COUNT) {
        fail = 129;
    }
    menu_selected = MENU_COUNT - 1;
    menu_layout(1440);
    if (!menu_scroll || menu_selected < menu_scroll || menu_selected >= menu_scroll + rows) {
        fail = 130;
    }
    menu_selected = 3; /* the preset row, below its header and notes */
    menu_layout(1440);
    if (menu_items[3].kind != MENU_PRESET || menu_scroll) {
        fail = 131; /* back at the top, the header and notes show again */
    }

    /* Presets: names load from SKGraphics_Presets.ini beside the INI. */
    const char *ini = "C:\\Game\\SKGraphics.ini";
    for (UINT i = 0; ini[i]; i++) {
        ini_path[i] = (WCHAR)ini[i];
    }
    profile_string = fake_string;
    profile_section = fake_section;
    menu_presets_load();
    if (menu_preset_count != 3 ||
        !equal(menu_preset_names[1], "SKGraphics Standard (3840 x 2160)") ||
        !wide_is(last_preset_path, "C:\\Game\\SKGraphics_Presets.ini")) {
        fail = 133;
    }
    /* - / = pick a preset without applying it; ends do not wrap. */
    menu_selected = 3;
    tap(0xbb);
    tap(0xbb);
    tap(0xbb);
    if (menu_preset_choice != 2 || contains(ini_writes, "Rendering.Width")) {
        fail = 134;
    }
    tap(0xbd);
    if (menu_preset_choice != 1) {
        fail = 135;
    }
    /* Apply writes graphics keys only, trimmed; bad lines are counted, not written. */
    ini_write_used = 0;
    ini_writes[0] = 0;
    menu_dirty = 1;
    menu_selected = 4;
    tap(0xbb);
    if (!contains(ini_writes, "Rendering.Width=5120\n") ||
        !contains(ini_writes, "Shadows.Resolution=12288\n") ||
        contains(ini_writes, "Diagnostics") || contains(ini_writes, "Name") ||
        contains(ini_writes, "Bloom.Strength") || menu_dirty ||
        !contains(menu_message, "with errors")) {
        fail = 136;
    }
    /* Without a presets file the row says so and Apply does nothing. */
    presets_present = 0;
    menu_presets_load();
    ini_write_used = 0;
    ini_writes[0] = 0;
    tap(0xbb);
    if (menu_preset_count || ini_write_used || !contains(menu_message, "No SKGraphics_Presets")) {
        fail = 137;
    }
    profile_string = 0;
    profile_section = 0;

    /* Every label ends before the value column, and every value (plus
     * " (restart)") ends before the scroll marker column. */
    for (UINT i = 0; i < MENU_COUNT; i++) {
        const MenuItem *m = &menu_items[i];
        UINT n = 0;
        while (m->label[n]) {
            n++;
        }
        if (m->kind <= MENU_NOTE) {
            if (n > MENU_COLUMNS - 1) {
                fail = 140;
            }
            continue;
        }
        if (m->kind == MENU_ACTION || m->kind == MENU_PRESET) {
            continue;
        }
        if (1 + n + 1 > MENU_VALUE_COLUMN) {
            fail = 141;
        }
        if (m->kind == MENU_CHOICE) {
            for (UINT v = m->min; v <= m->max; v++) {
                UINT w = 0;
                while (m->names[v][w]) {
                    w++;
                }
                if (MENU_VALUE_COLUMN + w + (m->value && !m->available ? 0 : 10) > MENU_COLUMNS - 1) {
                    fail = 142;
                }
            }
        }
    }
    /* Layout stays inside the quad budget at small and 4K heights. */
    if (!menu_layout(720) || menu_layout(720) >= MENU_MAX_QUADS || !menu_layout(2160) ||
        menu_layout(2160) >= MENU_MAX_QUADS || menu_layout(4320) >= MENU_MAX_QUADS) {
        fail = 118;
    }

    /* Draw: one scene, one batched draw, game state restored, font made once. */
    device_setup();
    DeviceState *t = device_state_new(&dobj);
    t->main = 1;
    current_rt = &oldrt;
    current_depth = &olddepth;
    actualvp = (Viewport){0, 0, 3840, 2160, 0, 1};
    in_scene = 0;
    applied = begins = ends = 0;
    menu_draw(t);
    menu_draw(t);
    if (menu_failed || begins != 2 || ends != 2 || in_scene || applied != 2 || draw_calls != 2 ||
        current_rt != &oldrt || current_depth != &olddepth || actualvp.w != 3840 ||
        font_creates != 1 || font_unlocks != 1) {
        fail = 119;
    }
    /* Glyph bits land in the atlas: 'A' row 0 is 0x0c (pixels 2 and 3 set). */
    UINT ax = (65 % 16) * 8, ay = (65 / 16) * 8;
    if (font_pixels[ay * 128 + ax + 2] != 0xffffffff || font_pixels[ay * 128 + ax] != 0x00ffffff) {
        fail = 120;
    }

    /* Closed menu draws nothing. */
    menu_open = 0;
    menu_draw(t);
    if (draw_calls != 2) {
        fail = 121;
    }

    /* A draw failure disables the menu but still restores the game's state. */
    menu_open = 1;
    force_draw_failure = 1;
    menu_draw(t);
    force_draw_failure = 0;
    if (!menu_failed || menu_open || in_scene || current_rt != &oldrt || applied != 3) {
        fail = 122;
    }
    tap(0x78);
    if (menu_open) {
        fail = 123; /* stays off until device reset */
    }
    menu_cleanup();
    if (menu_failed || menu_font_texture) {
        fail = 124;
    }

    /* Controller: LB+RB+Back+Start opens and closes; part of it does nothing;
     * the D-pad selects and changes like 1/2 and -/=. */
    menu_xinput = fake_xinput;
    menu_open = 0;
    press(0x0310); /* no Back */
    press(0x0030); /* Start + Back only */
    if (menu_open) {
        fail = 150;
    }
    press(0x0330 | 0x1000); /* combo plus another button still counts */
    if (!menu_open) {
        fail = 151;
    }
    menu_selected = item_index(&bloom_strength);
    UINT pad_before = bloom_strength;
    press(0x0008); /* right */
    if (bloom_strength <= pad_before) {
        fail = 152;
    }
    press(0x0004); /* left */
    if (bloom_strength != pad_before) {
        fail = 153;
    }
    press(0x0002); /* down */
    if (menu_items[menu_selected].value == &bloom_strength) {
        fail = 154;
    }
    press(0x0001); /* up */
    if (menu_items[menu_selected].value != &bloom_strength) {
        fail = 155;
    }
    /* Holding right repeats on number rows, like holding =. */
    pad_buttons = 0x0008;
    for (UINT i = 0; i < 30; i++) {
        menu_input();
    }
    pad_buttons = 0;
    menu_input();
    if (bloom_strength < pad_before + 3 * 5) {
        fail = 156;
    }
    bloom_strength = pad_before;
    /* D-pad does nothing while the menu is closed. */
    press(0x0330);
    if (menu_open) {
        fail = 157;
    }
    menu_selected = item_index(&bloom_strength);
    press(0x0008);
    if (bloom_strength != pad_before) {
        fail = 158;
    }
    /* Absent pads are polled about every 2 s, the connected one every frame. */
    for (UINT i = 0; i < 4; i++) {
        pad_polls[i] = 0;
    }
    for (UINT i = 0; i < 240; i++) {
        menu_input();
    }
    if (pad_polls[1] != 240 || pad_polls[0] > 3 || pad_polls[2] > 3) {
        fail = 159;
    }
    /* No input while another window has focus. */
    menu_windows[0] = (PVOID)1;
    press(0x0330);
    menu_windows[0] = focused;
    if (menu_open) {
        fail = 160;
    }

    /* Joystick API pads (for example PlayStation pads under Wine/Proton
     * without Steam Input), alone and alongside XInput. */
    for (UINT pass = 0; pass < 2; pass++) {
        menu_xinput = pass ? fake_xinput : 0;
        menu_joy_state = fake_joy_state;
        menu_joy_caps = fake_joy_caps;
        for (UINT i = 0; i < 16; i++) {
            joy_hat[i] = 65535;
            menu_joy_wait[i] = menu_joy_layout[i] = 0;
        }
        menu_open = 0;
        /* Sony: L1 (5) + R1 (6) + L2 (7) + R2 (8) is not the combination. */
        joy(2, 0x00f0, 65535);
        if (menu_open) {
            fail = 172;
        }
        /* Sony: L1 + R1 + Share (9) + Options (10) opens. */
        joy(2, 0x0330, 65535);
        if (!menu_open) {
            fail = 173;
        }
        menu_selected = item_index(&bloom_strength);
        UINT joy_before = bloom_strength;
        joy(2, 0, 9000); /* hat right */
        if (bloom_strength <= joy_before) {
            fail = 174;
        }
        joy(5, 0, 27000); /* other pad, hat left */
        if (bloom_strength != joy_before) {
            fail = 175;
        }
        joy(2, 0, 18000); /* down */
        joy(2, 0, 0);     /* up */
        if (menu_items[menu_selected].value != &bloom_strength) {
            fail = 176;
        }
        joy(2, 0, 13500); /* down-right diagonal counts as both */
        if (menu_items[menu_selected].value == &bloom_strength) {
            fail = 177;
        }
        menu_selected = item_index(&bloom_strength);
        bloom_strength = joy_before;
        bloom_threshold = 400;
        /* Other pads: LB + RB + Back (7) + Start (8) closes. */
        joy(5, 0x00f0, 65535);
        if (menu_open) {
            fail = 178;
        }
        /* Absent ids are retried about every 2 s; layout read once per pad. */
        for (UINT i = 0; i < 16; i++) {
            joy_polls[i] = 0;
        }
        caps_reads = 0;
        for (UINT i = 0; i < 240; i++) {
            menu_input();
        }
        if (joy_polls[2] != 240 || joy_polls[0] > 3 || joy_polls[15] > 3 || caps_reads) {
            fail = 179;
        }
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
