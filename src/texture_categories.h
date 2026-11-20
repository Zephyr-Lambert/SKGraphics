/* Dump-only categorization. INI is read once at startup, never per texture.
 * A texture may belong to multiple categories. Matching any listed category
 * selects it in Include mode or rejects it in Exclude mode.
 */
static UINT category_mode, category_unknown = 1, category_bad, category_count, category_entries,
                           category_filtered;
static WCHAR category_selection[1024], category_sections[8192], category_section[65536];

typedef struct {
    unsigned char digest[32];
    UINT used, selected;
} CategoryEntry;

static CategoryEntry category_table[32768];

static WCHAR category_lower(WCHAR c) {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static INT category_space(WCHAR c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static INT category_word(const WCHAR *p, UINT n, const char *s) {
    UINT i = 0;
    while (s[i]) {
        if (i >= n || category_lower(p[i]) != (WCHAR)s[i]) {
            return 0;
        }
        i++;
    }
    return i == n;
}

static UINT category_len(const WCHAR *p) {
    UINT n = 0;
    while (p[n]) {
        n++;
    }
    return n;
}

static INT category_selected(const WCHAR *name) {
    UINT len = category_len(name), i = 0;
    while (category_selection[i]) {
        UINT a = i;
        while (category_selection[i] && category_selection[i] != ',') {
            i++;
        }
        UINT b = i;
        while (a < b && category_space(category_selection[a])) {
            a++;
        }
        while (b > a && category_space(category_selection[b - 1])) {
            b--;
        }
        if (b - a == len) {
            UINT j = 0;
            for (; j < len; j++) {
                if (category_lower(category_selection[a + j]) != category_lower(name[j])) {
                    break;
                }
            }
            if (j == len) {
                return 1;
            }
        }
        if (category_selection[i]) {
            i++;
        }
    }
    return 0;
}

static INT category_hex(WCHAR c) {
    c = category_lower(c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static INT category_key(const WCHAR *p, UINT n, unsigned char *digest) {
    while (n && category_space(*p)) {
        p++;
        n--;
    }
    while (n && category_space(p[n - 1])) {
        n--;
    }
    if (n == 68 && (category_word(p + 64, 4, ".dds") || category_word(p + 64, 4, ".png"))) {
        n = 64;
    }
    if (n != 64) {
        return 0;
    }
    for (UINT i = 0; i < 32; i++) {
        INT a = category_hex(p[i * 2]), b = category_hex(p[i * 2 + 1]);
        if (a < 0 || b < 0) {
            return 0;
        }
        digest[i] = (unsigned char)((a << 4) | b);
    }
    return 1;
}

static CategoryEntry *category_find(const unsigned char *d, INT add) {
    UINT h = 2166136261u;
    for (UINT i = 0; i < 32; i++) {
        h = (h ^ d[i]) * 16777619u;
    }
    for (UINT probe = 0; probe < 32768; probe++) {
        CategoryEntry *e = &category_table[(h + probe) & 32767];
        if (!e->used) {
            if (!add) {
                return 0;
            }
            if (category_entries >= 16384) {
                category_bad = 1;
                return 0;
            }
            for (UINT i = 0; i < 32; i++) {
                e->digest[i] = d[i];
            }
            e->used = 1;
            category_entries++;
            return e;
        }
        UINT i = 0;
        for (; i < 32; i++) {
            if (e->digest[i] != d[i]) {
                break;
            }
        }
        if (i == 32) {
            return e;
        }
    }
    category_bad = 1;
    return 0;
}

static void category_read_options(void) {
    static const WCHAR section[] = {'T', 'e', 'x', 't', 'u', 'r', 'e', 's', 0};
    static const WCHAR mode[] = {'D', 'u', 'm', 'p', 'C', 'a', 't', 'e', 'g',
                                 'o', 'r', 'y', 'M', 'o', 'd', 'e', 0};
    static const WCHAR names[] = {'D', 'u', 'm', 'p', 'C', 'a', 't', 'e',
                                  'g', 'o', 'r', 'i', 'e', 's', 0};
    static const WCHAR def[] = {'A', 'l', 'l', 0}, empty[] = {0};
    WCHAR value[32];
    UINT n = GetPrivateProfileStringW(section, mode, def, value, 32, path), a = 0;
    while (a < n && category_space(value[a])) {
        a++;
    }
    while (n > a && category_space(value[n - 1])) {
        n--;
    }
    if (category_word(value + a, n - a, "all")) {
        category_mode = 0;
    } else if (category_word(value + a, n - a, "include")) {
        category_mode = 1;
    } else if (category_word(value + a, n - a, "exclude")) {
        category_mode = 2;
    } else {
        category_bad = 1;
    }
    if (GetPrivateProfileStringW(section, names, empty, category_selection, 1024, path) >= 1023) {
        category_bad = 1;
    }
    category_unknown = config_int("Textures", "DumpUncategorized", 1) != 0;
}

static void category_load(const WCHAR *file) {
    UINT n = GetPrivateProfileSectionNamesW(category_sections, 8192, file), invalid = 0;
    if (n >= 8190) {
        category_bad = 1;
    }
    if (!category_bad) {
        for (UINT i = 0; i < n;) {
            WCHAR *name = category_sections + i;
            UINT len = category_len(name);
            if (!len) {
                break;
            }
            i += len + 1;
            category_count++;
            UINT size = GetPrivateProfileSectionW(name, category_section, 65536, file);
            if (size >= 65534) {
                category_bad = 1;
                break;
            }
            INT selected = category_selected(name);
            for (UINT j = 0; j < size;) {
                WCHAR *entry = category_section + j;
                UINT length = category_len(entry);
                if (!length) {
                    break;
                }
                j += length + 1;
                UINT keylen = 0;
                while (keylen < length && entry[keylen] != '=') {
                    keylen++;
                }
                unsigned char digest[32];
                if (!category_key(entry, keylen, digest)) {
                    invalid++;
                    continue;
                }
                CategoryEntry *e = category_find(digest, 1);
                if (e && selected) {
                    e->selected = 1;
                }
            }
        }
    }
    log_begin("Texture dump categories");
    log_field("mode", category_mode);
    log_field("uncategorized", category_unknown);
    log_field("sections", category_count);
    log_field("uniqueHashes", category_entries);
    log_field("invalidEntries", invalid);
    log_emit();
    if (category_bad) {
        log_begin("Texture dumping disabled: invalid filter options or category capacity exceeded; "
                  "replacements unaffected");
        log_emit();
    } else if (!category_count && category_mode) {
        log_begin("Texture category file missing or empty: all textures count as uncategorized");
        log_emit();
    }
}

static INT category_allows(const char *hash) {
    if (category_bad) {
        return 0;
    }
    unsigned char digest[32];
    for (UINT i = 0; i < 32; i++) {
        INT a = category_hex(hash[i * 2]), b = category_hex(hash[i * 2 + 1]);
        if (a < 0 || b < 0) {
            return 0;
        }
        digest[i] = (unsigned char)((a << 4) | b);
    }
    CategoryEntry *e = category_find(digest, 0);
    if (!e) {
        return category_unknown;
    }
    return category_mode == 0 ? 1 : category_mode == 1 ? e->selected : !e->selected;
}
