/* Exercise both sample handover paths, including malformed savestate FIFOs. */
#include "ai/ai_controller.h"
#include "ri/ri_controller.h"
#include "vi/vi_controller.h"
#include "r4300/r4300_core.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>

static uint32_t cp0_regs[CP0_REGS_COUNT];
static unsigned int event_count;
void cp0_update_count(void) {}
uint32_t* r4300_cp0_regs(void) { return cp0_regs; }
unsigned int get_event(int type) { (void)type; return event_count; }
void add_interrupt_event(int type, unsigned int delay) { (void)type; event_count = delay; }
void raise_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; (void)intr; }
void clear_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; (void)intr; }

static uint32_t ram[64];
static const unsigned char* handed;
static size_t handed_size;
static unsigned int calls;
static void push(void* opaque, const void* data, size_t size)
{
    (void)opaque;
    handed = data;
    handed_size = size;
    calls++;
    assert(handed >= (unsigned char*)ram);
    assert(handed + size <= (unsigned char*)(ram + 64));
    assert(size != 0 && size % 4 == 0);
    /* The real backend writes a whole four-byte frame at each iteration. */
    memset((void*)data, 0x5a, size);
}

static void check_span(struct ai_controller* ai, uint32_t addr, uint32_t length,
        uint32_t unread, size_t expected, int poll)
{
    uint32_t remaining;
    ai->fifo[0].address = addr;
    ai->fifo[0].length = length;
    ai->fifo[0].duration = 0;
    ai->last_read = unread;
    calls = 0;
    if (poll)
        read_ai_regs(ai, AI_LEN_REG * 4, &remaining);
    else
        ai_end_of_dma_event(ai);
    assert(calls == (expected != 0));
    if (expected)
    {
        assert(handed_size == expected);
        assert(handed == (unsigned char*)ram + (addr & ~7u) + (uint32_t)(length - unread));
    }
}

int main(void)
{
    struct ai_controller ai = {0};
    struct ri_controller ri = {0};
    struct vi_controller vi = {0};
    ri.rdram.dram = ram;
    ri.rdram.dram_size = sizeof(ram);
    vi.clock = 48681812;
    vi.delay = 811125;
    vi.expected_refresh_rate = 60;
    init_ai(&ai, NULL, NULL, push, NULL, &ri, &vi, 0);
    for (int poll = 0; poll <= 1; ++poll)
    {
        check_span(&ai, 32, 32, 24, 24, poll);
        check_span(&ai, sizeof(ram) - 8, 32, 32, 8, poll);
        check_span(&ai, sizeof(ram), 32, 32, 0, poll);
        check_span(&ai, 0xfffffff8, 32, 32, 0, poll);
        check_span(&ai, 0, 0, 32, 0, poll); /* underflowed offset */
        check_span(&ai, 0, 32, 0xffffffff, 220, poll);
        check_span(&ai, sizeof(ram) - 8, 32, 24, 0, poll);
        check_span(&ai, sizeof(ram) - 8, 32, 23, 0, poll);
        check_span(&ai, sizeof(ram) - 8, 7, 7, 4, poll);
        check_span(&ai, sizeof(ram) - 8, 3, 3, 0, poll);
    }
    poweron_ai(&ai);
    write_ai_regs(&ai, AI_DRAM_ADDR_REG * 4, 0xa0000027, ~0u);
    write_ai_regs(&ai, AI_LEN_REG * 4, 0xfffc001f, ~0u);
    assert(ai.fifo[0].address == 0x20 && ai.fifo[0].length == 0x18);
    assert(ai.regs[AI_LEN_REG] == 0x18);
    write_ai_regs(&ai, AI_DRAM_ADDR_REG * 4, 0xa000003f, ~0u);
    write_ai_regs(&ai, AI_LEN_REG * 4, 0xffffffff, ~0u);
    assert(ai.fifo[1].address == 0x38 && ai.fifo[1].length == 0x3fff8);
    return 0;
}
