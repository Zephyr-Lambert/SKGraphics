/* Scale only the light clip X/Y components, leaving camera projection, depth
 * and W unchanged. Both known receiver VSs use c144..147 just like the writer.
 * Temporary changes are restored before CPU pose capture or any later draw.
 * Original shader bytecode is not modified or bundled.
 */
typedef struct {
    float original[16];
    UINT changed;
} CoverageDraw;

static UINT coverage_failed, coverage_writers, coverage_receivers;

static void coverage_scale(float *m) {
    if (!shadow_coverage || coverage_failed) {
        return;
    }
    float factor = 1.0f / (float)(shadow_coverage + 1);
    for (UINT r = 0; r < 4; r++) {
        m[r * 4] *= factor;
        m[r * 4 + 1] *= factor;
    }
}

static void coverage_fail(const char *why) {
    if (!coverage_failed) {
        log_begin("Shadow coverage disabled until reset");
        log_text(" reason=");
        log_text(why);
        log_emit();
    }
    coverage_failed = 1;
}

static void coverage_end(PVOID o, CoverageDraw *c) {
    if (c->changed) {
        if (((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 94))(
                o, 144, c->original, 4) < 0) {
            coverage_fail("matrix restoration failed");
        }
        c->changed = 0;
    }
}

static void coverage_begin(PVOID o, CoverageDraw *c) {
    c->changed = 0;
    DeviceState *t = device_state(o);
    if (!shadow_coverage || coverage_failed || !shadow_image || !t || t->aa_busy) {
        return;
    }
    if (cs_device && cs_device != o) {
        return;
    }
    cs_device = o;
    UINT role = cs_role(o);
    if (!((role == 2 && t->shadow_pass) || (role == 3 && !t->shadow_pass))) {
        return;
    }
    if (((INT(API *)(PVOID, UINT, float *, UINT))original_method(o, 95))(o, 144, c->original, 4) <
            0 ||
        !cs_finite(c->original, 16)) {
        coverage_fail("light matrix unavailable");
        return;
    }
    float adjusted[16];
    for (UINT i = 0; i < 16; i++) {
        adjusted[i] = c->original[i];
    }
    coverage_scale(adjusted);
    c->changed = 1;
    if (((INT(API *)(PVOID, UINT, const float *, UINT))original_method(o, 94))(o, 144, adjusted,
                                                                               4) < 0) {
        coverage_fail("light matrix update failed");
        coverage_end(o, c);
        return;
    }
    UINT *count = role == 2 ? &coverage_writers : &coverage_receivers;
    if (++*count == 1) {
        log_begin(role == 2 ? "Shadow coverage writer active" : "Shadow coverage receiver active");
        log_field("setting", shadow_coverage);
        log_field("linearMultiplier", shadow_coverage + 1);
        log_emit();
    }
}

static void coverage_reset(void) {
    coverage_failed = coverage_writers = coverage_receivers = 0;
}
