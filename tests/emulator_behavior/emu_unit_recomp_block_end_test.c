/* A branch at the decode cutoff needs both FIN_BLOCK entries: its not-taken
 * path skips the delay slot and lands on the second one. Exercise the real
 * shared decoder with the same extended page layout used by both backends. */
#undef NDEBUG
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "main/device.h"
#include "r4300/r4300.h"
#include "r4300/cp0_private.h"
#include "r4300/cp1_private.h"
#include "r4300/recomp.h"

struct device g_dev;
struct precomp_block *blocks[0x100000];
char invalid_code[0x100000];
int64_t reg[32];
unsigned int r4300emu = CORE_INTERPRETER;
cpu_instruction_table current_instruction_table;
uint32_t g_cp0_regs[CP0_REGS_COUNT];

static void nop(void) {}
static void branch(void) {}
static void finish(void) {}
static void uncompiled(void) {}

void DebugMessage(int level, const char *message, ...) { (void)level; (void)message; }
uint32_t virtual_to_physical_address(struct r4300_core *cpu, uint32_t address, int write)
{ (void)cpu; (void)address; (void)write; assert(0); return 0; }

int main(void)
{
    enum { LENGTH = 1024, CAPACITY = 1281, CUTOFF = 1278 };
    uint32_t source[CAPACITY + 1] = {0};
    struct precomp_block block = {0};
    struct precomp_instr *decoded = calloc(CAPACITY + 1, sizeof(*decoded));
    assert(decoded != NULL);
    block.start = 0x80090000;
    block.end = 0x80091000;
    block.block = decoded;
    /* An impossible opcode pointer in the guard entry detects an overrun. */
    decoded[CAPACITY].ops = uncompiled;
    current_instruction_table.NOP = nop;
    current_instruction_table.BNE = branch;
    current_instruction_table.BNE_OUT = branch;
    current_instruction_table.BNE_IDLE = branch;
    current_instruction_table.FIN_BLOCK = finish;
    current_instruction_table.NOTCOMPILED = uncompiled;
    source[CUTOFF] = 0x1400ffff; /* BNE zero, zero, self: not taken. */

    recompile_block(source, &block, block.start);
    assert(decoded[CUTOFF].ops == branch);
    assert(decoded[CUTOFF + 1].ops == finish);
    assert(decoded[CUTOFF + 1].addr == 0x800913fc);
    assert(decoded[CUTOFF + 2].ops == finish);
    assert(decoded[CUTOFF + 2].addr == 0x80091400);
    assert(decoded[CAPACITY].ops == uncompiled);
    free(decoded);
    return 0;
}
