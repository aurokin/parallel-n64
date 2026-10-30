/* Exercise the production workaround against ordinary reusable RDRAM. */
#include "main/cheat.h"
#include "main/device.h"
#include "main/rom.h"
#include "memory/memory.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

struct device g_dev;
m64p_rom_header ROM_HEADER;
static uint32_t ram[0x800000 / sizeof(uint32_t)];

int event_gameshark_active(void) { return 0; }

int main(void)
{
    const struct {
        uint32_t crc1, crc2, condition, target;
        uint16_t expected;
    } regions[] = {
        {0xCA12B547, 0x71FA4EE4, 0x382D1C, 0x382D0F, 0x0002}, /* USA */
        {0x7BB18D40, 0x83138559, 0x382D1C, 0x382D0F, 0x0002}, /* Australia */
        {0x39119872, 0x07722E9F, 0x382D1C, 0x382D0F, 0x0002}, /* Station */
        {0xEC0F690D, 0x32A7438C, 0x36D22C, 0x36D21F, 0x802A}, /* Japan 1.0 */
        {0xE0044E9E, 0xCD659D0D, 0x36D22C, 0x36D21F, 0x802A}, /* Japan 1.1 */
        {0x5753720D, 0x2A8A884D, 0x381BDC, 0x381BCF, 0x802C}, /* Germany */
        {0x4FF5976F, 0xACF559D8, 0x381BFC, 0x381BEF, 0x802C}, /* Europe */
        {0xBA6C293A, 0x9FAFA338, 0x381BFC, 0x381BEF, 0x802C}, /* France */
        {0xC0C85046, 0x61051B05, 0x381BFC, 0x381BEF, 0x802C}, /* Italy */
        {0x817D286A, 0xEF417416, 0x381BFC, 0x381BEF, 0x802C}, /* Spain */
    };

    g_dev.ri.rdram.dram = ram;
    g_dev.ri.rdram.dram_size = sizeof(ram);
    for (size_t i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
        memset(ram, 0x5a, sizeof(ram));
        memset(&ROM_HEADER, 0, sizeof(ROM_HEADER));
        memcpy(ROM_HEADER.Name, "POKEMON SNAP", 12);
        ROM_HEADER.CRC1 = sl(regions[i].crc1);
        ROM_HEADER.CRC2 = sl(regions[i].crc2);
        uint8_t *bytes = (uint8_t *)ram;
        const uint32_t target = regions[i].target ^ S8;
        const uint32_t condition = regions[i].condition ^ S16;

        /* A reused heap object must survive every VI when the guard is false. */
        uint16_t value = regions[i].expected ^ 1;
        memcpy(bytes + condition, &value, sizeof(value));
        for (unsigned vi = 0; vi < 3; ++vi)
            cheat_apply_cheats(ENTRY_VI);
        assert(bytes[target] == 0x5a);

        value = regions[i].expected;
        memcpy(bytes + condition, &value, sizeof(value));
        cheat_apply_cheats(ENTRY_BOOT);
        assert(bytes[target] == 0x5a);
        cheat_apply_cheats(ENTRY_VI);
        assert(bytes[target] == 0);
        assert(bytes[target - 1] == 0x5a && bytes[target + 1] == 0x5a);

        bytes[target] = 0x7d;
        value ^= 1;
        memcpy(bytes + condition, &value, sizeof(value));
        cheat_apply_cheats(ENTRY_VI);
        assert(bytes[target] == 0x7d);

        memcpy(ROM_HEADER.Name, "OTHER GAME", 11);
        value = regions[i].expected;
        memcpy(bytes + condition, &value, sizeof(value));
        cheat_apply_cheats(ENTRY_VI);
        assert(bytes[target] == 0x7d);
    }
    return 0;
}
