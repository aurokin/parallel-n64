/* Cartridge write echoes expire with IO, without a DMA completion IRQ. */
#include "main/device.h"
#include "memory/memory.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>

struct device g_dev;
int g_rom_size;
void cp0_update_count(void) {}
static unsigned events, interrupts, removals, clears, pending;
static int event_type;
static unsigned event_delay;
void add_interrupt_event(int type, unsigned int delay) { ++events; ++pending; event_type=type; event_delay=delay; }
void remove_event(int type) { assert(type == PI_INT && pending); --pending; ++removals; }
int has_interrupt_event(int type) { assert(type == PI_INT); return pending != 0; }
void raise_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; assert(intr == MI_INTR_PI); ++interrupts; }
void clear_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; assert(intr == MI_INTR_PI); ++clears; }
void invalidate_r4300_cached_code(uint32_t addr, size_t size) { (void)addr; (void)size; }
void force_detected_rdram_size_hack(void) {}
void dd_pi_test(void) {}
void dd_update_bm(void* dd) { (void)dd; }
void DebugMessage(int level, const char* fmt, ...) { (void)level; (void)fmt; }
void dma_read_sram(struct pi_controller* pi) { (void)pi; }
void dma_write_sram(struct pi_controller* pi) { (void)pi; }
void init_sram(struct sram* s, void* u, void (*save)(void*), uint8_t* data)
{ (void)s; (void)u; (void)save; (void)data; }
void init_dd_rom(struct dd_rom* r, uint8_t* data, size_t size)
{ (void)r; (void)data; (void)size; }
void poweron_dd_rom(struct dd_rom* r) { (void)r; }

static uint32_t ram[64];
static uint32_t cartridge[4] = {0xfc000000, 0x11223344, 0x55667788, 0x99aabbcc};

static void fire_pi(struct pi_controller *pi)
{
    assert(pending);
    --pending;
    pi_end_of_dma_event(pi);
}

int main(void)
{
    struct pi_controller *pi = &g_dev.pi;
    pi->r4300 = &g_dev.r4300;
    pi->ri = &g_dev.ri;
    pi->ri->rdram.dram = ram;
    pi->ri->rdram.dram_size = sizeof(ram);
    init_cart_rom(&pi->cart_rom, (uint8_t*)cartridge, sizeof(cartridge));
    poweron_cart_rom(&pi->cart_rom);
    g_rom_size = sizeof(cartridge);
    uint32_t word;

    write_cart_rom(pi, 0x10000000, 0, ~0u);
    assert(pi->regs[PI_STATUS_REG] == PI_STATUS_IO_BUSY);
    assert(events == 1 && event_type == PI_INT && event_delay == 0x1000);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0); /* The write echo lasts until IO completion. */
    fire_pi(pi);
    assert(pi->regs[PI_STATUS_REG] == 0 && interrupts == 0);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);

    /* Idle reads are independent of the old bus latch. */
    pi->cart_rom.last_write = 0;
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);

    write_cart_rom(pi, 0x10000000, 0xaabbccdd, 0xffff0000);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xaabb0000);
    memset(ram, 0x5a, sizeof(ram));
    pi->regs[PI_CART_ADDR_REG] = 0x10000000;
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(pi->regs[PI_STATUS_REG] == (PI_STATUS_IO_BUSY | PI_STATUS_ERROR));
    assert(events == 2 && ram[0] == 0x5a5a5a5a);
    write_pi_regs(pi, PI_RD_LEN_REG * 4, 1, ~0u);
    assert(events == 2 && cartridge[0] == 0xfc000000);
    fire_pi(pi);
    assert(interrupts == 0);

    pi->regs[PI_STATUS_REG] = 0;
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(pi->regs[PI_STATUS_REG] & PI_STATUS_DMA_BUSY);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);
    assert(events == 3);
    write_cart_rom(pi, 0x10000000, 0x12340000, ~0u);
    assert(pi->regs[PI_STATUS_REG] & PI_STATUS_ERROR);
    assert(events == 3); /* A busy write cannot replace the active DMA event. */
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);
    fire_pi(pi);
    assert(interrupts == 1);
    assert(!(pi->regs[PI_STATUS_REG] & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)));
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);
    /* Reset aborts the active operation and clears sticky request errors. */
    write_cart_rom(pi, 0x10000000, 0, ~0u);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(pi->regs[PI_STATUS_REG] & PI_STATUS_ERROR);
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET, 0);
    assert(pi->regs[PI_STATUS_REG] & PI_STATUS_IO_BUSY);
    assert(removals == 0);
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET | PI_STATUS_CLR_INTR, ~0u);
    assert(pi->regs[PI_STATUS_REG] == 0 && removals == 1 && clears == 1);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(pi->regs[PI_STATUS_REG] & PI_STATUS_DMA_BUSY);
    assert(events == 5);
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET, ~0u);
    assert(pi->regs[PI_STATUS_REG] == 0 && removals == 2);
    write_cart_rom(pi, 0x10000000, 0, ~0u);
    assert(events == 6 && pi->regs[PI_STATUS_REG] == PI_STATUS_IO_BUSY);
    fire_pi(pi);
    assert(interrupts == 1 && pi->regs[PI_STATUS_REG] == 0);
    /* A busy write still updates the bus latch, without replacing its event. */
    write_cart_rom(pi, 0x10000000, 0x11223344, ~0u);
    write_cart_rom(pi, 0x10000000, 0xaabbccdd, ~0u);
    assert(events == 7);
    assert(pi->regs[PI_STATUS_REG] == (PI_STATUS_IO_BUSY | PI_STATUS_ERROR));
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xaabbccdd);
    fire_pi(pi);
    assert(interrupts == 1);
    read_cart_rom(pi, 0x10000000, &word);
    assert(word == 0xfc000000);
    /* Legacy overlapping DMAs keep both completion interrupts. */
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET, ~0u);
    unsigned old_events = events, old_interrupts = interrupts;
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(events == old_events + 2);
    assert(!(pi->regs[PI_STATUS_REG] & PI_STATUS_ERROR));
    fire_pi(pi);
    fire_pi(pi);
    assert(interrupts == old_interrupts + 2);
    /* A stale DMA completion cannot take ownership of a new CPU IO event. */
    old_events = events; old_interrupts = interrupts;
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    fire_pi(pi);
    write_cart_rom(pi, 0x10000000, 0x12345678, ~0u);
    assert(events == old_events + 2 && pending == 1);
    assert(pi->regs[PI_STATUS_REG] == PI_STATUS_ERROR);
    fire_pi(pi);
    assert(interrupts == old_interrupts + 2);
    write_cart_rom(pi, 0x10000000, 0xabcdef01, ~0u);
    assert(events == old_events + 3 && pending == 1);
    fire_pi(pi);
    assert(interrupts == old_interrupts + 2);

    /* Reset drains all legacy duplicate events, including the final one. */
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET, ~0u);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    write_pi_regs(pi, PI_WR_LEN_REG * 4, 1, ~0u);
    assert(pending == 2);
    unsigned old_removals = removals;
    write_pi_regs(pi, PI_STATUS_REG * 4, PI_STATUS_RESET, ~0u);
    assert(pending == 0 && removals == old_removals + 2);
    assert(pi->regs[PI_STATUS_REG] == 0);
    return 0;
}
