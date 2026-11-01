/* Completed plugin tasks run inside a CPU register store. Their break IRQ
 * must wait for SP_INT, while software-raised and older IRQs stay pending. */
#undef NDEBUG
#include <assert.h>
#include <boolean.h>
#include <stdint.h>
#include <string.h>
#include "main/device.h"
#include "main/rom.h"
#include "plugin/plugin.h"
#include "r4300/cp0_private.h"
#include "r4300/exception.h"
#include "r4300/interrupt.h"
#include "r4300/r4300.h"
#include "r4300/tlb.h"

struct device g_dev;
rom_params ROM_PARAMS;
rsp_plugin_functions rsp;
uint32_t send_allist_to_hle_rsp;
uint32_t g_cp0_regs[CP0_REGS_COUNT];
struct precomp_instr *PC;
uint32_t skip_jump, next_interrupt, last_addr;
unsigned int dyna_interp, count_per_op = 2, r4300emu = CORE_INTERPRETER;
int stop, reset_hard_job, g_gs_vi_counter;
tlb tlb_e[32];

static struct precomp_instr current;
static unsigned exceptions;
static enum { COMPLETE, SOFTWARE_IRQ, HALT_ONLY, INCOMPLETE } task;

void DebugMessage(int level, const char *message, ...) { (void)level; (void)message; }
void cp0_update_count(void) {}
uint32_t *r4300_cp0_regs(void) { return g_cp0_regs; }
uint32_t *r4300_pc(void) { return &current.addr; }
void exception_general(void) { ++exceptions; g_cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_EXL; }
void generic_jump_to(uint32_t address) { current.addr = address; }
void dyna_jump(void) {}
void dyna_stop(void) {}
void init_blocks(void) {}
void free_blocks(void) {}
void reset_hard(void) {}
int retro_return(bool flipping) { (void)flipping; return 0; }
void pifbootrom_hle_execute(struct device *dev) { (void)dev; }
void vi_vertical_interrupt_event(struct vi_controller *vi) { (void)vi; }
void si_end_of_dma_event(struct si_controller *si) { (void)si; }
void pi_end_of_dma_event(struct pi_controller *pi) { (void)pi; }
void ai_end_of_dma_event(struct ai_controller *ai) { (void)ai; }
void rdp_interrupt_event(struct rdp_core *dp) { (void)dp; }
void protect_framebuffers(struct rdp_core *dp) { (void)dp; }
void unprotect_framebuffers(struct rdp_core *dp) { (void)dp; }
void new_frame(void) {}
unsigned int hleDoRspCycles(unsigned int cycles) { (void)cycles; assert(0); return 0; }

static unsigned int CALL run_task(unsigned int cycles)
{
    struct rsp_core *sp = &g_dev.sp;
    (void)cycles;
    /* Microcode may reuse the task descriptor in DMEM before returning. */
    sp->mem[0xfc0 / 4] = 0xdeadbeef;
    if (task == HALT_ONLY)
        sp->regs[SP_STATUS_REG] |= SP_STATUS_HALT;
    else if (task != INCOMPLETE)
        sp->regs[SP_STATUS_REG] |= SP_STATUS_HALT | SP_STATUS_BROKE | SP_STATUS_TASKDONE;
    if (task == SOFTWARE_IRQ || (task == COMPLETE && (sp->regs[SP_STATUS_REG] & SP_STATUS_INTR_BREAK)))
        g_dev.r4300.mi.regs[MI_INTR_REG] |= MI_INTR_SP;
    return 0;
}

static void reset(void)
{
    memset(&g_dev, 0, sizeof g_dev);
    memset(g_cp0_regs, 0, sizeof g_cp0_regs);
    g_cp0_regs[CP0_COUNT_REG] = 100;
    g_cp0_regs[CP0_STATUS_REG] = UINT32_C(0x0400); /* IM2 set, interrupts disabled. */
    current.addr = 0x80001000;
    PC = &current;
    exceptions = 0;
    stop = reset_hard_job = skip_jump = 0;
    init_interrupt();
    remove_event(SPECIAL_INT);
    init_rsp(&g_dev.sp, &g_dev.r4300, &g_dev.dp, &g_dev.ri, 0);
    poweron_rsp(&g_dev.sp);
    g_dev.sp.mem[0xfc0 / 4] = 2;
    g_dev.r4300.mi.regs[MI_INTR_MASK_REG] = MI_INTR_SP;
    rsp.doRspCycles = run_task;
    task = COMPLETE;
}

static void fire_completion(void)
{
    assert(get_next_event_type() == SP_INT);
    g_cp0_regs[CP0_COUNT_REG] = get_event(SP_INT);
    gen_interrupt();
    assert(get_event(SP_INT) == 0);
}

int main(void)
{
    reset();
    assert(!has_interrupt_event(PI_INT));
    add_interrupt_event_count(PI_INT, 0);
    assert(get_event(PI_INT) == 0 && has_interrupt_event(PI_INT));
    add_interrupt_event_count(PI_INT, 42);
    while (has_interrupt_event(PI_INT))
        remove_event(PI_INT);
    assert(!has_interrupt_event(PI_INT));

    reset();
    write_rsp_regs(&g_dev.sp, 0x10, 0x125, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == 0);
    assert((g_cp0_regs[CP0_CAUSE_REG] & CP0_CAUSE_IP2) == 0);
    assert(get_event(CHECK_INT) == 0 && get_event(SP_INT) == 4100);
    fire_completion();
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_SP);
    assert(g_cp0_regs[CP0_CAUSE_REG] & CP0_CAUSE_IP2);
    assert(exceptions == 0);
    g_cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_IE;
    check_interrupt();
    assert(get_next_event_type() == CHECK_INT);
    gen_interrupt();
    assert(exceptions == 1);

    /* Graphics keeps 1000 counts; audio latency must not apply to all types. */
    reset();
    g_dev.sp.mem[0xfc0 / 4] = 1;
    write_rsp_regs(&g_dev.sp, 0x10, 0x125, UINT32_MAX);
    assert(g_dev.sp.mem[0xfc0 / 4] == 0xdeadbeef);
    assert(get_event(SP_INT) == 1100);
    fire_completion();
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_SP);

    /* A microcode SET_INTR with INTR_BREAK clear is not replayed by SP_INT. */
    reset();
    task = SOFTWARE_IRQ;
    write_rsp_regs(&g_dev.sp, 0x10, 0x25, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_SP);
    assert(get_event(SP_INT) == 0);

    reset();
    g_dev.r4300.mi.regs[MI_INTR_REG] = MI_INTR_SP | MI_INTR_VI;
    g_cp0_regs[CP0_CAUSE_REG] |= CP0_CAUSE_IP2;
    write_rsp_regs(&g_dev.sp, 0x10, 0x125, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == (MI_INTR_SP | MI_INTR_VI));
    write_rsp_regs(&g_dev.sp, 0x10, 0x8, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_VI);
    assert((g_cp0_regs[CP0_CAUSE_REG] & CP0_CAUSE_IP2) == 0);
    g_cp0_regs[CP0_STATUS_REG] |= CP0_STATUS_IE;
    fire_completion();
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == (MI_INTR_SP | MI_INTR_VI));
    assert(exceptions == 1);

    /* Halting without BREAK must not replay an older acknowledged IRQ. */
    reset();
    task = HALT_ONLY;
    g_dev.r4300.mi.regs[MI_INTR_REG] = MI_INTR_SP;
    write_rsp_regs(&g_dev.sp, 0x10, 0x125, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_SP);
    assert(get_event(SP_INT) == 0);
    write_rsp_regs(&g_dev.sp, 0x10, 0x8, UINT32_MAX);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == 0);

    reset();
    task = INCOMPLETE;
    g_dev.r4300.mi.regs[MI_INTR_REG] = MI_INTR_VI;
    write_rsp_regs(&g_dev.sp, 0x10, 0x125, UINT32_MAX);
    assert(g_dev.sp.rsp_task_locked == 1);
    assert(g_dev.r4300.mi.regs[MI_INTR_REG] == MI_INTR_VI);
    assert(get_event(SP_INT) == 1100);
    return 0;
}
