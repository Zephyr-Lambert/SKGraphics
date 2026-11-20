/* Read-only one-frame pass capture for locating the pre-HUD bloom boundary.
 * No resource creation, shader replacement, render-state changes or retained
 * COM references. Three manually triggered frames, bounded output per frame.
 */
static PVOID scene_log, scene_device;
static short(API *scene_key)(INT);
static UINT scene_initialized, scene_down, scene_active, scene_captures, scene_lines, scene_draws;
static UINT scene_code[4096];
static UINT ao_constants[56], ao_constants_valid;
static UINT character_role(const char *hash);

static UINT scene_cap(void) {
    return actor_shadow_probe ? 16000 : 4000;
}

static void scene_emit(void) {
    if (!scene_log || scene_lines >= scene_cap()) {
        return;
    }
    UINT written;
    log_text("\r\n");
    WriteFile(scene_log, log_line, log_used, &written, 0);
    scene_lines++;
}

static void scene_begin(const char *name) {
    log_begin(name);
    log_field("capture", scene_captures);
    log_field("sequence", scene_lines);
}

static INT scene_recording(PVOID o) {
    return scene_probe_enabled && scene_active && o == scene_device &&
           scene_lines < scene_cap() - 2;
}

static void scene_shader(PVOID shader, char hash[65]) {
    hash[0] = '?';
    hash[1] = 0;
    if (!shader) {
        hash[0] = '0';
        return;
    }
    UINT bytes = 0;
    PVOID *v = *(PVOID **)shader;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, 0, &bytes) < 0 || bytes < 8 ||
        bytes > sizeof(scene_code) || bytes % 4) {
        return;
    }
    UINT size = bytes;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, scene_code, &size) < 0 || size != bytes) {
        return;
    }
    texture_hash(scene_code, bytes, hash);
}

static void scene_surface(PVOID surf, const char *name) {
    log_hex_field(name, (UINT)(unsigned long)surf);
    if (!surf) {
        return;
    }
    SurfaceDesc d = {0};
    PVOID *v = *(PVOID **)surf;
    if (((INT(API *)(PVOID, SurfaceDesc *))v[12])(surf, &d) >= 0) {
        log_field("w", d.w);
        log_field("h", d.h);
        log_field("fmt", d.format);
    }
}

/* Read-only AO investigation. Hex values preserve exact IEEE-754 float bits.
 * CTAB identifies c0..3=world, c4..7=local, c8..11=WVP, c149..150=eye.
 * Constants are evidence, not necessarily meaningful for every shader.
 */
static void scene_ao(PVOID o, PVOID depth, PVOID vs, const Viewport *vp) {
    if (!ao_probe_enabled || scene_lines >= scene_cap() - 2) {
        return;
    }
    PVOID tex = 0;
    SurfaceDesc d = {0};
    INT desc_hr = -1, container_hr = -1;
    if (depth) {
        PVOID *v = *(PVOID **)depth;
        desc_hr = ((INT(API *)(PVOID, SurfaceDesc *))v[12])(depth, &d);
        container_hr =
            ((INT(API *)(PVOID, const void *, PVOID *))v[11])(depth, probe_texture_iid, &tex);
    }

    union {
        float f;
        UINT u;
    } zmin, zmax;

    zmin.f = vp->minz;
    zmax.f = vp->maxz;
    scene_begin("AOProbe depth");
    log_field("draw", scene_draws);
    log_hex_field("surface", (UINT)(unsigned long)depth);
    log_hex_field("texture", (UINT)(unsigned long)tex);
    log_hex_field("descResult", desc_hr);
    log_hex_field("containerResult", container_hr);
    log_field("width", d.w);
    log_field("height", d.h);
    log_field("format", d.format);
    log_field("multisample", d.ms);
    log_hex_field("minZbits", zmin.u);
    log_hex_field("maxZbits", zmax.u);
    scene_emit();
    com_release(tex);
    if (!vs || scene_lines >= scene_cap() - 2) {
        return;
    }

    union {
        float f[56];
        UINT u[56];
    } values;

    PVOID get = original_method(o, 95);
    if (!get) {
        return;
    }
    INT hr = ((INT(API *)(PVOID, UINT, float *, UINT))get)(o, 0, values.f, 12);
    INT eye = ((INT(API *)(PVOID, UINT, float *, UINT))get)(o, 149, values.f + 48, 2);
    if (hr < 0 || eye < 0) {
        scene_begin("AOProbe constants unavailable");
        log_field("draw", scene_draws);
        log_hex_field("result", hr);
        log_hex_field("eyeResult", eye);
        scene_emit();
        ao_constants_valid = 0;
        return;
    }
    UINT changed = !ao_constants_valid;
    for (UINT i = 0; i < 56; i++) {
        if (values.u[i] != ao_constants[i]) {
            changed = 1;
        }
    }
    if (!changed) {
        return;
    }
    for (UINT group = 0; group < 4; group++) {
        if (scene_lines >= scene_cap() - 2) {
            break;
        }
        UINT start = group * 16, count = group == 3 ? 8 : 16;
        scene_begin("AOProbe constants");
        log_field("draw", scene_draws);
        log_field("firstRegister", group == 3 ? 149 : group * 4);
        log_text(" floatBits=");
        for (UINT i = 0; i < count; i++) {
            if (i) {
                log_text(",");
            }
            log_hex_number(values.u[start + i]);
        }
        scene_emit();
    }
    for (UINT i = 0; i < 56; i++) {
        ao_constants[i] = values.u[i];
    }
    ao_constants_valid = 1;
}

/* Diagnostic reads only. Buffer identities are meaningful within one run.
 * No buffer locks, game-memory walks or retained COM references. */
static void scene_geometry(PVOID o, UINT slot, INT base, UINT minimum, UINT vertices, UINT start,
                           UINT stride) {
    if (!actor_shadow_probe || !scene_recording(o)) {
        return;
    }
    scene_begin("ActorShadow geometry");
    log_field("draw", scene_draws);
    log_field("slot", slot);
    log_hex_field("baseVertexBits", (UINT)base);
    log_field("minVertex", minimum);
    log_field("vertexCount", vertices);
    log_field("startIndexOrVertex", start);
    log_field("upStride", stride);
    PVOID ib = 0, decl = 0;
    PVOID get = original_method(o, 105);
    INT hi = get ? ((INT(API *)(PVOID, PVOID *))get)(o, &ib) : -1;
    get = original_method(o, 88);
    INT hd = get ? ((INT(API *)(PVOID, PVOID *))get)(o, &decl) : -1;
    log_hex_field("indices", (UINT)(unsigned long)ib);
    log_hex_field("indicesResult", hi);
    log_hex_field("declaration", (UINT)(unsigned long)decl);
    log_hex_field("declarationResult", hd);
    scene_emit();
    com_release(ib);
    com_release(decl);
    if (slot == 83 || slot == 84) {
        return; /* UP draw data is supplied directly, not bound. */
    }
    for (UINT j = 0; j < 4 && scene_lines < scene_cap() - 2; j++) {
        PVOID vb = 0;
        UINT offset = 0, step = 0;
        get = original_method(o, 101);
        INT hr =
            get ? ((INT(API *)(PVOID, UINT, PVOID *, UINT *, UINT *))get)(o, j, &vb, &offset, &step)
                : -1;
        if (vb || hr < 0) {
            scene_begin("ActorShadow stream");
            log_field("draw", scene_draws);
            log_field("stream", j);
            log_hex_field("buffer", (UINT)(unsigned long)vb);
            log_field("offset", offset);
            log_field("stride", step);
            log_hex_field("result", hr);
            scene_emit();
        }
        com_release(vb);
    }
}

static void scene_actor(PVOID o, PVOID vs, const char *vh) {
    if (!actor_shadow_probe || !scene_recording(o)) {
        return;
    }
    DeviceState *t = device_state(o);
    scene_begin("ActorShadow role");
    log_field("draw", scene_draws);
    log_field("shadowWriter", t ? t->shadow_pass : 0);
    log_field("mainTarget", t ? t->main : 0);
    log_field("actorShaderClass", character_role(vh));
    scene_emit();
    if (!vs) {
        return;
    }

    union {
        float f[1024];
        UINT u[1024];
    } c;

    PVOID get = original_method(o, 95);
    INT hr = get ? ((INT(API *)(PVOID, UINT, float *, UINT))get)(o, 0, c.f, 256) : -1;
    if (hr < 0) {
        scene_begin("ActorShadow constants unavailable");
        log_field("draw", scene_draws);
        log_hex_field("result", hr);
        scene_emit();
        return;
    }
    /* CTAB of the captured shadow writer: bones c16..143, light WVP c144..147.
  * Hash separately: the old c12..255 fingerprint conflated unrelated state.
  * Capture every draw for comparison, not just already-known actor shaders. */
    char hash[65];
    texture_hash(c.u + 64, 128 * 4 * sizeof(UINT), hash);
    scene_begin("ActorShadow bones");
    log_field("draw", scene_draws);
    log_text(" c16to143=");
    log_text(hash);
    scene_emit();
    for (UINT group = 0; group < 5 && scene_lines < scene_cap() - 2; group++) {
        UINT first = group == 4 ? 144 : group * 4;
        scene_begin("ActorShadow transform");
        log_field("draw", scene_draws);
        log_field("firstRegister", first);
        log_text(" floatBits=");
        for (UINT i = 0; i < 16; i++) {
            if (i) {
                log_text(",");
            }
            log_hex_number(c.u[first * 4 + i]);
        }
        scene_emit();
    }
}

/* Bounded shader evidence from the captured frame; do not let startup shaders
 * consume the budget. Each code chunk fits the shared line buffer. */
static char actor_shader_hashes[32][65];
static UINT actor_shader_count;

static void scene_actor_shader(PVOID shader, UINT vertex, const char *hash) {
    if (!actor_shadow_probe || !shader || actor_shader_count >= (background_probe ? 32 : 8) ||
        scene_lines >= scene_cap() - 2) {
        return;
    }
    if (hash[0] == '0' || hash[0] == '?') {
        return;
    }
    for (UINT i = 0; i < actor_shader_count; i++) {
        UINT j = 0;
        while (j < 64 && actor_shader_hashes[i][j] == hash[j]) {
            j++;
        }
        if (j == 64) {
            return;
        }
    }
    UINT bytes = 0;
    PVOID *v = *(PVOID **)shader;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, 0, &bytes) < 0 || bytes < 8 ||
        bytes > sizeof(scene_code) || bytes % 4) {
        return;
    }
    UINT size = bytes;
    if (((INT(API *)(PVOID, PVOID, UINT *))v[4])(shader, scene_code, &size) < 0 || size != bytes) {
        return;
    }
    for (UINT i = 0; i < 65; i++) {
        actor_shader_hashes[actor_shader_count][i] = hash[i];
    }
    UINT id = ++actor_shader_count;
    scene_begin("ActorShadow shader");
    log_field("id", id);
    log_field("vertex", vertex);
    log_field("bytes", bytes);
    log_text(" hash=");
    log_text(hash);
    scene_emit();
    for (UINT i = 0; i < bytes / 4 && scene_lines < scene_cap() - 2; i += 24) {
        scene_begin("ActorShadow code");
        log_field("id", id);
        log_field("word", i);
        for (UINT j = i; j < bytes / 4 && j < i + 24; j++) {
            log_text(" ");
            log_hex_number(scene_code[j]);
        }
        scene_emit();
    }
}

/* F8-only background evidence, never a runtime exclusion rule.
 * Retain up to32 texture references only until the captured frame finishes. */
static PVOID bg_textures[32];
static UINT bg_count, bg_bytes, bg_limit_reported;

static void bg_clear(void) {
    for (UINT i = 0; i < bg_count; i++) {
        com_release(bg_textures[i]);
    }
    bg_count = bg_bytes = bg_limit_reported = 0;
}

static void bg_capture(PVOID o, const char *vh) {
    DeviceState *t = device_state(o);
    if (!background_probe || !scene_recording(o) || !t || !t->main || t->shadow_pass ||
        character_role(vh) == 1) {
        return;
    }
    PVOID tex = 0;
    if (((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, 0, &tex) < 0 || !tex) {
        com_release(tex);
        return;
    }
    for (UINT i = 0; i < bg_count; i++) {
        if (bg_textures[i] == tex) {
            scene_begin("BackgroundProbe texture reference");
            log_field("draw", scene_draws);
            log_field("id", i + 1);
            scene_emit();
            com_release(tex);
            return;
        }
    }
    if (bg_count == 32) {
        if (!bg_limit_reported++) {
            scene_begin("BackgroundProbe texture limit");
            scene_emit();
        }
        com_release(tex);
        return;
    }
    UINT id = ++bg_count;
    bg_textures[id - 1] = tex;
    PVOID *v = *(PVOID **)tex, buffer = 0;
    SurfaceDesc d = {0};
    UINT bytes = 0;
    char hash[65] = {0};
    INT hr = -1;
    if (((UINT(API *)(PVOID))v[10])(tex) == 3) {
        hr = ((INT(API *)(PVOID, UINT, SurfaceDesc *))v[17])(tex, 0, &d);
    }
    if (hr >= 0 && (!d.w || !d.h || d.w > 2048 || d.h > 2048 || bg_bytes >= 32 * 1048576)) {
        hr = -1;
    }
    if (hr >= 0) {
        PVOID lib = d3dx_library();
        TextureSaveFn save =
            (TextureSaveFn)(lib ? GetProcAddress(lib, "D3DXSaveTextureToFileInMemory") : 0);
        hr = save ? save(&buffer, 4, tex, 0) : -1;
        if (hr >= 0 && buffer) {
            PVOID *b = *(PVOID **)buffer;
            bytes = ((UINT(API *)(PVOID))b[4])(buffer);
            PVOID data = ((PVOID(API *)(PVOID))b[3])(buffer);
            if (!data || !bytes || bytes > 24 * 1048576 || bytes > 32 * 1048576 - bg_bytes) {
                hr = -1;
            } else {
                texture_hash(data, bytes, hash);
                bg_bytes += bytes;
            }
        } else if (hr >= 0) {
            hr = -1;
        }
    }
    scene_begin("BackgroundProbe texture");
    log_field("draw", scene_draws);
    log_field("id", id);
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
}

static void scene_draw(PVOID o, UINT slot, UINT type, UINT primitives, PVOID caller) {
    if (!scene_recording(o)) {
        return;
    }
    PVOID rt = 0, depth = 0, ps = 0, vs = 0, container = 0;
    Viewport vp = {0};
    char ph[65], vh[65];
    ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &rt);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &depth);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 108))(o, &ps);
    ((INT(API *)(PVOID, PVOID *))original_method(o, 93))(o, &vs);
    scene_shader(ps, ph);
    scene_shader(vs, vh);
    ((INT(API *)(PVOID, Viewport *))original_method(o, 48))(o, &vp);
    scene_begin("SceneProbe draw");
    log_field("draw", ++scene_draws);
    log_field("slot", slot);
    log_field("type", type);
    log_field("primitives", primitives);
    log_hex_field("callerRVA", (UINT)((unsigned long)caller - (unsigned long)GetModuleHandleW(0)));
    scene_surface(rt, "rt");
    log_hex_field("depth", (UINT)(unsigned long)depth);
    if (rt) {
        PVOID *v = *(PVOID **)rt;
        ((INT(API *)(PVOID, const void *, PVOID *))v[11])(rt, probe_texture_iid, &container);
    }
    log_hex_field("rtTexture", (UINT)(unsigned long)container);
    log_field("vpX", vp.x);
    log_field("vpY", vp.y);
    log_field("vpW", vp.w);
    log_field("vpH", vp.h);
    log_text(" ps=");
    log_text(ph);
    log_text(" vs=");
    log_text(vh);
    const UINT rs[] = {7, 14, 15, 27, 19, 20, 171, 168};
    const char *names[] = {"z", "zwrite", "atest", "blend", "src", "dst", "blendop", "writeMask"};
    for (UINT j = 0; j < 8; j++) {
        UINT val = 0;
        INT hr = ((INT(API *)(PVOID, UINT, UINT *))original_method(o, 58))(o, rs[j], &val);
        log_field(names[j], hr >= 0 ? val : 0xffffffff);
    }
    for (UINT j = 0; j < 8; j++) {
        PVOID tex = 0;
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 64))(o, j, &tex);
        log_text(" t");
        log_number(j);
        log_text("=");
        log_hex_number((UINT)(unsigned long)tex);
        com_release(tex);
    }
    scene_emit();
    scene_actor(o, vs, vh);
    bg_capture(o, vh);
    DeviceState *actor_state = device_state(o);
    if (actor_shadow_probe && (background_probe || (actor_state && actor_state->shadow_pass) ||
                               character_role(vh) == 1)) {
        scene_actor_shader(vs, 1, vh);
        scene_actor_shader(ps, 0, ph);
    }
    scene_ao(o, depth, vs, &vp);
    com_release(container);
    com_release(rt);
    com_release(depth);
    com_release(ps);
    com_release(vs);
}

static void scene_finish(const char *reason) {
    if (!scene_active) {
        bg_clear();
        return;
    }
    bg_clear();
    scene_begin("SceneProbe end");
    log_field("draws", scene_draws);
    log_field("truncated", scene_lines >= scene_cap() - 2);
    log_text(" reason=");
    log_text(reason);
    scene_emit();
    scene_active = 0;
    scene_device = 0;
}

static void scene_present(PVOID o) {
    if (!scene_probe_enabled) {
        return;
    }
    if (scene_active && o == scene_device) {
        scene_finish("Present-before-AA");
    }
    if (!scene_initialized) {
        scene_initialized = 1;
        static const WCHAR dll[] = {'u', 's', 'e', 'r', '3', '2', '.', 'd', 'l', 'l', 0};
        PVOID lib = LoadLibraryW(dll);
        if (lib) {
            scene_key = (short(API *)(INT))GetProcAddress(lib, "GetAsyncKeyState");
        }
        log_begin(scene_key
                      ? "SceneProbe ready: press F8 during gameplay (three captures per launch)"
                      : "SceneProbe unavailable: GetAsyncKeyState missing");
        log_emit();
    }
    if (!scene_key) {
        return;
    }
    UINT down = (((UINT)scene_key(0x77)) & 0x8000) != 0;
    if (down && !scene_down && scene_captures < 3) {
        if (!scene_log) {
            WCHAR file[1024];
            UINT k = GetModuleFileNameW(self_module, file, 1024);
            if (k && k < 980) {
                while (k && file[k - 1] != '\\' && file[k - 1] != '/') {
                    k--;
                }
                const char *name = "SKGraphics_SceneProbe.log";
                while (*name) {
                    file[k++] = (WCHAR)*name++;
                }
                file[k] = 0;
                scene_log = CreateFileW(file, 0x40000000, 1, 0, 2, 0x80, 0);
                if (scene_log == (PVOID)-1) {
                    scene_log = 0;
                }
            }
        }
        if (scene_log) {
            scene_captures++;
            actor_shader_count = 0;
            scene_lines = 0;
            scene_draws = 0;
            ao_constants_valid = 0;
            scene_active = 1;
            scene_device = o;
            scene_begin("SceneProbe start");
            log_text(" version=2.0 raw-shader-SHA256");
            log_field("aoProbe", ao_probe_enabled);
            log_field("actorShadowProbe", actor_shadow_probe);
            scene_emit();
        } else {
            log_begin("SceneProbe ERROR: cannot create capture log");
            log_emit();
        }
    }
    scene_down = down;
}

static INT API scene_beginscene(PVOID o) {
    INT hr = ((INT(API *)(PVOID))original_method(o, 41))(o);
    if (scene_recording(o)) {
        scene_begin("SceneProbe BeginScene");
        log_hex_field("result", hr);
        scene_emit();
    }
    return hr;
}

static INT API scene_endscene(PVOID o) {
    INT hr = ((INT(API *)(PVOID))original_method(o, 42))(o);
    if (scene_recording(o)) {
        scene_begin("SceneProbe EndScene");
        log_hex_field("result", hr);
        scene_emit();
    }
    return hr;
}

static INT API scene_clear(PVOID o, UINT count, const Rect *rects, UINT flags, UINT color, float z,
                           UINT stencil) {
    INT hr = ((INT(API *)(PVOID, UINT, const Rect *, UINT, UINT, float, UINT))original_method(
        o, 43))(o, count, rects, flags, color, z, stencil);
    if (scene_recording(o)) {
        PVOID rt = 0;
        ((INT(API *)(PVOID, UINT, PVOID *))original_method(o, 38))(o, 0, &rt);
        scene_begin("SceneProbe Clear");
        scene_surface(rt, "rt");
        log_field("rects", count);
        log_hex_field("flags", flags);
        log_hex_field("color", color);
        log_hex_field("result", hr);
        scene_emit();
        com_release(rt);
        if (ao_probe_enabled && scene_lines < scene_cap() - 2) {
            PVOID ds = 0;
            ((INT(API *)(PVOID, PVOID *))original_method(o, 40))(o, &ds);

            union {
                float f;
                UINT u;
            } bits;

            bits.f = z;
            scene_begin("AOProbe clear");
            scene_surface(ds, "depth");
            log_hex_field("flags", flags);
            log_hex_field("zBits", bits.u);
            log_field("stencil", stencil);
            log_hex_field("result", hr);
            scene_emit();
            com_release(ds);
        }
    }
    return hr;
}
