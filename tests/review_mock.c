#define _start old_mock_entry
#include "mock.c"
#undef _start
static UINT queries;

static INT API counted_desc(PVOID o, SurfaceDesc *d) {
    (void)o;
    (void)d;
    queries++;
    return 0;
}

void _start(void) {
    d3dx_module = 0;
    library_loads = 0;
    if (!d3dx_library() || !d3dx_library() || library_loads != 1) {
        fail = 57;
    }
    HookTable t = {0};
    PVOID slots[4] = {0};
    t.vt = slots;
    t.count = 4;
    slots[1] = (PVOID)11;
    protect_calls = 0;
    protect_fail_at = 2;
    hook_install(&t, 1, (PVOID)22);
    if (slots[1] != (PVOID)11 || t.hooked[0] || protect_calls != 3) {
        fail = 50;
    }
    protect_calls = 0;
    protect_fail_at = 0;
    hook_install(&t, 1, (PVOID)22);
    if (slots[1] != (PVOID)22 || t.hooked[0] != 2) {
        fail = 51;
    }
    hook_install(&t, 1, (PVOID)33);
    if (slots[1] != (PVOID)22 || protect_calls != 2) {
        fail = 52;
    }
    sv[12] = counted_desc;
    log_file = 0;
    log_signature_count = 0;
    log_surface("test", 0, 0, &sobj, 0);
    draw_sample(&dobj, 0);
    shadow_draw_sample(&dobj, 0);
    if (queries || log_signature_is_new(1, 2, 3, 4, 5, 6, 7, 8) || log_signature_count) {
        fail = 53;
    }
    log_file = (PVOID)3;
    log_events = log_limit;
    log_surface("test", 0, 0, &sobj, 0);
    if (queries || log_signature_is_new(1, 2, 3, 4, 5, 6, 7, 8)) {
        fail = 54;
    }
    if (main_surface(0, &sobj)) {
        fail = 55;
    }
    log_used = 0;
    log_text(0);
    if (log_used) {
        fail = 56;
    }
    path_set_suffix(32768, "x");
    __asm__ volatile("syscall" ::"a"(60), "D"((unsigned long)fail) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
