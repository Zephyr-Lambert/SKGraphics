#define _start unused_texture_entry
#include "texture_mock.c"
#undef _start

static void identity(float *w) {
    for (UINT i = 0; i < 16; i++) {
        w[i] = i % 5 == 0 ? 1 : 0;
    }
    w[12] = 100;
    w[14] = 200;
}

void _start(void) {
    float w[16];
    identity(w);
    cs_bank = 0;
    cs_counts[0] = 1;
    identity(cs_items[0][0].world);
    cs_items[0][0].role = 1;
    if (circle_owner(w) != 1) {
        fail = 81;
    }
    cs_items[0][0].world[13] = 350;
    if (circle_owner(w) != 1) {
        fail = 82;
    }
    w[12] += 1;
    if (circle_owner(w)) {
        fail = 83;
    }
    w[12] -= 1;
    cs_items[0][0].role = 4;
    if (circle_owner(w) != 4) {
        fail = 84;
    }
    cs_counts[0] = 2;
    identity(cs_items[0][1].world);
    cs_items[0][1].role = 1;
    if (circle_owner(w)) {
        fail = 85;
    }
    cs_player_count = 1;
    identity(cs_players[0].world);
    if (circle_owner(w) != 2) {
        fail = 86;
    }
    cs_counts[0] = 0;
    if (circle_owner(w) != 2) {
        fail = 87;
    }
    cs_player_count = 0;
    if (circle_owner(w)) {
        fail = 88;
    }

    union {
        UINT u;
        float f;
    } nan;

    nan.u = 0x7fc00000;
    w[13] = nan.f;
    if (circle_world_equal(w, w)) {
        fail = 89;
    }
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
