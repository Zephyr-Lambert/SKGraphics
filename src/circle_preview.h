/* Read the actual bound texture only during a manual scene capture.
 * DDS serialization hash is NOT the source-file hash used by replacements.
 * No pointer identity is cached; every sampled draw owns/releases its refs. */
static UINT circle_capture_id, circle_capture_reads, circle_capture_bytes;

static void circle_texture_capture(PVOID o) {
    if (!scene_recording(o)) {
        return;
    }
    if (circle_capture_id != scene_captures) {
        circle_capture_id = scene_captures;
        circle_capture_reads = 0;
        circle_capture_bytes = 0;
    }
    if (circle_capture_reads >= 64 || circle_capture_bytes >= 16 * 1048576) {
        return;
    }
    circle_capture_reads++;
    PVOID tex = 0, buffer = 0;
    SurfaceDesc d = {0};
    char hash[65] = {0};
    UINT bytes = 0;
    INT hr = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, 0, &tex);
    if (hr >= 0 && tex) {
        PVOID *v = *(PVOID **)tex;
        if (((UINT(API *)(PVOID))v[10])(tex) != 3) {
            hr = (INT)0x80004005;
        } else {
            hr = ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(tex, 0, &d);
        }
        if (hr >= 0 && (!d.w || !d.h || d.w > 512 || d.h > 512)) {
            hr = (INT)0x80004005;
        }
        if (hr >= 0) {
            PVOID lib = d3dx_library();
            TextureSaveFn save =
                (TextureSaveFn)(lib ? GetProcAddress(lib, "D3DXSaveTextureToFileInMemory") : 0);
            hr = save ? save(&buffer, 4, tex, 0) : (INT)0x80004005;
            if (hr >= 0 && buffer) {
                PVOID *b = *(PVOID **)buffer;
                bytes = ((UINT(API *)(PVOID))b[4])(buffer);
                PVOID data = ((PVOID(API *)(PVOID))b[3])(buffer);
                if (!data || !bytes || bytes > 2 * 1048576 ||
                    bytes > 16 * 1048576 - circle_capture_bytes) {
                    hr = (INT)0x80004005;
                } else {
                    texture_hash(data, bytes, hash);
                    circle_capture_bytes += bytes;
                }
            } else if (hr >= 0) {
                hr = (INT)0x80004005;
            }
        }
    } else if (hr >= 0) {
        hr = (INT)0x80004005;
    }
    scene_begin("CircleProbe bound texture");
    log_field("draw", scene_draws);
    log_hex_field("texture", (UINT)(unsigned long)tex);
    log_field("width", d.w);
    log_field("height", d.h);
    log_field("format", d.format);
    log_field("ddsBytes", bytes);
    log_hex_field("result", hr);
    log_text(" ddsHash=");
    log_text(hash[0] ? hash : "unavailable");
    scene_emit();
    com_release(buffer);
    com_release(tex);
}

/* Verified candidate textures retained until Reset/device replacement.
 * This prevents pointer reuse; at most32 small candidate resources are held. */
static PVOID circle_refs[32];
static UINT circle_matches[32], circle_ref_count;
static UINT circle_hidden, circle_kept, circle_reported, circle_log_frame;

static void circle_cleanup(void) {
    for (UINT i = 0; i < circle_ref_count; i++) {
        com_release(circle_refs[i]);
    }
    circle_ref_count = 0;
    circle_hidden = circle_kept = circle_reported = 0;
}

static INT circle_texture_known(PVOID o) {
    PVOID tex = 0;
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, 0, &tex) < 0 || !tex) {
        com_release(tex);
        return 0;
    }
    for (UINT i = 0; i < circle_ref_count; i++) {
        if (circle_refs[i] == tex) {
            com_release(tex);
            return circle_matches[i];
        }
    }
    if (circle_ref_count == 32) {
        com_release(tex);
        return 0;
    }
    PVOID *v = *(PVOID **)tex;
    SurfaceDesc d = {0};
    INT ok = 0;
    PVOID buffer = 0;
    if (((UINT(API *)(PVOID))v[10])(tex) != 3 ||
        ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(tex, 0, &d) < 0 || d.w != 128 ||
        d.h != 128 || d.format != 894720068) {
        com_release(tex);
        return 0;
    }
    {
        PVOID lib = d3dx_library();
        TextureSaveFn save =
            (TextureSaveFn)(lib ? GetProcAddress(lib, "D3DXSaveTextureToFileInMemory") : 0);
        if (save && save(&buffer, 4, tex, 0) >= 0 && buffer) {
            PVOID *b = *(PVOID **)buffer;
            UINT n = ((UINT(API *)(PVOID))b[4])(buffer);
            PVOID data = ((PVOID(API *)(PVOID))b[3])(buffer);
            if (n == 16512 && data) {
                char hash[65];
                texture_hash(data, n, hash);
                ok = cs_hash_is(hash,
                                "79c8066ae0f9f4eb9f6518863d3dc2bc4d40936af5a2e9cc8f4183b1c4c0df19");
            }
        }
    }
    com_release(buffer);
    circle_refs[circle_ref_count] = tex;
    circle_matches[circle_ref_count++] = ok;
    log_begin("Circle texture validation");
    log_field("matched", ok);
    log_field("width", d.w);
    log_field("height", d.h);
    log_emit();
    return ok;
}

/* Exact current-frame transform match except world Y (jump height).
 * Never guess nearest actor; player matches always veto suppression. */
static INT circle_world_equal(const float *a, const float *b) {
    if (!cs_finite(a, 16) || !cs_finite(b, 16)) {
        return 0;
    }
    for (UINT i = 0; i < 16; i++) {
        if (i != 13 && a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

static UINT circle_owner(const float *w) {
    for (UINT i = 0; i < cs_player_count; i++) {
        if (circle_world_equal(w, cs_players[i].world)) {
            return 2;
        }
    }
    UINT owner = 0;
    for (UINT i = 0; i < cs_counts[cs_bank]; i++) {
        CSItem *a = &cs_items[cs_bank][i];
        if (!circle_world_equal(w, a->world)) {
            continue;
        }
        if (owner && owner != a->role) {
            return 0;
        }
        owner = a->role;
    }
    return owner;
}

static PVOID circle_before(PVOID o, UINT type, UINT vertices, UINT primitives, UINT *skip) {
    *skip = 0;
    DeviceState *t = device_state(o);
    if ((!circle_preview && !actor_shadows) || !shadow_image || !t || !t->main || t->shadow_pass ||
        t->aa_busy) {
        return 0;
    }
    if (type != 5 || vertices != 4 || primitives != 2 || cs_role(o) != 5) {
        return 0;
    }
    UINT keys[] = {14, 15, 27, 19, 20}, expected[] = {0, 0, 1, 5, 6};
    for (UINT i = 0; i < 5; i++) {
        UINT value = ~0u;
        if (((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, keys[i], &value) < 0 ||
            value != expected[i]) {
            return 0;
        }
    }
    PVOID vb = 0;
    UINT offset = 0, stride = 0;
    INT stream = ((INT(API *)(PVOID, UINT, PVOID *, UINT *, UINT *))original_method(o, 101))(
        o, 0, &vb, &offset, &stride);
    INT valid = stream >= 0 && vb && stride == 32;
    com_release(vb);
    if (!valid) {
        return 0;
    }
    if (actor_shadows) {
        float w[16];
        UINT owner = 0;
        if (!cs_failed && !cs_overflow && cs_device == o && cs_writer &&
            ((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 95))(o, 0, w, 4) >= 0) {
            owner = circle_owner(w);
        }
        /* Collect-only mode draws no replacement shadows, so circles stay visible. */
        if (cpu_shadow_test == 1 && (owner == 1 || (owner == 4 && actor_shadows == 2)) &&
            circle_texture_known(o)) {
            *skip = 1;
            circle_hidden++;
        } else {
            circle_kept++;
        }
        if (scene_recording(o)) {
            scene_begin("Circle owner");
            log_field("draw", scene_draws);
            log_field("owner", owner);
            log_field("hidden", *skip);
            scene_emit();
        }
        /* Running totals, at most once per 600 frames (not per circle drawn). */
        if (!circle_reported || frame_count - circle_log_frame >= 600) {
            circle_reported = 1;
            circle_log_frame = frame_count;
            log_begin("Circle suppression");
            log_field("selection", actor_shadows);
            log_field("hiddenTotal", circle_hidden);
            log_field("keptTotal", circle_kept);
            log_emit();
        }
        return 0;
    }
    circle_texture_capture(o);
    PVOID ps = 0;
    if (((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps) < 0 || !ps) {
        com_release(ps);
        return 0;
    }
    PVOID replacement = character_variant(o, ps, 1);
    if (!replacement || ((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, replacement) < 0) {
        com_release(ps);
        return 0;
    }
    return ps;
}
