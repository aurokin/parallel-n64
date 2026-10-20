/* A pending interrupt survives a synchronous exception and is delivered as an
 * interrupt afterwards.
 *
 * A Compare interrupt that fires while EXL is set stays pending in Cause.IP7
 * until the handler returns. If that handler's ERET jumps to an unmapped page,
 * the dynamic recompiler and cached interpreter take the TLB refill inside
 * ERET, before ERET checks for interrupts. The refill exception used to
 * overwrite Cause and lose IP7, so libultra's timer interrupt never arrived and
 * Conker's Bad Fur Day deadlocked under the dynamic recompiler. */

#include "main/device.h"
#include "r4300/cp0_private.h"
#include "r4300/exception.h"
#include "r4300/interrupt.h"
#include "r4300/r4300.h"
#include "r4300/r4300_core.h"
#include "r4300/recomp.h"
#include "r4300/tlb.h"

#include <boolean.h>
#include <stdio.h>
#include <stdlib.h>

/* State and helpers exception.c and interrupt.c link against. */
struct device g_dev;
uint32_t g_cp0_regs[CP0_REGS_COUNT];
struct precomp_instr *PC;
uint32_t skip_jump;
unsigned int dyna_interp;
unsigned int r4300emu = CORE_PURE_INTERPRETER;
uint32_t next_interrupt;
uint32_t last_addr;
unsigned int count_per_op = 2;
int stop;
int reset_hard_job;
int g_gs_vi_counter;
tlb tlb_e[32];

static struct precomp_instr current_instr;
static uint32_t jumped_to;

void generic_jump_to(uint32_t address)
{
   jumped_to = address;
   current_instr.addr = address;
}

void cp0_update_count(void) {}
void dyna_jump(void) {}
void dyna_stop(void) {}
void init_blocks(void) {}
void free_blocks(void) {}
void reset_hard(void) {}
int retro_return(bool just_flipping) { (void)just_flipping; return 0; }
void DebugMessage(int level, const char *message, ...) { (void)level; (void)message; }
void pifbootrom_hle_execute(struct device *dev) { (void)dev; }
void vi_vertical_interrupt_event(struct vi_controller *vi) { (void)vi; }
void si_end_of_dma_event(struct si_controller *si) { (void)si; }
void pi_end_of_dma_event(struct pi_controller *pi) { (void)pi; }
void ai_end_of_dma_event(struct ai_controller *ai) { (void)ai; }
void rsp_interrupt_event(struct rsp_core *sp) { (void)sp; }
void rdp_interrupt_event(struct rdp_core *dp) { (void)dp; }

static int failures;

static void expect_eq(const char *what, uint32_t actual, uint32_t expected)
{
   if (actual != expected)
   {
      fprintf(stderr, "%s: expected 0x%08x, got 0x%08x\n", what, expected, actual);
      failures++;
   }
}

static void reset_cpu(uint32_t status, uint32_t cause, uint32_t pc)
{
   size_t i;

   for (i = 0; i < CP0_REGS_COUNT; i++)
      g_cp0_regs[i] = 0;
   for (i = 0; i < 32; i++)
   {
      tlb_e[i].start_even = tlb_e[i].end_even = 0;
      tlb_e[i].start_odd = tlb_e[i].end_odd = 0;
   }
   g_cp0_regs[CP0_STATUS_REG] = status;
   g_cp0_regs[CP0_CAUSE_REG] = cause;
   g_cp0_regs[CP0_COUNT_REG] = UINT32_C(0x4e26aed4);
   g_dev.r4300.delay_slot = 0;
   g_dev.r4300.mi.regs[MI_INTR_REG] = 0;
   g_dev.r4300.mi.regs[MI_INTR_MASK_REG] = 0;
   current_instr.addr = pc;
   PC = &current_instr;
   jumped_to = 0;
   init_interrupt();
}

int main(void)
{
   const uint32_t im7_enabled = UINT32_C(0x00008001); /* IM7 | IE */

   /* The Conker sequence: Compare fired during a handler (IP7 pending), the
    * handler's ERET cleared EXL and its jump to an unmapped page took the
    * refill exception before any interrupt check. */
   reset_cpu(im7_enabled, CP0_CAUSE_IP7, UINT32_C(0x1502c380));
   TLB_refill_exception(UINT32_C(0x1502c380), 2);
   expect_eq("refill: cause", g_cp0_regs[CP0_CAUSE_REG],
         CP0_CAUSE_IP7 | CP0_CAUSE_EXCCODE_TLBL);
   expect_eq("refill: vector", jumped_to, UINT32_C(0x80000000));
   expect_eq("refill: epc", g_cp0_regs[CP0_EPC_REG], UINT32_C(0x1502c380));

   /* The refill handler returns: as in ERET, clear EXL and check interrupts.
    * The timer interrupt is taken, as an interrupt, at the faulting address. */
   g_cp0_regs[CP0_STATUS_REG] &= ~CP0_STATUS_EXL;
   current_instr.addr = UINT32_C(0x1502c380);
   check_interrupt();
   gen_interrupt();
   expect_eq("delivery: cause", g_cp0_regs[CP0_CAUSE_REG],
         CP0_CAUSE_IP7 | CP0_CAUSE_EXCCODE_INT);
   expect_eq("delivery: vector", jumped_to, UINT32_C(0x80000180));
   expect_eq("delivery: epc", g_cp0_regs[CP0_EPC_REG], UINT32_C(0x1502c380));

   /* A store refill nested in a handler keeps the RCP and timer lines and
    * replaces the previous exception code. */
   reset_cpu(UINT32_C(0x0000ff03), CP0_CAUSE_IP7 | CP0_CAUSE_IP2 | CP0_CAUSE_EXCCODE_SYS,
         UINT32_C(0x10007000));
   TLB_refill_exception(UINT32_C(0x10007000), 1);
   expect_eq("nested store refill: cause", g_cp0_regs[CP0_CAUSE_REG],
         CP0_CAUSE_IP7 | CP0_CAUSE_IP2 | CP0_CAUSE_EXCCODE_TLBS);
   expect_eq("nested store refill: vector", jumped_to, UINT32_C(0x80000180));

   if (failures)
      return EXIT_FAILURE;
   puts("cp0 exception cause: ok");
   return EXIT_SUCCESS;
}
