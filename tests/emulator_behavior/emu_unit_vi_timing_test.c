/* Exercise the controller's event period and scanline readback together. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/interrupt.h"
#include "r4300/mi_controller.h"
#include "vi/vi_controller.h"

int g_vi_refresh_rate;
unsigned alternate_vi_timing, g_vi_clock_timing;
static uint32_t cp0[32], scheduled_count, interrupt_bits;
static int scheduled_type;

static void CALL noop(void) {}
gfx_plugin_functions gfx = {
    .updateScreen = noop, .viStatusChanged = noop, .viWidthChanged = noop
};
void new_vi(void) {}
void cp0_update_count(void) {}
uint32_t *r4300_cp0_regs(void) { return cp0; }
void add_interrupt_event_count(int type, unsigned int count)
{
    scheduled_type = type;
    scheduled_count = count;
}
void raise_rcp_interrupt(struct r4300_core *r4300, uint32_t bits)
{
    (void)r4300;
    interrupt_bits |= bits;
}
void clear_rcp_interrupt(struct r4300_core *r4300, uint32_t bits)
{
    (void)r4300;
    interrupt_bits &= ~bits;
}

static void expect(const char *what, uint32_t actual, uint32_t expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: got %u, expected %u\n", what,
                (unsigned)actual, (unsigned)expected);
        exit(EXIT_FAILURE);
    }
}

static void check_timing(unsigned clock, unsigned refresh, unsigned vsync,
                         unsigned clock_timing, int override, unsigned cps, unsigned period)
{
    struct vi_controller vi = {0};
    uint32_t current;
    memset(cp0, 0, sizeof cp0);
    scheduled_count = interrupt_bits = 0;
    g_vi_clock_timing = clock_timing;
    g_vi_refresh_rate = override;
    init_vi(&vi, clock, refresh, NULL);
    poweron_vi(&vi);
    vi.regs[VI_V_SYNC_REG] = vsync;
    vi_vertical_interrupt_event(&vi);
    expect("field period", vi.delay, period);
    expect("next VI", vi.next_vi, 5000 + period);
    expect("scheduled event", scheduled_type, VI_INT);
    expect("scheduled count", scheduled_count, vi.next_vi);
    expect("raised VI interrupt", interrupt_bits, MI_INTR_VI);

    cp0[CP0_COUNT_REG] = vi.next_vi - vi.delay + 262*cps;
    read_vi_regs(&vi, 4*VI_CURRENT_REG, &current);
    expect("scanline uses the event clock", current, 262);
    cp0[CP0_COUNT_REG] = vi.next_vi - 1;
    read_vi_regs(&vi, 4*VI_CURRENT_REG, &current);
    expect("last scanline", current, vsync & ~1u);
}

static void fallback_reads(void)
{
    struct vi_controller vi = {0};
    uint32_t current;
    unsigned i;
    static const uint32_t vsync[] = {0, UINT32_MAX, 0x00ffffff};
    g_vi_clock_timing = 1;
    g_vi_refresh_rate = 0;
    init_vi(&vi, 48681812, 60, NULL);
    vi.delay = vi.next_vi = 1000000;
    cp0[CP0_COUNT_REG] = 20*1500;
    for (i = 0; i < sizeof vsync / sizeof vsync[0]; ++i) {
        vi.regs[VI_V_SYNC_REG] = vsync[i];
        read_vi_regs(&vi, 4*VI_CURRENT_REG, &current);
        expect("unconfigured/invalid timing read", current, 20);
    }
    poweron_vi(&vi);
    vi_vertical_interrupt_event(&vi);
    expect("unconfigured field fallback", vi.delay, 500000);
}

int main(void)
{
    /* Exotica programs 525 briefly, then 524 during boot. */
    check_timing(48681812, 60, 525, 1, 0, 1542, 811092);
    check_timing(48681812, 60, 524, 1, 0, 1545, 811125);
    check_timing(48681812, 60, 524, 1, 1542, 1542, 809550);
    check_timing(49656530, 50, 625, 1, 0, 1586, 992836);
    check_timing(48681812, 60, 525, 1, 1500, 1500, 789000);
    check_timing(48681812, 60, 525, 1, 1600, 1600, 841600);
    check_timing(48681812, 60, 525, 1, 2200, 2200, 1157200);
    check_timing(0, 0, 525, 1, 0, 1500, 789000);
    check_timing(0, 60, 525, 1, 0, 1500, 789000);
    /* Ordinary ROMs keep their exact fixed-period path at either V_SYNC. */
    check_timing(48681812, 60, 525, 0, 1500, 1500, 789000);
    check_timing(48681812, 60, 524, 0, 1500, 1500, 787500);
    fallback_reads();
    return EXIT_SUCCESS;
}
