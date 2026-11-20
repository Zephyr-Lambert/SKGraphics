/* Live INI reload: change detection, settle delay, live and restart-only keys. */
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

/* Simulated INI contents. */
static UINT bloom_on = 1, strength = 35, composite, ao_on, width = 3840, actors, live = 1;

static UINT options(const WCHAR *a, const WCHAR *b, INT def, const WCHAR *p) {
    (void)p;
    if (weq(a, "Bloom") && weq(b, "Enabled")) {
        return bloom_on;
    }
    if (weq(a, "Bloom") && weq(b, "Strength")) {
        return strength;
    }
    if (weq(a, "Bloom") && weq(b, "Composite")) {
        return composite;
    }
    if (weq(a, "AmbientOcclusion") && weq(b, "Enabled")) {
        return ao_on;
    }
    if (weq(a, "Rendering") && weq(b, "Width")) {
        return width;
    }
    if (weq(a, "Shadows") && weq(b, "ActorShadows")) {
        return actors;
    }
    if (weq(a, "Compatibility") && weq(b, "LiveReload")) {
        return live;
    }
    return (UINT)def;
}

static void stamp(UINT a, UINT b, UINT c) {
    ini_stamp[0] = a;
    ini_stamp[1] = b;
    ini_stamp[2] = c;
}

/* Advance to the next poll frame (every 30 frames). */
static void poll(void) {
    frame_count = (frame_count / 30 + 1) * 30;
    live_poll();
}

void _start(void) {
    profile_int = options;
    stamp(1, 2, 3);
    initialized = 0;
    initialize();
    if (!bloom_enabled || !bloom_available || ao_enabled || ao_available || bloom_strength != 35 ||
        !live_has_stamp) {
        fail = 90;
    }

    /* Unchanged file: nothing happens. */
    poll();
    if (live_waiting || bloom_strength != 35) {
        fail = 91;
    }

    /* Edit: live keys, a restart-only key, and AO which was off at startup. */
    strength = 80;
    composite = 2;
    ao_on = 1;
    width = 2560;
    actors = 2;
    stamp(5, 6, 7);
    poll();
    if (!live_waiting || bloom_strength != 35) {
        fail = 92; /* first sighting only waits for the save to settle */
    }
    frame_count++;
    live_poll(); /* not a poll frame */
    poll();
    if (live_waiting || bloom_strength != 80 || bloom_composite != 2 || ao_enabled ||
        !(live_restart_wanted & 2) || actor_shadows != 2 || cpu_shadow_test != 1 ||
        !crowd_shadow_test || restart_raw[1] != 2560) {
        fail = 93;
    }

    /* A save still being written (stamp keeps changing) is not applied early. */
    strength = 120;
    stamp(8, 8, 8);
    poll();
    stamp(9, 9, 9);
    poll();
    if (bloom_strength != 80) {
        fail = 94;
    }
    poll();
    if (bloom_strength != 120) {
        fail = 95;
    }

    /* Bloom was on at startup, so it can be switched off and back on live. */
    bloom_on = 0;
    stamp(10, 0, 0);
    poll();
    poll();
    if (bloom_enabled) {
        fail = 96;
    }
    bloom_on = 1;
    actors = 0;
    stamp(11, 0, 0);
    poll();
    poll();
    if (!bloom_enabled || cpu_shadow_test || crowd_shadow_test) {
        fail = 97;
    }

    /* A missing file is ignored; LiveReload=0 stops polling. */
    ini_stamp_missing = 1;
    poll();
    ini_stamp_missing = 0;
    live_reload = 0;
    strength = 10;
    stamp(12, 0, 0);
    poll();
    poll();
    if (bloom_strength != 120) {
        fail = 98;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
