/* Injectable INI readers for host tests. */
static UINT (*profile_names)(WCHAR *, UINT, const WCHAR *);
static UINT (*profile_section)(const WCHAR *, WCHAR *, UINT, const WCHAR *);
static UINT (*profile_string)(const WCHAR *, const WCHAR *, const WCHAR *, WCHAR *, UINT,
                              const WCHAR *);

UINT API GetPrivateProfileStringW(const WCHAR *a, const WCHAR *b, const WCHAR *def, WCHAR *out,
                                  UINT cap, const WCHAR *p) {
    if (profile_string) {
        return profile_string(a, b, def, out, cap, p);
    }
    UINT i = 0;
    while (def[i] && i + 1 < cap) {
        out[i] = def[i];
        i++;
    }
    out[i] = 0;
    return i;
}

UINT API GetPrivateProfileSectionNamesW(WCHAR *out, UINT cap, const WCHAR *p) {
    if (profile_names) {
        return profile_names(out, cap, p);
    }
    out[0] = out[1] = 0;
    return 0;
}

UINT API GetPrivateProfileSectionW(const WCHAR *a, WCHAR *out, UINT cap, const WCHAR *p) {
    if (profile_section) {
        return profile_section(a, out, cap, p);
    }
    out[0] = out[1] = 0;
    return 0;
}

/* Live reload: tests control the INI write time and size through ini_stamp. */
static UINT ini_stamp[3], ini_stamp_missing;

INT API GetFileAttributesExW(const WCHAR *p, INT level, PVOID out) {
    (void)p;
    (void)level;
    if (ini_stamp_missing) {
        return 0;
    }
    UINT *d = out;
    for (UINT i = 0; i < 9; i++) {
        d[i] = 0;
    }
    d[5] = ini_stamp[0];
    d[6] = ini_stamp[1];
    d[8] = ini_stamp[2];
    return 1;
}

/* Menu saving: records writes as "Section.Key=Value" lines. */
static char ini_writes[4096];
static UINT ini_write_used, ini_write_fail;

INT API WritePrivateProfileStringW(const WCHAR *a, const WCHAR *b, const WCHAR *c, const WCHAR *p) {
    (void)p;
    const WCHAR *parts[3] = {a, b, c};
    const char sep[3] = {'.', '=', '\n'};
    for (UINT i = 0; i < 3; i++) {
        for (const WCHAR *w = parts[i]; *w && ini_write_used < 4000; w++) {
            ini_writes[ini_write_used++] = (char)*w;
        }
        if (ini_write_used < 4000) {
            ini_writes[ini_write_used++] = sep[i];
        }
    }
    ini_writes[ini_write_used] = 0;
    return !ini_write_fail;
}
