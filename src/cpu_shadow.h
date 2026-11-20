/* Experimental one-frame-delayed CPU-playable-character shadow replay.
 * No game-memory hooks. GPU-skinned, non-dynamic indexed triangle lists only.
 * Retained buffers are NOT frozen copies: non-dynamic in-place edits remain a
 * limitation of this experiment. No cache survives a missing frame or Reset.
 */
#define CS_MAX 512
#define CS_PLAYER_MAX 128

typedef struct {
    PVOID vb, ib, decl;
    UINT offset, stride, minimum, vertices, start, primitives, role;
    INT base;
    float world[16], bones[512];
} CSItem;

static CSItem cs_items[2][CS_MAX], cs_players[CS_PLAYER_MAX];
static UINT cs_counts[2], cs_bank, cs_player_count, cs_ready_frame, cs_ready;
static UINT cs_overflow, cs_failed, cs_flushed, cs_written, cs_replayed, cs_skipped, cs_frames;
static PVOID cs_device, cs_writer, cs_rt, cs_depth;
static float cs_light[16];
static PVOID cs_shader_refs[128];
static UINT cs_shader_roles[128], cs_shader_count;
static UINT cs_shader_reads, cs_shader_hits, cs_crowd_replayed, cs_crowd_reported;

static float cs_abs(float x) {
    return x < 0 ? -x : x;
}

static INT cs_finite(const float *p, UINT n) {
    for (UINT i = 0; i < n; i++) {
        union {
            float f;
            UINT u;
        } v;

        v.f = p[i];
        if ((v.u & 0x7f800000) == 0x7f800000) {
            return 0;
        }
    }
    return 1;
}

static void cs_multiply(const float *a, const float *b, float *out) {
    for (UINT i = 0; i < 4; i++) {
        for (UINT j = 0; j < 4; j++) {
            float x = 0;
            for (UINT k = 0; k < 4; k++) {
                x += a[i * 4 + k] * b[k * 4 + j];
            }
            out[i * 4 + j] = x;
        }
    }
}

static INT cs_light_matrix(const float *w, const float *l, float *out) {
    if (!cs_finite(w, 16) || !cs_finite(l, 16) ||
        cs_abs(w[3]) + cs_abs(w[7]) + cs_abs(w[11]) > 0.00001f || cs_abs(w[15] - 1) > 0.00001f) {
        return 0;
    }
    float det = w[0] * (w[5] * w[10] - w[6] * w[9]) - w[1] * (w[4] * w[10] - w[6] * w[8]) +
                w[2] * (w[4] * w[9] - w[5] * w[8]);
    if (cs_abs(det) < 0.000001f) {
        return 0;
    }
    float inv[16] = {0};
    inv[0] = (w[5] * w[10] - w[6] * w[9]) / det;
    inv[1] = (w[2] * w[9] - w[1] * w[10]) / det;
    inv[2] = (w[1] * w[6] - w[2] * w[5]) / det;
    inv[4] = (w[6] * w[8] - w[4] * w[10]) / det;
    inv[5] = (w[0] * w[10] - w[2] * w[8]) / det;
    inv[6] = (w[2] * w[4] - w[0] * w[6]) / det;
    inv[8] = (w[4] * w[9] - w[5] * w[8]) / det;
    inv[9] = (w[1] * w[8] - w[0] * w[9]) / det;
    inv[10] = (w[0] * w[5] - w[1] * w[4]) / det;
    inv[15] = 1;
    for (UINT j = 0; j < 3; j++) {
        inv[12 + j] = -(w[12] * inv[j] + w[13] * inv[4 + j] + w[14] * inv[8 + j]);
    }
    cs_multiply(inv, l, out);
    return cs_finite(out, 16);
}

static void cs_drop(CSItem *i) {
    com_release(i->vb);
    com_release(i->ib);
    com_release(i->decl);
    i->vb = i->ib = i->decl = 0;
}

static void cs_drop_bank(UINT b) {
    for (UINT i = 0; i < cs_counts[b]; i++) {
        cs_drop(&cs_items[b][i]);
    }
    cs_counts[b] = 0;
}

static void cs_drop_frame(void) {
    for (UINT i = 0; i < cs_player_count; i++) {
        cs_drop(&cs_players[i]);
    }
    cs_player_count = 0;
    com_release(cs_writer);
    com_release(cs_rt);
    com_release(cs_depth);
    cs_writer = cs_rt = cs_depth = 0;
    cs_written = cs_flushed = 0;
}

static void cs_cleanup(void) {
    cs_drop_bank(0);
    cs_drop_bank(1);
    cs_drop_frame();
    for (UINT i = 0; i < cs_shader_count; i++) {
        com_release(cs_shader_refs[i]);
    }
    cs_shader_count = 0;
    cs_crowd_reported = cs_crowd_replayed = 0;
    cs_shader_reads = cs_shader_hits = 0;
    cs_ready = cs_failed = cs_overflow = cs_bank = 0;
    cs_device = 0;
}

static void cs_fail(const char *why) {
    if (!cs_failed) {
        log_begin("Actor shadows disabled until reset");
        log_text(" reason=");
        log_text(why);
        log_emit();
    }
    cs_failed = 1;
}

static INT cs_hash_is(const char *a, const char *b) {
    for (UINT i = 0; i < 64; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/* Only enable the previously observed crowd VS when its constant table
 * confirms the same 32-matrix bone palette layout as the player writer.
 * Unknown/stripped tables remain diagnostic evidence, never a guessed layout. */
static INT cs_crowd_layout(const UINT *code, UINT bytes) {
    if (bytes < 8 || bytes % 4 || bytes > sizeof(scene_code) || code[0] != 0xfffe0300) {
        return 0;
    }
    UINT p = 1, count = bytes / 4;
    while (p < count) {
        UINT t = code[p], op = t & 65535, n = op == 65534 ? (t >> 16) & 32767 : (t >> 24) & 15;
        if (n > count - p - 1) {
            return 0;
        }
        if (op == 65534 && n >= 8 && code[p + 1] == 0x42415443) {
            const unsigned char *b = (const unsigned char *)(code + p + 2);
            UINT size = (n - 1) * 4;
            UINT entries = sh_u32(b + 12), offset = sh_u32(b + 16);
            if (offset > size || entries > (size - offset) / 20) {
                return 0;
            }
            for (UINT j = 0; j < entries; j++) {
                const unsigned char *e = b + offset + j * 20;
                UINT name = sh_u32(e), type = sh_u32(e + 12);
                static const unsigned char bone[] = "g_matBone";
                if (name > size || sizeof(bone) > size - name ||
                    !sh_equal(b + name, bone, sizeof(bone))) {
                    continue;
                }
                if (type > size || 16 > size - type) {
                    return 0;
                }
                const unsigned char *q = b + type;
                return e[4] == 2 && !e[5] && e[6] == 16 && !e[7] && e[8] == 128 && !e[9] &&
                       q[0] == 3 && !q[1] && q[2] == 3 && !q[3] && q[4] == 4 && !q[5] &&
                       q[6] == 4 && !q[7] && q[8] == 32 && !q[9];
            }
        }
        if (op == 65535) {
            break;
        }
        p += n + 1;
    }
    return 0;
}

static INT cs_crowd_check(PVOID vs) {
    UINT bytes = 0;
    PVOID *v = *(PVOID **)vs;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(vs, 0, &bytes) < 0 || bytes < 8 ||
        bytes > sizeof(scene_code) || bytes % 4) {
        return 0;
    }
    UINT capacity = bytes;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(vs, scene_code, &capacity) < 0 ||
        capacity != bytes) {
        return 0;
    }
    INT ok = cs_crowd_layout(scene_code, bytes);
    if (!cs_crowd_reported) {
        cs_crowd_reported = 1;
        log_begin("Crowd shadow shader layout");
        log_field("compatibleBonePalette", ok);
        log_field("bytes", bytes);
        log_emit();
        /* One bounded shader dump per device session, no F8 capture required. */
        for (UINT i = 0; i < bytes / 4; i += 24) {
            log_begin("Crowd shadow shader code");
            log_field("word", i);
            for (UINT j = i; j < bytes / 4 && j < i + 24; j++) {
                log_text(" ");
                log_hex_number(scene_code[j]);
            }
            log_emit();
        }
    }
    return ok;
}

static UINT cs_role(PVOID o) {
    PVOID vs = 0;
    if (((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &vs) < 0 || !vs) {
        com_release(vs);
        return 0;
    }
    for (UINT i = 0; i < cs_shader_count; i++) {
        if (cs_shader_refs[i] == vs) {
            cs_shader_hits++;
            com_release(vs);
            return cs_shader_roles[i];
        }
    }
    char h[65];
    cs_shader_reads++;
    scene_shader(vs, h);
    UINT role = 0;
    if (cs_hash_is(h, "15dfdc31edb7fed6fb5db53903fc70a554cfafdd79f27c3b3d91c069d5c62be7") ||
        cs_hash_is(h, "a71e412f97d8faffbf6cea5a0782b0a8428c03753132d208516ec27e2c076394")) {
        role = 1;
    }
    if (cs_hash_is(h, "d316c720775d884291cf814853d21a2c755c778d4f3c29b26a01492e66e86372")) {
        role = 2;
    }
    if (cs_hash_is(h, "37ddb1d1007174af6cee0b44c6e2d00179f767d7daf731cd68152b25c3de58f2") ||
        cs_hash_is(h, "e12a27770f809981713b650c3df251c060562e750addd859807e65ec7f06fec0")) {
        role = 3;
    }
    if (crowd_shadow_test &&
        cs_hash_is(h, "f1ebe9237bd023f31d074fb535f4de87d84e99ed8d53712949c5e868ecf85fe1") &&
        cs_crowd_check(vs)) {
        role = 4;
    }
    if (cs_hash_is(h, "ce7435051f0887ea339b5478ec68952257b3c8812bb714648859f07c8a78d004")) {
        role = 5;
    }
    /* Cache negative results too. Owning the COM reference prevents address
  * reuse from misclassifying a later shader. Bounded until reset/cleanup. */
    if (cs_shader_count < 128) {
        cs_shader_refs[cs_shader_count] = vs;
        cs_shader_roles[cs_shader_count++] = role;
    } else {
        com_release(vs);
    }
    return role;
}

static INT cs_equal(const CSItem *a, const CSItem *b) {
    if (a->vb != b->vb || a->ib != b->ib || a->offset != b->offset || a->stride != b->stride ||
        a->base != b->base || a->minimum != b->minimum || a->vertices != b->vertices ||
        a->start != b->start || a->primitives != b->primitives) {
        return 0;
    }
    for (UINT i = 0; i < 16; i++) {
        if (a->world[i] != b->world[i]) {
            return 0;
        }
    }
    return 1;
}

static INT cs_geometry(PVOID o, CSItem *i, INT base, UINT minimum, UINT vertices, UINT start,
                       UINT primitives) {
    *i = (CSItem){0};
    i->base = base;
    i->minimum = minimum;
    i->vertices = vertices;
    i->start = start;
    i->primitives = primitives;
    if (!vertices || !primitives || primitives > 1000000) {
        return 0;
    }
    if (((INT(API *)(PVOID, UINT, PVOID *, UINT *, UINT *))original_method(o, 101))(
            o, 0, &i->vb, &i->offset, &i->stride) < 0 ||
        !i->vb || !i->stride || i->stride > 256) {
        goto fail;
    }
    if (((INT(API *)(PVOID, PVOID *))original_method(o, 105))(o, &i->ib) < 0 || !i->ib ||
        ((INT(API *)(PVOID, PVOID *))original_method(o, 88))(o, &i->decl) < 0 || !i->decl) {
        goto fail;
    }
    UINT frequency = 0;
    if (((INT(API *)(PVOID, UINT, UINT *))original_method(o, 103))(o, 0, &frequency) < 0 ||
        frequency != 1) {
        goto fail;
    }
    for (UINT j = 1; j < 4; j++) {
        PVOID other = 0;
        UINT offset, stride;
        INT hr = ((INT(API *)(PVOID, UINT, PVOID *, UINT *, UINT *))original_method(o, 101))(
            o, j, &other, &offset, &stride);
        INT bad = hr < 0 || other != 0;
        com_release(other);
        if (bad) {
            goto fail;
        }
    }
    UINT vd[6] = {0}, id[6] = {0};
    PVOID *v = *(PVOID **)i->vb, *b = *(PVOID **)i->ib;
    if (((INT(API *)(PVOID, PVOID))v[13])(i->vb, vd) < 0 ||
        ((INT(API *)(PVOID, PVOID))b[13])(i->ib, id) < 0 || (vd[2] & 0x200) || (id[2] & 0x200)) {
        goto fail;
    }
    UINT width = id[0] == 101 ? 2 : id[0] == 102 ? 4 : 0;
    if (!width || start > id[4] / width || primitives > (id[4] / width - start) / 3) {
        goto fail;
    }
    /* Signed base+minimum bounds without overflow on either host ABI. */
    if (minimum > 0x7fffffff || base > 0x7fffffff - (INT)minimum || base < -(INT)minimum) {
        goto fail;
    }
    UINT first = (UINT)(base + (INT)minimum);
    if (i->offset > vd[4] || first > (vd[4] - i->offset) / i->stride ||
        vertices > (vd[4] - i->offset) / i->stride - first) {
        goto fail;
    }
    if (((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 95))(o, 0, i->world, 4) < 0 ||
        !cs_finite(i->world, 16)) {
        goto fail;
    }
    return 1;
fail:
    cs_drop(i);
    return 0;
}

static void cs_indexed(PVOID o, UINT type, INT base, UINT minimum, UINT vertices, UINT start,
                       UINT primitives) {
    DeviceState *t = device_state(o);
    if (!cpu_shadow_test || cs_failed || !shadow_image || !t || type != 4 || t->aa_busy) {
        return;
    }
    if (cs_device && cs_device != o) {
        return;
    }
    cs_device = o;
    if (!t->shadow_pass && (!t->main || !cs_writer)) {
        return;
    }
    UINT role = cs_role(o);
    if ((t->shadow_pass && role != 2) ||
        (!t->shadow_pass &&
         (!t->main || (role != 1 && !(role == 4 && crowd_shadow_test)) || !cs_writer))) {
        return;
    }
    CSItem item;
    if (!cs_geometry(o, &item, base, minimum, vertices, start, primitives)) {
        cs_skipped++;
        return;
    }
    item.role = role;
    if (t->shadow_pass) {
        if (cs_player_count == CS_PLAYER_MAX) {
            cs_drop(&item);
            cs_fail("player draw limit");
            return;
        }
        cs_players[cs_player_count++] = item;
        cs_written++;
        if (!cs_writer) {
            float l[16];
            PVOID ps = 0;
            char h[65];
            INT ok = ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps) >= 0 && ps;
            if (ok) {
                scene_shader(ps, h);
                ok = cs_hash_is(h,
                                "ea6f8df2849261d25cc5a0dac626c20d9f55a0f57692c44535b53177cc6c3851");
            }
            com_release(ps);
            if (!ok ||
                ((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 95))(o, 144, l, 4) <
                    0 ||
                !cs_light_matrix(item.world, l, cs_light)) {
                cs_fail("shadow shader or projection mismatch");
                return;
            }
            if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &cs_rt) < 0 ||
                !cs_rt || ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &cs_depth) < 0 ||
                !cs_depth ||
                ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 59))(o, 1, &cs_writer) < 0 ||
                !cs_writer) {
                cs_fail("writer state capture");
            }
        }
    } else {
        for (UINT i = 0; i < cs_player_count; i++) {
            if (cs_equal(&item, &cs_players[i])) {
                cs_drop(&item);
                return;
            }
        }
        for (UINT i = 0; i < cs_counts[cs_bank]; i++) {
            if (cs_equal(&item, &cs_items[cs_bank][i])) {
                cs_drop(&item);
                return;
            }
        }
        if (cs_counts[cs_bank] == CS_MAX) {
            cs_overflow = 1;
            cs_drop(&item);
            return;
        }
        if (((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 95))(o, 16, item.bones,
                                                                             128) < 0 ||
            !cs_finite(item.bones, 512)) {
            cs_skipped++;
            cs_drop(&item);
            return;
        }
        cs_items[cs_bank][cs_counts[cs_bank]++] = item;
    }
}

static void cs_flush(PVOID o) {
    if (!cpu_shadow_test || cpu_shadow_test == 2 || cs_failed || cs_device != o || !cs_writer ||
        cs_flushed) {
        return;
    }
    cs_flushed = 1;
    UINT old = cs_bank ^ 1;
    if (!cs_ready || cs_ready_frame + 1 != frame_count || !cs_counts[old]) {
        return;
    }
    PVOID rt = 0, ds = 0, save = 0;
    INT ok = ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &rt) >= 0 &&
             ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &ds) >= 0 && rt == cs_rt &&
             ds == cs_depth;
    com_release(rt);
    com_release(ds);
    if (!ok) {
        cs_fail("shadow target changed before replay");
        return;
    }
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 59))(o, 1, &save) < 0 || !save) {
        com_release(save);
        cs_fail("replay state capture");
        return;
    }
    PVOID *v = *(PVOID **)cs_writer;
    INT hr = ((INT(API *)(PVOID))v[5])(cs_writer);
    if (hr >= 0) {
        for (UINT j = 0; j < 16 && hr >= 0; j++) {
            hr = ((INT(API *)(PVOID, UINT, PVOID))original_method(o, 65))(o, j, 0);
        }
        for (UINT j = 0; j < cs_counts[old] && hr >= 0; j++) {
            CSItem *i = &cs_items[old][j];
            float light[16];
            cs_multiply(i->world, cs_light, light);
            coverage_scale(light);
            if (!cs_finite(light, 16)) {
                cs_skipped++;
                continue;
            }
#define CS_DO(expr)                                                                                \
    do {                                                                                           \
        hr = (expr);                                                                               \
        if (hr < 0)                                                                                \
            goto restore;                                                                          \
    } while (0)
            CS_DO(((INT(API *)(PVOID, PVOID))original_method(o, 87))(o, i->decl));
            CS_DO(((INT(API *)(PVOID, UINT, PVOID, UINT, UINT))original_method(o, 100))(
                o, 0, i->vb, i->offset, i->stride));
            CS_DO(((INT(API *)(PVOID, PVOID))original_method(o, 104))(o, i->ib));
            CS_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 94))(
                o, 16, i->bones, 128));
            CS_DO(((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 94))(o, 144,
                                                                                         light, 4));
            CS_DO(((INT(API *)(PVOID, UINT, INT, UINT, UINT, UINT, UINT))original_method(o, 82))(
                o, 4, i->base, i->minimum, i->vertices, i->start, i->primitives));
            cs_replayed++;
            if (i->role == 4) {
                cs_crowd_replayed++;
            }
#undef CS_DO
        }
    }
restore:
    v = *(PVOID **)save;
    INT restored = ((INT(API *)(PVOID))v[5])(save);
    com_release(save);
    if (hr < 0 || restored < 0) {
        cs_fail(restored < 0 ? "state restoration failed" : "replay API failure");
    }
}

static void cs_present(PVOID o, INT hr) {
    if (!cpu_shadow_test || cs_device != o) {
        return;
    }
    /* Summary every 600 frames, skipped while nothing is captured (menus, cutscenes). */
    ++cs_frames;
    INT active = cs_player_count || cs_counts[cs_bank] || cs_replayed || cs_skipped ||
                 cs_overflow || cs_failed;
    if (cs_frames == 1 || (cs_frames % 600 == 0 && active)) {
        log_begin("Actor shadow frame");
        log_field("playerDraws", cs_player_count);
        UINT crowds = 0;
        for (UINT i = 0; i < cs_counts[cs_bank]; i++) {
            if (cs_items[cs_bank][i].role == 4) {
                crowds++;
            }
        }
        log_field("capturedCPUdraws", cs_counts[cs_bank] - crowds);
        log_field("capturedCrowdDraws", crowds);
        log_field("replayedCrowdDraws", cs_crowd_replayed);
        log_field("crowdEnabled", crowd_shadow_test);
        log_field("replayedDraws", cs_replayed);
        log_field("skippedDraws", cs_skipped);
        log_field("overflow", cs_overflow);
        log_field("failed", cs_failed);
        log_field("poseDelayFrames", 1);
        log_field("mode", cpu_shadow_test);
        log_field("shaderReads", cs_shader_reads);
        log_field("shaderCacheHits", cs_shader_hits);
        log_field("shaderCacheEntries", cs_shader_count);
        log_emit();
    }
    cs_drop_bank(cs_bank ^ 1);
    if (hr < 0 || cs_failed || cs_overflow || !cs_writer) {
        cs_drop_bank(cs_bank);
        cs_ready = 0;
    } else {
        cs_ready = 1;
        cs_ready_frame = frame_count;
    }
    cs_bank ^= 1;
    cs_drop_frame();
    cs_overflow = cs_replayed = cs_skipped = cs_crowd_replayed = 0;
    cs_shader_reads = cs_shader_hits = 0;
}
