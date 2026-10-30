/* FlashRAM status and DD DMA must not reach outside their backing arrays. */
#include "main/device.h"
#include "memory/memory.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>

struct device g_dev;
int g_rom_size = 0x4000000; /* Cart size must not be used as a DD buffer bound. */
void cp0_update_count(void) {}
void add_interrupt_event(int type, unsigned int delay) { (void)type; (void)delay; }
void raise_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; (void)intr; }
void clear_rcp_interrupt(struct r4300_core* cpu, uint32_t intr) { (void)cpu; (void)intr; }
void invalidate_r4300_cached_code(uint32_t addr, size_t size) { (void)addr; (void)size; }
void force_detected_rdram_size_hack(void) {}
void dd_pi_test(void) {}
void dd_update_bm(void* dd) { (void)dd; }
void DebugMessage(int level, const char* fmt, ...) { (void)level; (void)fmt; }
void dma_read_sram(struct pi_controller* pi) { (void)pi; }
void dma_write_sram(struct pi_controller* pi) { (void)pi; }
void init_sram(struct sram* s, void* u, void (*save)(void*), uint8_t* data)
{ (void)s; (void)u; (void)save; (void)data; }
void init_cart_rom(struct cart_rom* r, uint8_t* data, size_t size)
{ (void)r; (void)data; (void)size; }
void init_dd_rom(struct dd_rom* r, uint8_t* data, size_t size)
{ (void)r; (void)data; (void)size; }
void poweron_cart_rom(struct cart_rom* r) { (void)r; }
void poweron_dd_rom(struct dd_rom* r) { (void)r; }

static uint32_t ram[(0x800000 + 16) / 4];
static void flash_status(struct pi_controller* pi, uint32_t address)
{
    pi->regs[PI_DRAM_ADDR_REG] = address;
    pi->flashram.mode = FLASHRAM_MODE_STATUS;
    pi->flashram.status = UINT64_C(0x0123456789abcdef);
    dma_read_flashram(pi);
    for (unsigned int i = 0x800000 / 4; i < sizeof(ram) / 4; ++i)
        assert(ram[i] == 0x5a5a5a5a);
}

int main(void)
{
    struct pi_controller* pi = &g_dev.pi;
    pi->ri = &g_dev.ri;
    pi->ri->rdram.dram = ram;
    pi->ri->rdram.dram_size = 0x800000;
    memset(ram, 0x5a, sizeof(ram));
    flash_status(pi, 0);
    assert(ram[0] == 0x01234567 && ram[1] == 0x89abcdef);
    flash_status(pi, 0x7ffffc);
    assert(ram[0x7ffffc / 4] == 0x01234567);
    flash_status(pi, 0x800000);
    flash_status(pi, 0xfffffc);

    const uint32_t addresses[] = {0, 0x7ffffe, 0x800000, 0x900000, 0xfffffe, 0xffffffff};
    for (unsigned int a = 0; a < sizeof(addresses) / sizeof(addresses[0]); ++a)
    {
        memset(ram, 0x5a, sizeof(ram));
        memset(&g_dev.dd, 0x77, sizeof(g_dev.dd));
        pi->regs[PI_CART_ADDR_REG] = 0x05000400;
        pi->regs[PI_DRAM_ADDR_REG] = addresses[a];
        /* A huge requested length must stop at the 256-byte sector buffer. */
        write_pi_regs(pi, PI_RD_LEN_REG * 4, 0xffffff, ~0u);
        unsigned int copied = addresses[a] >= 0x800000 ? 0 :
            (addresses[a] == 0x7ffffe ? 2 : sizeof(g_dev.dd.sec_buf));
        for (unsigned int i = 0; i < sizeof(g_dev.dd.sec_buf); ++i)
            assert(g_dev.dd.sec_buf[i ^ S8] == (i < copied ? 0x5a : 0x77));
        /* No DD field adjacent to the buffer may be overwritten. */
        struct dd_controller expected;
        memset(&expected, 0x77, sizeof(expected));
        memcpy(expected.sec_buf, g_dev.dd.sec_buf, sizeof(expected.sec_buf));
        assert(memcmp(&expected, &g_dev.dd, sizeof(expected)) == 0);

        for (int c2 = 0; c2 <= 1; ++c2)
        {
            memset(ram, 0x5a, sizeof(ram));
            memset(&g_dev.dd, 0x77, sizeof(g_dev.dd));
            pi->regs[PI_CART_ADDR_REG] = c2 ? 0x05000000 : 0x05000400;
            write_pi_regs(pi, PI_WR_LEN_REG * 4, 0xffffff, ~0u);
            unsigned int n = addresses[a] >= 0x800000 ? 0 :
                (addresses[a] == 0x7ffffe ? 2 : (c2 ? 0x400 : 0x100));
            if (n)
                for (unsigned int i = 0; i < n; ++i)
                    assert(((uint8_t*)ram)[(addresses[a] + i) ^ S8] == 0x77);
            /* Check all RDRAM bytes outside the expected span, including guard. */
            for (unsigned int i = 0; i < sizeof(ram); ++i)
                if ((i ^ S8) < addresses[a] || (uint64_t)(i ^ S8) >= (uint64_t)addresses[a] + n)
                    assert(((uint8_t*)ram)[i] == 0x5a);
        }
    }
    return 0;
}
