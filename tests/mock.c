/* Forwarding/rehook smoke test with mock Win32 and D3D9; not an in-game test. */
#include "../src/diagnostic.c"
static PVOID dv[119], av[17], sv[13];
static PVOID dobj = dv, aobj = av, sobj = sv;
static UINT writes, calls, fail, file_creates, protect_calls, protect_fail_at, library_loads;
static UINT (*profile_int)(const WCHAR *, const WCHAR *, INT, const WCHAR *);
static UINT tw = 1280, th = 720, tu = 1;

static INT equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

PVOID API GetModuleHandleW(const WCHAR *p) {
    (void)p;
    return 0;
}

PVOID API GetCurrentProcess(void) {
    return (PVOID)-1;
}

INT API FlushInstructionCache(PVOID p, const void *a, UINT n) {
    (void)p;
    (void)a;
    (void)n;
    return 1;
}

UINT API GetSystemDirectoryW(WCHAR *p, UINT c) {
    (void)c;
    p[0] = 'C';
    p[1] = 0;
    return 1;
}

PVOID API LoadLibraryW(const WCHAR *p) {
    /* The in-game menu loads user32.dll for keyboard access; not a backend. */
    {
        const char *user = "user32.dll";
        UINT i = 0;
        while (user[i] && p[i] == (WCHAR)user[i]) {
            i++;
        }
        if (!user[i] && !p[i]) {
            return (PVOID)3;
        }
    }
    /* The menu's optional controller support: report XInput/winmm as absent. */
    if ((p[0] == 'x' && p[1] == 'i' && p[2] == 'n' && p[3] == 'p') ||
        (p[0] == 'w' && p[1] == 'i' && p[2] == 'n' && p[3] == 'm')) {
        return 0;
    }
    library_loads++;
#ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
    const char *expected = "C\\ReShade\\d3d9.dll";
    UINT i = 0;
    while (expected[i] && p[i] == (WCHAR)expected[i]) {
        i++;
    }
    if (expected[i] || p[i]) {
        fail = 32;
    }
#else
    (void)p;
#endif
    return (PVOID)2;
}

UINT API GetModuleFileNameW(PVOID m, WCHAR *p, UINT c) {
    (void)m;
    (void)c;
    p[0] = 'C';
    p[1] = '\\';
    p[2] = 'x';
    p[3] = 0;
    return 3;
}

PVOID API CreateFileW(const WCHAR *p, UINT a, UINT b, PVOID c, UINT d, UINT e, PVOID f) {
    (void)p;
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    file_creates++;
    return (PVOID)3;
}

INT API WriteFile(PVOID a, const void *b, UINT c, UINT *d, PVOID e) {
    (void)a;
    (void)b;
    (void)e;
    *d = c;
    writes++;
    return 1;
}

INT API VirtualProtect(PVOID a, UINT b, UINT c, UINT *d) {
    (void)a;
    (void)b;
    (void)c;
    *d = 4;
    return ++protect_calls != protect_fail_at;
}

INT API CloseHandle(PVOID a) {
    (void)a;
    return 1;
}

UINT API GetPrivateProfileIntW(const WCHAR *a, const WCHAR *b, INT c, const WCHAR *d) {
    if (profile_int) {
        return profile_int(a, b, c, d);
    }
#ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
    if (a[0] == 'C' && b[0] == 'L') {
        return 1;
    }
#endif
    (void)a;
    (void)b;
    (void)d;
    return c;
}

static UINT explicit_legacy(const WCHAR *a, const WCHAR *b, INT c, const WCHAR *d) {
    (void)d;
    if (a[0] == 'R' && b[0] == 'E') {
        return 1;
    }
    if (a[0] == 'D' && b[0] == 'L') {
        return 1;
    }
#ifdef SKGRAPHICS_REVERSE_CHAIN_TEST
    if (a[0] == 'C' && b[0] == 'L') {
        return 1;
    }
#endif
    return c;
}

static PVOID API make(UINT sdk) {
    if (sdk != 32) {
        fail = 1;
    }
    return &aobj;
}

PVOID API GetProcAddress(PVOID m, const char *n) {
    (void)m;
    return equal(n, "Direct3DCreate9") ? (PVOID)make : 0;
}

static INT API mkdev(PVOID o, UINT a, UINT t, PVOID w, UINT b, PresentParams *p, PVOID *out) {
    if (o != &aobj || a != 0 || t != 1 || w != (PVOID)123 || b != 64 || p->w != 3840 ||
        p->h != 2160) {
        fail = 2;
    }
    *out = &dobj;
    calls++;
    return 0;
}

static INT API getback(PVOID o, UINT a, UINT b, UINT c, PVOID *out) {
    (void)o;
    (void)a;
    (void)b;
    (void)c;
    *out = &sobj;
    return 0;
}

static INT API getdesc(PVOID o, SurfaceDesc *d) {
    (void)o;
    d->w = 3840;
    d->h = 2160;
    d->format = 21;
    d->ms = 0;
    return 0;
}

static UINT API release(PVOID o) {
    (void)o;
    return 1;
}

static INT API reset0(PVOID o, PresentParams *p) {
    if (o != &dobj || p->w != 3840) {
        fail = 3;
    }
    calls++;
    return 0;
}

static INT API present0(PVOID o, PVOID a, PVOID b, PVOID w, PVOID d) {
    if (o != &dobj || a || b || w || d) {
        fail = 4;
    }
    calls++;
    return 0;
}

static INT API rt0(PVOID o, UINT slot, PVOID target) {
    if (o != &dobj || slot || target != &sobj) {
        fail = 5;
    }
    calls++;
    return 0;
}

static INT API view0(PVOID o, const Viewport *v) {
    if (o != &dobj || v->w != 3840 || v->h != 2160) {
        fail = 6;
    }
    calls++;
    return 0;
}

static INT API tex0(PVOID o, UINT w, UINT h, UINT l, UINT u, UINT f, UINT pool, PVOID *out,
                    PVOID shared) {
    if (o != &dobj || w != tw || h != th || l != 1 || u != tu || f != 21 || pool || shared) {
        fail = 7;
    }
    *out = (PVOID)77;
    calls++;
    return 0;
}

static INT API getview0(PVOID o, Viewport *v) {
    (void)o;
    v->x = 0;
    v->y = 0;
    v->w = 3840;
    v->h = 2160;
    return 0;
}

static INT API draw0(PVOID o, UINT type, UINT start, UINT count) {
    if (o != &dobj || type != 4 || start != 0 || count != 3) {
        fail = 17;
    }
    return 0;
}

static PVOID outer_next;
static UINT outer_calls;

static INT API outer_create(PVOID o, UINT a, UINT t, PVOID w, UINT b, PresentParams *p,
                            PVOID *out) {
    outer_calls++;
    return ((INT(API *)(PVOID, UINT, UINT, PVOID, UINT, PresentParams *, PVOID *))outer_next)(
        o, a, t, w, b, p, out);
}

void _start(void) {
    av[16] = mkdev;
    dv[16] = reset0;
    dv[17] = present0;
    dv[18] = getback;
    dv[23] = tex0;
    dv[37] = rt0;
    dv[47] = view0;
    dv[48] = getview0;
    dv[81] = draw0;
    sv[12] = getdesc;
    sv[2] = release;
    profile_int = explicit_legacy;
    DllMain((PVOID)1, 1, 0);
    for (UINT i = 0; i < 2; i++) {
        PVOID a = Direct3DCreate9(32), d = 0;
        /* ReShade wraps the shared factory vtable after our first export returns.
   * The second export must leave this wrapper in place. */
        if (i == 0) {
            outer_next = av[16];
            av[16] = outer_create;
        }
        if (av[16] != (PVOID)outer_create) {
            fail = 30;
        }
        aa_method = 0;
        PresentParams p = {0};
        p.w = 2560;
        p.h = 1440;
        p.windowed = 1;
        ((INT(API *)(PVOID, UINT, UINT, PVOID, UINT, PresentParams *, PVOID *))(*(PVOID **)a)[16])(
            a, 0, 1, (PVOID)123, 64, &p, &d);
        if (p.w != 2560 || p.h != 1440) {
            fail = 10;
        }
        ((INT(API *)(PVOID, PresentParams *))(*(PVOID **)d)[16])(d, &p);
        if (p.w != 2560 || p.h != 1440) {
            fail = 11;
        }
        ((INT(API *)(PVOID, PVOID, PVOID, PVOID, PVOID))(*(PVOID **)d)[17])(d, 0, 0, 0, 0);
        ((INT(API *)(PVOID, UINT, PVOID))(*(PVOID **)d)[37])(d, 0, &sobj);
        Viewport v = {0};
        v.w = 2560;
        v.h = 1440;
        ((INT(API *)(PVOID, const Viewport *))(*(PVOID **)d)[47])(d, &v);
        PVOID tex = 0;
        ((INT(API *)(PVOID, UINT, UINT, UINT, UINT, UINT, UINT, PVOID *, PVOID))(*(PVOID **)d)[23])(
            d, 1280, 720, 1, 1, 21, 0, &tex, 0);
        if (tex != (PVOID)77) {
            fail = 8;
        }
    }
    if (calls != 12 || writes < 10 || table_count != 2) {
        fail = 9;
    }
    Viewport result = {0};
    getviewport(&dobj, &result);
    if (result.w != 2560 || result.h != 1440) {
        fail = 18;
    }
    draw(&dobj, 4, 0, 3);
    PVOID tex = 0;
    tw = 3840;
    th = 2160;
    tu = 1;
    texture(&dobj, 2560, 1440, 1, 1, 21, 0, &tex, 0);
    tw = 2048;
    th = 2048;
    texture(&dobj, 2048, 2048, 1, 1, 21, 0, &tex, 0);
    tw = 2560;
    th = 1440;
    tu = 0;
    texture(&dobj, 2560, 1440, 1, 0, 21, 0, &tex, 0);
    DeviceState t = {0};
    PresentParams p = {0};
    p.w = 1920;
    p.h = 1080;
    p.windowed = 1;
    select_size(&t, &p);
    if (!t.active || t.rw != 3840 || t.lw != 1920) {
        fail = 12;
    }
    /* Other screen shapes: enlarged on one axis to the output's shape. */
    p.w = 1280;
    p.h = 800; /* Steam Deck, 16:10 */
    select_size(&t, &p);
    if (!t.active || t.rw != 3840 || t.rh != 2400) {
        fail = 13;
    }
    p.w = 3440;
    p.h = 1440; /* ultrawide */
    select_size(&t, &p);
    if (!t.active || t.rw != 5160 || t.rh != 2160) {
        fail = 19;
    }
    p.w = 1366;
    p.h = 768; /* not exactly 16:9: rounded, at most half a pixel off */
    select_size(&t, &p);
    if (!t.active || t.rw != 3842 || t.rh != 2160) {
        fail = 20;
    }
    p.w = 1080;
    p.h = 1920; /* portrait: fitted height would exceed 16384 */
    render_width = 16000;
    render_height = 9000;
    select_size(&t, &p);
    render_width = 3840;
    render_height = 2160;
    if (t.active || t.rw != 1080) {
        fail = 21;
    }
    p.w = 1920;
    p.h = 1080;
    p.windowed = 0;
    select_size(&t, &p);
    if (t.active) {
        fail = 14;
    }
    p.windowed = 1;
    render_enabled = 0;
    select_size(&t, &p);
    if (t.active) {
        fail = 15;
    }
    render_enabled = 1;
    select_size(&t, &p);
    Rect a = {0, 0, 1920, 1080}, b;
    rectscale(&t, &a, &b);
    if (b.right != 3840 || b.bottom != 2160) {
        fail = 16;
    }

    if (outer_calls != 2) {
        fail = 31;
    }
    DllMain((PVOID)1, 0, 0);
#if defined(__x86_64__)
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
#else
    __asm__ volatile("int $0x80" ::"a"(1), "b"(fail) : "memory");
#endif
    __builtin_unreachable();
}

UINT API GetFileAttributesW(const WCHAR *p) {
    (void)p;
    return 0xffffffff;
}

INT API CreateDirectoryW(const WCHAR *p, PVOID a) {
    (void)p;
    (void)a;
    return 1;
}

INT API DeleteFileW(const WCHAR *p) {
    (void)p;
    return 1;
}

void API InitializeCriticalSection(PVOID p) {
    (void)p;
}

void API EnterCriticalSection(PVOID p) {
    (void)p;
}

void API LeaveCriticalSection(PVOID p) {
    (void)p;
}

#include "profile_stubs.h"
