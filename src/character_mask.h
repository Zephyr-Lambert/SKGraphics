/* Replay depth-writing scene geometry into an AO receiver mask.
 * The original draw has already completed. EQUAL tests the same depth;
 * neither depth nor stencil is written by the replay. Later scenery draws
 * replace earlier character coverage, avoiding cutouts through occluders.
 */
typedef struct {
    PVOID rt, ps;
    Viewport vp;
    UINT rs[7];
    INT changed;
} CharacterMaskState;

static const UINT cm_states[] = {14, 23, 27, 168, 194, 59, 174};

static void cm_fail(DeviceState *t) {
    if (!t->cm_failed) {
        log_begin("AO receiver mask unavailable: AO skipped until reset");
        log_emit();
    }
    t->cm_failed = 1;
}

static void cm_end(PVOID o, CharacterMaskState *m, INT draw_result) {
    DeviceState *t = device_state(o);
    INT bad = draw_result < 0;
    if (m->changed) {
        if (((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, m->rt) < 0) {
            bad = 1;
        }
        if (((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &m->vp) < 0) {
            bad = 1;
        }
        if (((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, m->ps) < 0) {
            bad = 1;
        }
        for (UINT i = 0; i < 7; i++) {
            if (aa_rs(o, cm_states[i], m->rs[i]) < 0) {
                bad = 1;
            }
        }
    }
    com_release(m->rt);
    com_release(m->ps);
    m->rt = m->ps = 0;
    if (bad && t) {
        cm_fail(t);
    }
}

static INT cm_begin(PVOID o, CharacterMaskState *m) {
    DeviceState *t = device_state(o);
    if (!ao_enabled || !(ao_exclude_characters || ao_exclude_backgrounds) || ao_depth_view ||
        character_preview || !t || t->cm_failed || t->ao_failed || t->bl_done || !t->bl_scene ||
        !t->main || t->shadow_pass) {
        return 0;
    }
    UINT z = 0, zw = 0, stencil = 0;
    if (((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, 7, &z) < 0 ||
        ((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, 14, &zw) < 0) {
        cm_fail(t);
        return 0;
    }
    if (!z || !zw) {
        return 0;
    }
    if (((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, 52, &stencil) < 0 || stencil) {
        cm_fail(t);
        return 0;
    }
    *m = (CharacterMaskState){0};
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &m->rt) < 0 ||
        m->rt != t->bl_scene) {
        com_release(m->rt);
        return 0;
    }
    for (UINT i = 1; i < 4; i++) {
        PVOID other = 0;
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, i, &other);
        if (other) {
            com_release(other);
            cm_end(o, m, -1);
            return 0;
        }
    }
    PVOID ds = 0;
    ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &ds);
    INT same = ds && ds == t->ao_depth;
    com_release(ds);
    if (!same) {
        cm_end(o, m, 0);
        return 0;
    }
    if (((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &m->vp) < 0 ||
        ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &m->ps) < 0 || !m->ps) {
        cm_end(o, m, -1);
        return 0;
    }
    for (UINT i = 0; i < 7; i++) {
        if (((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, cm_states[i], &m->rs[i]) <
            0) {
            cm_end(o, m, -1);
            return 0;
        }
    }
    PVOID vs = 0;
    UINT role = 0, background = 0;
    if (((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &vs) >= 0 && vs) {
        for (UINT i = 0; i < bl_id_count; i++) {
            if (bl_ids[i].object == vs) {
                role = bl_ids[i].character;
                background = bl_ids[i].background;
                break;
            }
        }
    }
    com_release(vs);
    INT excluded = (ao_exclude_characters && role == 1) || tb_receiver(o, background);
    PVOID shader = character_variant(o, m->ps, excluded ? 3 : 4);
    if (!shader) {
        cm_end(o, m, -1);
        return 0;
    }
    if (!t->cm_tex || t->cm_w != t->rw || t->cm_h != t->rh) {
        com_release(t->cm_surf);
        com_release(t->cm_tex);
        t->cm_surf = t->cm_tex = 0;
        t->cm_ready = 0;
        if (!aa_target(o, t->rw, t->rh, 21, &t->cm_tex, &t->cm_surf)) {
            cm_end(o, m, -1);
            return 0;
        }
        t->cm_w = t->rw;
        t->cm_h = t->rh;
        log_begin("AO receiver mask allocated");
        log_field("width", t->cm_w);
        log_field("height", t->cm_h);
        log_emit();
    }
    m->changed = 1;
#define CM_DO(expr)                                                                                \
    do {                                                                                           \
        if ((expr) < 0) {                                                                          \
            cm_end(o, m, -1);                                                                      \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)
    CM_DO(((INT(API *)(PVOID, UINT, PVOID))original_method(o, 37))(o, 0, t->cm_surf));
    if (!t->cm_ready) {
        Viewport full = {0, 0, t->rw, t->rh, 0, 1};
        CM_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &full));
        CM_DO(aa_rs(o, 174, 0));
        CM_DO(((INT(API *)(PVOID, UINT, PVOID, UINT, UINT, float, UINT))original_method(o, 43))(
            o, 0, 0, 1, 0, 1, 0));
        t->cm_ready = 1;
    }
    CM_DO(((INT(API *)(PVOID, const Viewport *))original_method(o, 47))(o, &m->vp));
    CM_DO(aa_rs(o, 174, m->rs[6]));
    CM_DO(aa_rs(o, 14, 0));
    CM_DO(aa_rs(o, 23, 3));
    CM_DO(aa_rs(o, 27, 0));
    CM_DO(aa_rs(o, 168, 7));
    CM_DO(aa_rs(o, 194, 0));
    CM_DO(aa_rs(o, 59, 0));
    CM_DO(((INT(API *)(PVOID, PVOID))original_method(o, 107))(o, shader));
    if (++t->cm_draws == 1) {
        log_begin("AO exclusion mask active");
        log_emit();
    }
#undef CM_DO
    return 1;
}
