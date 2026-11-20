/* Configuration integration: missing keys, minimal and explicit settings. */
#define _start old_mock_entry
#include "mock.c"
#undef _start

static INT weq(const WCHAR *w, const char *s) {
    while (*s && *w == (WCHAR)*s) {
        w++;
        s++;
    }
    return !*s && !*w;
}

static UINT mode;

static UINT options(const WCHAR *a, const WCHAR *b, INT def, const WCHAR *p) {
    (void)p;
    if (mode == 1 && weq(a, "Rendering") && weq(b, "Enabled")) {
        return 1;
    }
    if (mode >= 2) {
        if (weq(a, "Diagnostics")) {
            if (weq(b, "Logging")) {
                return mode == 2;
            }
            if (weq(b, "SceneProbe") || weq(b, "AOProbe") || weq(b, "ShadowProbe")) {
                return 1;
            }
        }
        if (weq(a, "Rendering") && weq(b, "Enabled")) {
            return 1;
        }
        if (weq(a, "Antialiasing") && weq(b, "Method")) {
            return 2;
        }
        if (weq(a, "Shadows") && weq(b, "Enabled")) {
            return 1;
        }
        if (weq(a, "Shadows") && weq(b, "Filter")) {
            return 2;
        }
        if (weq(a, "Outlines") && weq(b, "Enabled")) {
            return 0;
        }
    }
    /* Modes 4-6: 2.0 shadow settings live under [Shadows]; retired test keys are ignored. */
    if (mode >= 4) {
        if (weq(a, "Diagnostics") &&
            (weq(b, "CPUShadowTest") || weq(b, "CrowdShadowTest") || weq(b, "ShadowCoverage"))) {
            return 1;
        }
        if (mode == 4 && weq(a, "Shadows") && (weq(b, "ActorShadows") || weq(b, "Coverage"))) {
            return 2;
        }
        if (mode == 5 && weq(a, "Shadows") && weq(b, "ActorShadows")) {
            return 1;
        }
        if (mode == 5 && weq(a, "Diagnostics") && weq(b, "ActorShadowCollectOnly")) {
            return 1;
        }
    }
    return def;
}

void _start(void) {
    profile_int = options;
    for (mode = 0; mode < 7; mode++) {
        initialized = 0;
        log_file = 0;
        file_creates = writes = log_events = 0;
        cpu_shadow_test = crowd_shadow_test = shadow_coverage = actor_shadows = 0;
        initialize();
        if (mode == 4 && (actor_shadows != 2 || cpu_shadow_test != 1 || !crowd_shadow_test ||
                          shadow_coverage != 2)) {
            fail = 46;
        }
        if (mode == 5 &&
            (actor_shadows != 1 || cpu_shadow_test != 2 || crowd_shadow_test || shadow_coverage)) {
            fail = 47;
        }
        if ((mode < 4 || mode == 6) &&
            (actor_shadows || cpu_shadow_test || crowd_shadow_test || shadow_coverage)) {
            fail = 48;
        }
        if (mode == 0 &&
            (render_enabled || aa_method || shadow_enabled || shadow_filter || bloom_enabled ||
             ao_enabled || texture_dump_enabled || texture_replace_enabled || load_reshade ||
             character_preview || ao_depth_view || ao_exclude_characters)) {
            fail = 40;
        }
        if (mode < 2 && (!outline_enabled || outline_thickness != 100 || outline_opacity != 100 ||
                         outline_color_override)) {
            fail = 41;
        }
        if (mode == 1 && (!render_enabled || render_width != 3840 || render_height != 2160 ||
                          aa_method || shadow_enabled || shadow_filter)) {
            fail = 42;
        }
        if (mode >= 2 && (!render_enabled || aa_method != 2 || !shadow_enabled ||
                          shadow_filter != 2 || outline_enabled)) {
            fail = 43;
        }
        if (mode == 2) {
            if (!log_file || file_creates != 1 || !writes || !scene_probe_enabled ||
                !ao_probe_enabled || !shadow_probe_enabled) {
                fail = 44;
            }
        } else if (log_file || file_creates || writes || scene_probe_enabled || ao_probe_enabled ||
                   shadow_probe_enabled) {
            fail = 45;
        }
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
