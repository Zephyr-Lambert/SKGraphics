#include "../src/diagnostic.c"
static unsigned char exe[0x360000];
static PVOID dev_vt[119], tex_vt[19], surf_vt[3], dev = dev_vt, tex[2], surf[2];
static UINT fail, allocations, releases, protects, flushes, allocation_failure, protection_failure,
    flush_failure, cap_limit = 8192;

PVOID API GetModuleHandleW(const WCHAR *p) {
    (void)p;
    return exe;
}

PVOID API GetCurrentProcess(void) {
    return (PVOID)-1;
}

INT API FlushInstructionCache(PVOID p, const void *a, UINT n) {
    (void)p;
    (void)a;
    (void)n;
    return ++flushes != flush_failure;
}

INT API VirtualProtect(PVOID p, UINT n, UINT access, UINT *old) {
    (void)p;
    (void)n;
    (void)access;
    *old = 0x20;
    return ++protects != protection_failure;
}

UINT API GetSystemDirectoryW(WCHAR *p, UINT n) {
    (void)p;
    (void)n;
    return 0;
}

PVOID API LoadLibraryW(const WCHAR *p) {
    (void)p;
    return 0;
}

PVOID API GetProcAddress(PVOID p, const char *n) {
    (void)p;
    (void)n;
    return 0;
}

UINT API GetModuleFileNameW(PVOID p, WCHAR *q, UINT n) {
    (void)p;
    (void)q;
    (void)n;
    return 0;
}

PVOID API CreateFileW(const WCHAR *p, UINT a, UINT b, PVOID c, UINT d, UINT e, PVOID f) {
    (void)p;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    return 0;
}

INT API WriteFile(PVOID p, const void *q, UINT n, UINT *w, PVOID x) {
    (void)p;
    (void)q;
    (void)x;
    *w = n;
    return 1;
}

INT API CloseHandle(PVOID p) {
    (void)p;
    return 1;
}

UINT API GetPrivateProfileIntW(const WCHAR *a, const WCHAR *b, INT c, const WCHAR *d) {
    (void)a;
    (void)b;
    (void)d;
    return c;
}

static void fixture(void) {
    for (UINT i = 0; i < sizeof(exe); i++) {
        exe[i] = 0;
    }
    exe[0] = 'M';
    exe[1] = 'Z';
    sh_put32(exe + 60, 0x100);
    sh_put32(exe + 0x100, 0x4550);
    exe[0x104] = 0x4c;
    exe[0x105] = 1;
    sh_put32(exe + 0x108, 0x62f1e445);
    exe[0x118] = 0x0b;
    exe[0x119] = 1;
    sh_put32(exe + 0x118 + 56, 0xe4e000);
    for (UINT i = 0; i < sizeof(shadow_site); i++) {
        exe[0x54845 + i] = shadow_site[i];
    }
    /* One executable section covering the test image. */
    exe[0x114] = 0xe0;
    exe[0x106] = 1;
    sh_put32(exe + 0x1f8 + 8, sizeof(exe) - 0x1000);
    sh_put32(exe + 0x1f8 + 12, 0x1000);
    sh_put32(exe + 0x1f8 + 36, 0x60000020);
    movie_installed = 0;
    movie_enabled = 1;
    movie_percent = 25;
    protects = flushes = protection_failure = flush_failure = 0;
}

static const unsigned char setter[] = {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0xf3, 0x0f,
                                       0x10, 0x45, 0x0c, 0x80, 0x48, 0x17, 0x01, 0xf3,
                                       0x0f, 0x11, 0x40, 0x2c, 0x5d, 0xc3};
static const unsigned char a[] = {0x8b, 0x91, 0x48, 1,    0,    0,    0x52, 0xe8,
                                  0x6f, 0x89, 0xf9, 0xff, 0x83, 0xc4, 8};
static const unsigned char c[] = {0x8b, 0x88, 0x48, 1,    0,    0,    0x51, 0xe8,
                                  0xe6, 0x84, 0xf9, 0xff, 0x83, 0xc4, 8};

static void valid(void) {
    fixture();
    for (UINT i = 0; i < sizeof(setter); i++) {
        exe[0x1c3e70 + i] = setter[i];
    }
    for (UINT i = 0; i < sizeof(a); i++) {
        exe[0x22b4f5 + i] = a[i];
        exe[0x22b97e + i] = c[i];
    }
}

/* The game's ordinary-audio caller of the same setter: must stay untouched. */
static void audio_caller(void) {
    static const unsigned char d[] = {0xff, 0xb0, 0xdc, 0, 0, 0, 0xe8, 0xcd, 0xea, 0xff, 0xff};
    for (UINT i = 0; i < sizeof(d); i++) {
        exe[0x1c5398 + i] = d[i];
    }
}

/* Steam 01.06 identification fields, plus the setter and sites moved to
 * other addresses (Steam code differs from GOG). */
static void steam(void) {
    fixture();
    sh_put32(exe + 0x108, game_steam.timestamp);
    sh_put32(exe + 0x118 + 56, game_steam.image_size);
    for (UINT i = 0; i < sizeof(steam_shadow_site); i++) {
        exe[game_steam.shadow_rva + i] = steam_shadow_site[i];
    }
    static const unsigned char setrt[] = {0xff, 0x91, 0x94, 0, 0, 0};
    static const unsigned char draw[] = {0xff, 0x91, 0x48, 1, 0, 0};
    for (UINT i = 0; i < 6; i++) {
        exe[game_steam.binding_rva - 6 + i] = setrt[i];
        exe[game_steam.fade_rva - 6 + i] = draw[i];
        exe[game_steam.copy_rva - 6 + i] = draw[i];
        exe[game_steam.scene_copy_rva - 6 + i] = draw[i];
    }
    UINT set = 0x1c8000, s1 = 0x230100, s2 = 0x230700;
    for (UINT i = 0; i < sizeof(setter); i++) {
        exe[set + i] = setter[i];
    }
    for (UINT i = 0; i < sizeof(a); i++) {
        exe[s1 + i] = a[i];
        exe[s2 + i] = c[i];
    }
    sh_put32(exe + s1 + 8, set - (s1 + 12));
    sh_put32(exe + s2 + 8, set - (s2 + 12));
}

static UINT call_target(UINT site) {
    return (UINT)(unsigned long)(exe + site + 12) + sh_u32(exe + site + 8);
}

static INT intact(void) {
    return sh_equal(exe + 0x22b4f5, a, sizeof(a)) && sh_equal(exe + 0x22b97e, c, sizeof(c));
}

void _start(void) {
    valid();
    movie_enabled = 0;
    movie_install();
    if (protects || !intact()) {
        fail = 1;
    }
    valid();
    exe[0x22b97e] = 0;
    movie_install();
    if (protects || movie_installed) {
        fail = 2;
    }
    valid();
    protection_failure = 1;
    movie_install();
    if (movie_installed || !intact()) {
        fail = 3;
    }
    valid();
    flush_failure = 1;
    movie_install();
    if (movie_installed || !intact()) {
        fail = 4;
    }
    valid();
    protection_failure = 2;
    movie_install();
    if (movie_installed || !intact()) {
        fail = 5;
    }
    valid();
    movie_install();
    if (!movie_installed || protects != 2 || flushes != 1 || intact()) {
        fail = 6;
    }
    movie_install();
    if (protects != 2) {
        fail = 7;
    }
    /* The ordinary-audio caller is never redirected. */
    valid();
    audio_caller();
    movie_install();
    if (!movie_installed || sh_u32(exe + 0x1c5398 + 7) != 0xffffeacd) {
        fail = 11;
    }
    /* Steam: found at different addresses, both calls redirected, the
     * original setter remembered for the volume call itself. */
    steam();
    movie_install();
    if (!movie_installed || call_target(0x230100) != (UINT)(unsigned long)(PVOID)movie_set_volume ||
        call_target(0x230700) != (UINT)(unsigned long)(PVOID)movie_set_volume ||
        (PVOID)movie_original != (PVOID)(exe + 0x1c8000)) {
        fail = 12;
    }
    /* A third matching caller makes the search ambiguous: install nothing. */
    steam();
    for (UINT i = 0; i < sizeof(a); i++) {
        exe[0x231000 + i] = a[i];
    }
    sh_put32(exe + 0x231000 + 8, 0x1c8000 - (0x231000 + 12));
    movie_install();
    if (movie_installed || protects) {
        fail = 13;
    }
    /* A failed flush on Steam restores the moved calls exactly. */
    steam();
    flush_failure = 1;
    movie_install();
    if (movie_installed || call_target(0x230100) != (UINT)(unsigned long)(exe + 0x1c8000) ||
        call_target(0x230700) != (UINT)(unsigned long)(exe + 0x1c8000)) {
        fail = 14;
    }
    movie_percent = 25;
    if (movie_gain(0.8f) < 0.199f || movie_gain(0.8f) > 0.201f) {
        fail = 8;
    }
    movie_percent = 0;
    if (movie_gain(1.0f) != 0.0f) {
        fail = 9;
    }
    movie_percent = 100;
    if (movie_gain(0.5f) != 0.5f) {
        fail = 10;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
