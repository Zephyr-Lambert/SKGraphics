/* Shadow softening code must never read a temp register component before
 * writing it: the Windows D3D9 runtime rejects such shaders with
 * D3DERR_INVALIDCALL (DXVK accepts them). Builds the generated kernel with
 * synthetic inputs, so no game shader is needed. */
#define _start shadow_tests_unused
#include "shadow_mock.c"
#undef _start

static UINT written[8]; /* per temp register r0-r7: bit mask of written components */

/* Components a source token reads, given which result components are used. */
static UINT src_needs(UINT token, UINT dst_mask, UINT all) {
    UINT swizzle = (token >> 16) & 255, need = 0;
    for (UINT c = 0; c < 4; c++) {
        if (all || (dst_mask >> c & 1)) {
            need |= 1u << ((swizzle >> (c * 2)) & 3);
        }
    }
    return need;
}

static UINT is_temp(UINT token) {
    return (((token >> 28) & 7) | ((token >> 8) & 0x18)) == 0;
}

static UINT check(const UINT *code, UINT words) {
    for (UINT p = 0; p < words;) {
        UINT op = code[p] & 65535, len = (code[p] >> 24) & 15;
        if (!len || p + len >= words + 1) {
            return 200;
        }
        UINT dst = code[p + 1], mask = (dst >> 16) & 15;
        for (UINT s = 2; s <= len; s++) {
            UINT src = code[p + s];
            /* texld reads its whole coordinate; other ops the masked lanes. */
            if (is_temp(src)) {
                UINT reg = src & 2047, need = src_needs(src, mask, op == 66);
                if (reg >= 8 || (need & ~written[reg])) {
                    return 201 + op; /* uninitialized read */
                }
            }
        }
        if (is_temp(dst) && (dst & 2047) < 8) {
            written[dst & 2047] |= mask;
        }
        p += len + 1;
    }
    return 0;
}

void _start(void) {
    /* The game's shader has already written its own registers; only the
     * kernel's r4-r6 start empty. Coordinate source: v1.xyzw (input). */
    for (UINT filter = 1; filter <= 2; filter++) {
        for (UINT r = 0; r < 8; r++) {
            written[r] = r < 4 ? 15 : 0;
        }
        shadow_filter = filter;
        shadow_actual = 4096;
        shadow_filter_radius = 200;
        sf_used = 0;
        sf_kernel(0x90e40001, 0xa0000000);
        UINT result = check(sf_output, sf_used);
        if (result) {
            fail = result;
        }
        if (sf_used < 20 || sf_used >= 4096) {
            fail = 199;
        }
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
