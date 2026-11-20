/* Experimental AO receiver exclusions for three shared transformation assets.
 * Hash+shader pairing from Asuka/Hibari captures. Never a whole-scene bypass.
 * Retained small textures prevent stale pointer matches; released at Reset. */
static const char *tb_shader_ids[] = {
    "d6fad140002950138bb55dce28622452c3a6b1ab67df774627a589b8c4f569d5",
    "ce7435051f0887ea339b5478ec68952257b3c8812bb714648859f07c8a78d004",
    "36c01153a95b7492c259e56f352607d7ded0002ee58f4f1880926d4cdaafc4b3"};
static const char *tb_texture_ids[] = {
    "47bd71ac197603fe342af232aee04cbf9afb1d913eeaf8b0c2e245cd81a2326b",
    "0146e4708be6a0d7959556e477adca85c564969e45f4a22a7e299b376e0507d1",
    "51e20b54f2a1abece5fac3cf8e36f93e7dc48f249fd2e25836fdc7879eee9ad3"};

static UINT tb_identify(const char *h, const char *const *ids) {
    if (!h || h[0] == '?' || !h[0]) {
        return 0;
    }
    for (UINT i = 0; i < 3; i++) {
        UINT j = 0;
        while (j < 64 && h[j] && h[j] == ids[i][j]) {
            j++;
        }
        if (j == 64) {
            return i + 1;
        }
    }
    return 0;
}

static PVOID tb_refs[256];
static UINT tb_roles[256], tb_count, tb_seen;

static void tb_cleanup(void) {
    for (UINT i = 0; i < tb_count; i++) {
        com_release(tb_refs[i]);
    }
    tb_count = tb_seen = 0;
}

static INT tb_receiver(PVOID o, UINT shader_role) {
    if (!ao_exclude_backgrounds || !shader_role || shader_role > 3) {
        return 0;
    }
    PVOID tex = 0;
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, 0, &tex) < 0 || !tex) {
        com_release(tex);
        return 0;
    }
    for (UINT i = 0; i < tb_count; i++) {
        if (tb_refs[i] == tex) {
            com_release(tex);
            return tb_roles[i] == shader_role;
        }
    }
    if (tb_count == 256) {
        com_release(tex);
        return 0;
    }
    PVOID *v = *(PVOID **)tex;
    SurfaceDesc d = {0};
    if (((UINT(API *)(PVOID))v[10])(tex) != 3 ||
        ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(tex, 0, &d) < 0 || d.w != 256 ||
        d.h != 256 || d.format != 827611204) {
        com_release(tex);
        return 0;
    }
    PVOID buffer = 0;
    UINT role = 0;
    PVOID lib = d3dx_library();
    TextureSaveFn save =
        (TextureSaveFn)(lib ? GetProcAddress(lib, "D3DXSaveTextureToFileInMemory") : 0);
    if (save && save(&buffer, 4, tex, 0) >= 0 && buffer) {
        PVOID *b = *(PVOID **)buffer;
        UINT bytes = ((UINT(API *)(PVOID))b[4])(buffer);
        PVOID data = ((PVOID(API *)(PVOID))b[3])(buffer);
        if (bytes == 32896 && data) {
            char hash[65];
            texture_hash(data, bytes, hash);
            role = tb_identify(hash, tb_texture_ids);
        }
    }
    com_release(buffer);
    tb_refs[tb_count] = tex;
    tb_roles[tb_count++] = role;
    if (role) {
        tb_seen |= 1u << role;
        log_begin("AO transformation texture identified");
        log_field("asset", role);
        log_field("shaderRole", shader_role);
        log_emit();
    }
    return role && role == shader_role;
}
