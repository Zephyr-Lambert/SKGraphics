/* GOG and Steam 01.06: CRI Mana AtomVoice calls only. Do not hook the shared Atom
 * volume setter: it is also used by ordinary game audio. Cdecl at both sites.
 * Install before video playback, once, from Direct3D initialization. */
static UINT movie_installed;
static volatile UINT movie_calls;
static UINT (*movie_original)(PVOID, float);

static float movie_gain(float v) {
    return v * ((float)movie_percent / 100.0f);
}

static UINT movie_set_volume(PVOID voice, float volume) {
    __sync_fetch_and_add(&movie_calls, 1);
    return movie_original(voice, movie_gain(volume));
}

/* Code section of the loaded game: the first section marked executable. */
static INT movie_code(unsigned char *b, unsigned char **start, UINT *size) {
    UINT pe = sh_u32(b + 60), count = b[pe + 6] | b[pe + 7] << 8;
    unsigned char *h = b + pe + 24 + (b[pe + 20] | b[pe + 21] << 8);
    for (UINT i = 0; i < count && i < 16; i++, h += 40) {
        if (sh_u32(h + 36) & 0x20) {
            *start = b + sh_u32(h + 12);
            *size = sh_u32(h + 8);
            return *size > 64;
        }
    }
    return 0;
}

/* The setter and its two movie call sites are found by their bytes in the
 * loaded code, so the same search works for GOG and Steam (whose code is only
 * readable once the game runs). Each call site looks like:
 *   mov reg, [reg+148h] ; push reg ; call setter ; add esp, 8
 * The game's third caller (ordinary audio) has a different shape. */
static void movie_install(void) {
    if (!movie_enabled || movie_installed) {
        return;
    }
    unsigned char *b = GetModuleHandleW(0);
    if (!shadow_identify(b)) {
        log_begin("Movie volume skipped: unsupported executable");
        log_emit();
        return;
    }
    static const unsigned char setter[] = {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0xf3, 0x0f,
                                           0x10, 0x45, 0x0c, 0x80, 0x48, 0x17, 0x01, 0xf3,
                                           0x0f, 0x11, 0x40, 0x2c, 0x5d, 0xc3};
    unsigned char *code, *target = 0, *site[3] = {0};
    UINT size, setters = 0, sites = 0;
    if (!movie_code(b, &code, &size)) {
        log_begin("Movie volume skipped: code section not found");
        log_emit();
        return;
    }
    for (UINT i = 0; i + sizeof(setter) <= size; i++) {
        if (code[i] == 0x55 && sh_equal(code + i, setter, sizeof(setter))) {
            target = code + i;
            setters++;
        }
    }
    for (UINT i = 0; target && setters == 1 && i + 15 <= size; i++) {
        unsigned char *c = code + i;
        if (c[0] == 0x8b && c[2] == 0x48 && c[3] == 1 && !c[4] && !c[5] && (c[1] & 0xc0) == 0x80 &&
            (c[6] & 0xf8) == 0x50 && c[7] == 0xe8 && c[12] == 0x83 && c[13] == 0xc4 &&
            c[14] == 8 && c + 12 + (INT)sh_u32(c + 8) == target) {
            if (sites < 3) {
                site[sites] = c + 7;
            }
            sites++;
        }
    }
    if (setters != 1 || sites != 2) {
        log_begin("Movie volume skipped: call-site signature mismatch");
        log_field("setters", setters);
        log_field("sites", sites);
        log_emit();
        return;
    }
    unsigned char *p = site[0], *q = site[1];
    UINT keep_p = sh_u32(p + 1), keep_q = sh_u32(q + 1), old, unused;
    UINT length = (UINT)(q - p) + 5;
    if (!VirtualProtect(p, length, 0x40, &old)) {
        log_begin("Movie volume skipped: page protection failed");
        log_emit();
        return;
    }
    movie_original = (UINT(*)(PVOID, float))target;
    sh_put32(p + 1,
             (UINT)(unsigned long)(PVOID)movie_set_volume - (UINT)(unsigned long)(PVOID)(p + 5));
    sh_put32(q + 1,
             (UINT)(unsigned long)(PVOID)movie_set_volume - (UINT)(unsigned long)(PVOID)(q + 5));
    const char *problem = 0;
    if (!FlushInstructionCache(GetCurrentProcess(), p, length)) {
        problem = "Movie volume skipped: instruction cache flush failed; calls restored";
    } else if (!VirtualProtect(p, length, old, &unused)) {
        problem = "Movie volume skipped: protection restoration failed; calls restored";
    }
    if (problem) {
        sh_put32(p + 1, keep_p);
        sh_put32(q + 1, keep_q);
        FlushInstructionCache(GetCurrentProcess(), p, length);
        VirtualProtect(p, length, old, &unused);
        log_begin(problem);
        log_emit();
        return;
    }
    movie_installed = 1;
    log_begin("Movie volume installed");
    log_field("percent", movie_percent);
    log_hex_field("setterRVA", (UINT)(target - b));
    log_hex_field("firstSiteRVA", (UINT)(p - b));
    log_hex_field("secondSiteRVA", (UINT)(q - b));
    log_emit();
}
