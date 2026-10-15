/* Exercise the real COPS packet path without ROMs or disk images. */
#include <vars.h>
#undef NDEBUG
#include <assert.h>

static void packet(int dx, int dy)
{
    assert(via1_ira(0) == 0);
    int actual_x = (int8)via1_ira(0);
    int actual_y = (int8)via1_ira(0);
    if (actual_x != dx || actual_y != dy)
        fprintf(stderr, "Expected motion %d,%d, got %d,%d\n", dx, dy, actual_x, actual_y);
    assert(actual_x == dx && actual_y == dy);
}

int main(void)
{
    set_mouse_relative(1);
    cops_mouse = 10000;
    copsqueuelen = 0;
    via[1].via[DDRA] = 0xff;

    /* Accumulate host events; seeking must not damp or overwrite them. */
    add_mouse_delta(40, -20);
    add_mouse_delta(8, -4);
    seek_mouse_event();
    assert(mousequeuelen == 0);
    assert(lisa_rb_Oxdc00_cops_via1(IFR1) & VIA_IRQ_BIT_CA1);
    packet(48, -24);
    assert(!(lisa_rb_Oxdc00_cops_via1(IFR1) & VIA_IRQ_BIT_CA1));
    packet(0, 0);

    /* Preserve remainders and a packet already being read by the guest. */
    add_mouse_delta(300, -260);
    assert(via1_ira(0) == 0);
    add_mouse_delta(5, 5);
    set_mouse_button(1);
    assert((int8)via1_ira(0) == 127);
    assert((int8)via1_ira(0) == -127);
    assert(via1_ira(0) == 0x86);
    packet(127, -127);
    packet(51, -1);
    packet(0, 0);
    set_mouse_button(0);
    assert(via1_ira(0) == 0x06);

    /* Sub-pixel-target sized movements still arrive, in both directions. */
    add_mouse_delta(-1, 1);
    packet(-1, 1);
    add_mouse_delta(90, -90);
    add_mouse_delta(-90, 90);
    packet(0, 0);

    /* Disabled/reset hardware must not replay stale movement later. */
    add_mouse_delta(400, 400);
    via1_ora(0x70, 0);
    add_mouse_delta(100, 100);
    via1_ora(0x7c, 0);
    packet(0, 0);
    add_mouse_delta(400, 400);
    cops_reset();
    assert(mouse_pending_x == 0 && mouse_pending_y == 0);
    copsqueuelen = 0; /* Discard reset identification bytes. */
    packet(0, 0);

    /* Relative mode ignores absolute targets; desktop defaults remain usable. */
    add_mouse_event(100, 100, 0);
    assert(mousequeuelen == 0);
    set_mouse_relative(0);
    add_mouse_event(100, 100, 0);
    assert(mousequeuelen == 1);
    puts("COPS relative mouse tests passed");
    return 0;
}
