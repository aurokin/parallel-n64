/* Exercise the production shared TLBWrite directly. Unrelated interpreter
 * instructions are discarded with function/data sections by the test link. */
#include "../../mupen64plus-core/src/r4300/cached_interp.c"
/* Keep checks active in Release test builds too. */
#undef NDEBUG
#include <assert.h>

struct device g_dev;
uint32_t g_cp0_regs[CP0_REGS_COUNT];
unsigned int r4300emu;
static uint32_t test_pc = 0x80000000;
uint32_t *r4300_pc(void) { return &test_pc; }
static uint32_t ram[0x800000 / 4];
static struct precomp_block even, odd;

static void mapping(unsigned int virtual_addr, unsigned int physical_addr)
{
    g_cp0_regs[CP0_ENTRYHI_REG] = virtual_addr;
    g_cp0_regs[CP0_ENTRYLO0_REG] = ((physical_addr >> 12) << 6) | 7;
    g_cp0_regs[CP0_ENTRYLO1_REG] = (((physical_addr + 4096) >> 12) << 6) | 7;
    g_cp0_regs[CP0_PAGEMASK_REG] = 0;
    TLBWrite(0);
}

int main(void)
{
    g_dev.ri.rdram.dram = ram;
    for (unsigned int mode = CORE_INTERPRETER; mode <= CORE_DYNAREC; ++mode) {
        r4300emu = mode;
        memset(ram, 0, sizeof(ram));
        memset(invalid_code, 1, sizeof(invalid_code));
        for (unsigned int p = 1; p <= 6; ++p)
            invalid_code[0x80000 + p] = invalid_code[0xa0000 + p] = 0;
        memset(blocks, 0, sizeof(blocks));
        memset(&even, 0, sizeof(even));
        memset(&odd, 0, sizeof(odd));
        poweron_tlb();
        mapping(0x400000, 0x1000);
        blocks[0x400] = &even;
        blocks[0x401] = &odd;
        invalid_code[0x400] = invalid_code[0x401] = 0;
        mapping(0x400000, 0x1000);
        assert(!invalid_code[0x400] && !invalid_code[0x401]);
        assert(even.code_hash > UINT32_MAX && odd.code_hash > UINT32_MAX);

        /* A remap to identical physical bytes keeps both compiled pages. */
        mapping(0x400000, 0x3000);
        assert(!invalid_code[0x400] && !invalid_code[0x401]);
        /* The final byte of each page participates in change detection. */
        ((unsigned char *)ram)[0x1fff] = 1;
        ((unsigned char *)ram)[0x2fff] = 2;
        mapping(0x400000, 0x1000);
        assert(invalid_code[0x400] && invalid_code[0x401]);
        /* An already-invalid block loses its saved fingerprint. */
        mapping(0x600000, 0x5000);
        assert(even.code_hash == 0 && odd.code_hash == 0);

        /* A fingerprint differing only above bit 31 must reject reuse. */
        even.code_hash = XXH3_64bits((char *)ram + 0x1000, 4096) ^ (UINT64_C(1) << 40);
        odd.code_hash = XXH3_64bits((char *)ram + 0x2000, 4096);
        mapping(0x400000, 0x1000);
        assert(invalid_code[0x400] && !invalid_code[0x401]);
    }
    puts("TLB unchanged/changed/even/odd/full-width/invalid checks passed for cached interpreter and dynarec modes");
    return 0;
}
