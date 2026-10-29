/* Exercise instruction boundaries in both the predecoded and portable loops. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vu/add.c"
#include "vu/divide.c"
#include "vu/logical.c"
#include "vu/multiply.c"
#include "vu/select.c"
#include "vu/vu.c"
#include "su.c"

RSP_INFO RSP_INFO_NAME;
static ALIGNED u32 sp_memory[0x2000 / 4];
static ALIGNED u32 dram_memory[16];
static u32 cp0[16], pc, mi;
static unsigned messages;

void message(const char *body) { (void)body; ++messages; }

static void expect(const char *what, u32 actual, u32 expected)
{
    if (actual != expected) {
        fprintf(stderr, "%s: got %08x, expected %08x\n", what,
                (unsigned)actual, (unsigned)expected);
        exit(1);
    }
}

static void reset(void)
{
    unsigned i;
    memset(sp_memory, 0, sizeof sp_memory);
    memset(dram_memory, 0, sizeof dram_memory);
    memset(cp0, 0, sizeof cp0);
    memset(SR, 0, sizeof SR);
    memset(VR, 0, sizeof VR);
    memset(VACC, 0, sizeof VACC);
    memset(MFC0_count, 0, sizeof MFC0_count);
    memset(conf, 0, sizeof conf);
    for (i = 0; i < 16; ++i)
        CR[i] = &cp0[i];
    GET_RSP_INFO(SP_PC_REG) = &pc;
    GET_RSP_INFO(SP_STATUS_REG) = &cp0[4];
    GET_RSP_INFO(SP_RD_LEN_REG) = &cp0[2];
    GET_RSP_INFO(SP_DMA_BUSY_REG) = &cp0[6];
    GET_RSP_INFO(MI_INTR_REG) = &mi;
    DMEM = (pu8)sp_memory;
    IMEM = DMEM + 0x1000;
    DRAM = (pu8)dram_memory;
    pc = mi = messages = 0;
}

static void instruction(unsigned address, u32 word)
{
    sp_memory[(0x1000 + address) / 4] = word;
}

int main(void)
{
    reset();
    instruction(0, 0x34010007);  /* ORI r1, zero, 7 */
    instruction(4, 0x24220003);  /* ADDIU r2, r1, 3 */
    instruction(8, 0x00221821);  /* ADDU r3, r1, r2 */
    instruction(12, 0xac030000); /* SW r3, 0(zero) */
    instruction(16, 0x8c040000); /* LW r4, 0(zero) */
    instruction(20, 0x0000000d); /* BREAK */
    run_task();
    expect("scalar dispatch", SR[4], 17);
    expect("BREAK PC", pc, 0x04001018);
    expect("BREAK status", cp0[4], SP_STATUS_BROKE | SP_STATUS_HALT);

    reset();
    instruction(0, 0x10000003); /* BEQ zero, zero, 16 */
    instruction(4, 0x34010007);
    instruction(8, 0x34010009);
    instruction(16, 0x0000000d);
    run_task();
    expect("taken branch delay slot", SR[1], 7);
    expect("branch BREAK PC", pc, 0x04001014);

    /* CXD4 executes the first target as the second jump's delay slot. */
    reset();
    instruction(0, 0x08000004); /* J 16 */
    instruction(4, 0x08000008); /* J 32 */
    instruction(16, 0x3402000b);
    instruction(20, 0x3402000d);
    instruction(32, 0x0000000d);
    run_task();
    expect("branch in delay slot", SR[2], 11);
    expect("nested branch PC", pc, 0x04001024);

    reset();
    pc = 0xffc;
    instruction(0xffc, 0x34030005);
    instruction(0, 0x0000000d);
    run_task();
    expect("IMEM wrap", SR[3], 5);
    expect("wrapped PC", pc, 0x04001004);

    reset();
    instruction(0, 0x48000000 | (24u << 21) | (2u << 16) |
                   (3u << 11) | (4u << 6) | 0x33); /* VMOV v4[3], v2[0] */
    instruction(4, 0x0000000d);
    VR[2][0] = 123;
    VR[4][2] = 456;
    run_task();
    expect("helper reads inst_word", VR[4][3], 123);
    expect("partial vector write", VR[4][2], 456);
    expect("published final instruction", inst_word, 0x0000000d);

    pc = cp0[4] = 0;
    VR[2][0] = 321;
    run_task();
    expect("same instruction reads current register", VR[4][3], 321);
    pc = cp0[4] = 0;
    instruction(0, 0x48000000 | (25u << 21) | (3u << 16) |
                   (5u << 11) | (6u << 6) | 0x33); /* VMOV v6[5], v3[1] */
    VR[3][1] = 654;
    run_task();
    expect("rewritten vector operands and selector", VR[6][5], 654);
    expect("previous destination preserved", VR[4][3], 321);

    /* A plugin-side DMA in a delay slot replaces the target before fetching it. */
    reset();
    cp0[0] = 0x1010;
    SR[1] = 7;
    dram_memory[0] = 0x3405002a;
    dram_memory[1] = 0x0000000d;
    instruction(0, 0x08000004);
    instruction(4, 0x40811000); /* MTC0 r1, SP_RD_LEN */
    instruction(16, 0x3405000d);
    instruction(20, 0x0000000d);
    run_task();
    expect("DMA replaces target instruction", SR[5], 42);

    /* Reuse the same PC after a direct IMEM write between tasks. */
    pc = cp0[4] = 0;
    instruction(0, 0x34050063);
    instruction(4, 0x0000000d);
    run_task();
    expect("direct IMEM write", SR[5], 99);

    reset();
    MF_SP_STATUS_TIMEOUT = 2;
    instruction(0, 0x40012000); /* MFC0 r1, SP_STATUS */
    instruction(4, 0x40012000);
    instruction(8, 0x34020001);
    run_task();
    expect("COP0 timeout yield", cp0[4], SP_STATUS_HALT);
    expect("yield PC", pc, 0x04001008);
    expect("no instruction after yield", SR[2], 0);

    reset();
    instruction(0, 0xfc000000); /* Reserved primary opcode. */
    instruction(4, 0x0000000d);
    run_task();
    expect("reserved opcode reports once", messages, 1);
    return 0;
}
