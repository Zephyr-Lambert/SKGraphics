/* Transform only two SHA-256-identified GOG 01.06 shadow receiver programs.
 * No original game shaders are bundled. CTAB comments are omitted in output.
 * Generated code uses r4-r6 and local DEF c220/c221, unused by both originals.
 * Shader creation failure returns the already-created original to the game.
 */
static UINT sf_input[4096], sf_original[4096], sf_output[4096], sf_used, sf_matches, sf_inspected,
    sf_read_failures;

static UINT sf_float(float v) {
    union {
        float f;
        UINT u;
    } x;

    x.f = v;
    return x.u;
}

static UINT sf_src(UINT type, UINT reg, UINT swizzle) {
    return 0x80000000u | (type << 28) | (swizzle << 16) | reg;
}

static UINT sf_dst(UINT reg, UINT mask) {
    return 0x80000000u | (mask << 16) | reg;
}

static void sf_word(UINT v) {
    if (sf_used < 4096) {
        sf_output[sf_used++] = v;
    }
}

static void sf_op2(UINT op, UINT d, UINT a) {
    sf_word(0x02000000 | op);
    sf_word(d);
    sf_word(a);
}

static void sf_op3(UINT op, UINT d, UINT a, UINT b) {
    sf_word(0x03000000 | op);
    sf_word(d);
    sf_word(a);
    sf_word(b);
}

static void sf_op4(UINT op, UINT d, UINT a, UINT b, UINT c) {
    sf_word(0x04000000 | op);
    sf_word(d);
    sf_word(a);
    sf_word(b);
    sf_word(c);
}

static void sf_def(UINT reg, float a, float b, float c, float d) {
    sf_word(0x05000051);
    sf_word(0xa00f0000 | reg);
    sf_word(sf_float(a));
    sf_word(sf_float(b));
    sf_word(sf_float(c));
    sf_word(sf_float(d));
}

static UINT sf_offset(INT v) {
    return !v ? sf_src(2, 221, 0)
              : sf_src(2, 220,
                       v == 1    ? 0
                       : v == -1 ? 0x55
                       : v == 2  ? 0xaa
                                 : 0xff);
}

static void sf_kernel(UINT coord, UINT bias) {
    UINT radius = shadow_filter == 2 ? 2 : 1;
    /* Keep the game's ADD(sample,bias), MAD(z,-1/w,...) arithmetic order. */
    sf_op2(6, sf_dst(4, 1), 0x90ff0002);
    sf_op2(1, sf_dst(4, 2), sf_src(2, 221, 0));
    /* texld reads r5.xyzw but only x/y are computed below. The Windows D3D9
     * runtime rejects shaders that read uninitialized components (DXVK does
     * not), so zero z/w once. */
    sf_op2(1, sf_dst(5, 12), sf_src(2, 221, 0));
    UINT cx = (coord >> 16) & 3, cy = (coord >> 18) & 3;
    UINT base = coord & ~0x00ff0000u;
    for (INT y = -(INT)radius; y <= (INT)radius; y++) {
        for (INT x = -(INT)radius; x <= (INT)radius; x++) {
            sf_op3(2, sf_dst(5, 1), base | ((cx * 0x55) << 16), sf_offset(x));
            sf_op3(2, sf_dst(5, 2), base | ((cy * 0x55) << 16), sf_offset(y));
            sf_op3(66, sf_dst(6, 15), sf_src(0, 5, 0xe4), 0xa0e40805);
            sf_op3(2, sf_dst(6, 1), sf_src(0, 6, 0), bias);
            sf_op4(4, sf_dst(6, 1), 0x90aa0002, sf_src(0, 4, 0) | 0x01000000, sf_src(0, 6, 0));
            sf_op4(88, sf_dst(6, 1), sf_src(0, 6, 0), sf_src(2, 221, 0), sf_src(2, 221, 0x55));
            sf_op4(4, sf_dst(4, 2), sf_src(0, 6, 0), sf_src(2, 221, 0xaa), sf_src(0, 4, 0x55));
        }
    }
}

static UINT sf_transform(const UINT *data, UINT bytes) {
    if (bytes < 8 || bytes > sizeof(sf_input) || bytes % 4 || data[0] != 0xffff0300) {
        return 0;
    }
    UINT count = bytes / 4, n = 0, p = 1;
    INT ended = 0;
    while (p < count) {
        UINT token = data[p], op = token & 65535,
             len = op == 65534 ? (token >> 16) & 32767 : (token >> 24) & 15;
        if (len > count - p - 1) {
            return 0;
        }
        if (op != 65534) {
            for (UINT j = 0; j <= len; j++) {
                sf_original[n++] = data[p + j];
            }
        }
        p += len + 1;
        if (op == 65535) {
            ended = 1;
            break;
        }
    }
    if (!ended || p != count) {
        return 0;
    }
    char hash[65];
    texture_hash(sf_original, n * 4, hash);
    static const char *ids[] = {"988bf8de871bc62d67527054f57ea5504eafa438aea3fa421770ffdf2e1bef71",
                                "508f45621d6197458320def0fb50f1b794d57153dfdb99230f28dbf8fa0a14ac"};
    INT variant = -1;
    for (UINT k = 0; k < 2; k++) {
        UINT j = 0;
        while (j < 64 && hash[j] == ids[k][j]) {
            j++;
        }
        if (j == 64) {
            variant = (INT)k;
        }
    }
    if (variant < 0) {
        return 0;
    }
    sf_used = 0;
    sf_word(data[0]);
    UINT radius = shadow_filter == 2 ? 2 : 1;
    float step = (float)shadow_filter_radius / (100.0f * (float)radius * (float)shadow_actual);
    sf_def(220, step, -step, 2.0f * step, -2.0f * step);
    sf_def(221, 0.0f, 1.0f, 1.0f / (float)((radius * 2 + 1) * (radius * 2 + 1)), 0.0f);
    UINT replacements = 0;
    for (p = 0; p < n;) {
        UINT t = sf_original[p], op = t & 65535, len = (t >> 24) & 15;
        if (op == 66 && sf_original[p + 3] == 0xa0e40805) {
            sf_kernel(sf_original[p + 2], variant ? 0xa0aa0000 : 0xa0000000);
            if (!variant) {
                sf_op3(5, 0x80110000, sf_src(0, 4, 0x55), 0x81000000);
                p += 21;
            } else {
                p += 13;
            }
            replacements++;
            continue;
        }
        if (variant && op == 88 && sf_original[p + 1] == 0x80010000) {
            sf_op3(5, 0x80110000, sf_src(0, 4, 0x55), 0x81550000);
            p += 5;
            replacements++;
            continue;
        }
        if (op == 88 && sf_original[p + 1] == 0x80070000) {
            /* Blend lit and shadow color by coverage: lit + r0.x * (shadow - lit).
             * Written as ADD + MAD, not LRP: the Windows D3D9 runtime rejected
             * the LRP form (its destination overlaps the coverage source).
             * Alpha/fog remain game-owned. */
            UINT shadow = sf_original[p + 4], lit = sf_original[p + 3];
            if ((lit | shadow) & 0x0f000000) {
                return 0; /* source modifiers are not expected here */
            }
            sf_op3(2, sf_dst(6, 7), shadow, lit | 0x01000000);
            sf_op4(4, 0x80070000, 0x80000000, sf_src(0, 6, 0xe4), lit);
            p += 5;
            replacements++;
            continue;
        }
        for (UINT j = 0; j <= len; j++) {
            sf_word(sf_original[p + j]);
        }
        p += len + 1;
    }
    return replacements == (variant ? 3u : 2u) ? sf_used : 0;
}

static INT API filtered_pixel_shader(PVOID o, const UINT *code, PVOID *out) {
    INT hr = ((INT(API *)(PVOID, const UINT *, PVOID *))original_method(o, 106))(o, code, out);
    if (hr < 0 || !out || !*out || !shadow_filter || !shadow_image) {
        return hr;
    }
    UINT bytes = 0;
    PVOID *v = *(PVOID **)*out;
    /* GetFunction need not update the capacity argument on a data-copy call.
  * Query the exact length first, as the working shadow probe already does. */
    INT read = ((INT(API *)(PVOID, PVOID, UINT *))v[4])(*out, 0, &bytes);
    UINT capacity = bytes;
    if (read >= 0 && bytes >= 8 && bytes <= sizeof(sf_input) && !(bytes % 4)) {
        read = ((INT(API *)(PVOID, PVOID, UINT *))v[4])(*out, sf_input, &capacity);
    } else {
        if (read >= 0) {
            read = (INT)0x80070057;
        }
    }
    if (read < 0 || capacity != bytes) {
        if (sf_read_failures++ < 8) {
            log_begin("Shadow filter shader read failed");
            log_field("bytes", bytes);
            log_field("returnedSize", capacity);
            log_hex_field("result", read);
            log_emit();
        }
        return hr;
    }
    sf_inspected++;
    if (!sf_transform(sf_input, bytes)) {
        if (sf_inspected <= 8 || bytes == 712 || bytes == 828) {
            char hash[65];
            texture_hash(sf_input, bytes, hash);
            log_begin("Shadow filter shader unmatched");
            log_field("bytes", bytes);
            log_text(" sha256=");
            log_text(hash);
            log_emit();
        }
        return hr;
    }
    PVOID filtered = 0;
    INT result = ((INT(API *)(PVOID, const UINT *, PVOID *))original_method(o, 106))(o, sf_output,
                                                                                     &filtered);
    if (result >= 0 && filtered) {
        com_release(*out);
        *out = filtered;
        sf_matches++;
    } else {
        com_release(filtered);
        /* Rejected by the driver: log both programs (twice per session at
         * most) so the transformed code can be checked offline. */
        static UINT dumps;
        if (dumps < 2) {
            dumps++;
            for (UINT which = 0; which < 2; which++) {
                const UINT *words = which ? sf_output : sf_input;
                UINT total = which ? sf_used : bytes / 4;
                for (UINT i = 0; i < total; i += 24) {
                    log_begin(which ? "Shadow filter rejected code"
                                    : "Shadow filter original code");
                    log_field("word", i);
                    for (UINT j = i; j < total && j < i + 24; j++) {
                        log_text(" ");
                        log_hex_number(words[j]);
                    }
                    log_emit();
                }
            }
        }
    }
    log_begin("Shadow filter shader");
    log_field("applied", result >= 0 && filtered != 0);
    log_field("quality", shadow_filter);
    log_field("radiusHundredths", shadow_filter_radius);
    log_field("mapSize", shadow_actual);
    log_field("words", sf_used);
    log_field("matchedCount", sf_matches);
    log_hex_field("result", result);
    log_emit();
    return hr;
}
