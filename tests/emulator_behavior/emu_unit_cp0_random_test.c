/* Random stays within the TLB for guest and savestate Wired values. */
#include "r4300/cp0.h"
#undef NDEBUG
#include <assert.h>

int main(void)
{
    uint32_t regs[CP0_REGS_COUNT] = {0};
    const uint32_t counts[] = {0, 1, 2, 63, 64, 100, 0x7fffffff, 0xffffffff};
    const uint32_t bad_wired[] = {32, 33, 63, 64, 0x80000000, 0xffffffff};
    unsigned int w, i;
    for (w = 0; w < 32; ++w)
        for (i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i)
        {
            regs[CP0_WIRED_REG] = w;
            regs[CP0_COUNT_REG] = counts[i];
            uint32_t index = cp0_random_index(regs);
            assert(index >= w && index < 32);
            assert(index == (counts[i] / 2) % (32 - w) + w);
        }
    for (w = 0; w < sizeof(bad_wired) / sizeof(bad_wired[0]); ++w)
        for (i = 0; i < sizeof(counts) / sizeof(counts[0]); ++i)
        {
            regs[CP0_WIRED_REG] = bad_wired[w];
            regs[CP0_COUNT_REG] = counts[i];
            assert(cp0_random_index(regs) == 31);
        }
    return 0;
}
