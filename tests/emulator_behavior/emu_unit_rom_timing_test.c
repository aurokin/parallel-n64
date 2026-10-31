/* Exercise ROM timing selection through the loader using synthetic headers. */
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main/main.h"
#include "main/rom.h"
#include "r4300/r4300.h"

unsigned int count_per_op;
int g_MemHasBeenBSwapped, g_delay_si;
bool frame_dupe;

void DebugMessage(int level, const char *message, ...)
{
    (void)level;
    (void)message;
}

static void put_be32(unsigned char *out, uint32_t value)
{
    out[0] = value >> 24;
    out[1] = value >> 16;
    out[2] = value >> 8;
    out[3] = value;
}

static void expect_timing(uint32_t crc1, uint32_t crc2, const char *cart_id, unsigned expected, int expected_vi)
{
    unsigned char rom[4096] = { 0x80, 0x37, 0x12, 0x40 };
    put_be32(rom + 0x10, crc1);
    put_be32(rom + 0x14, crc2);
    memcpy(rom + 0x20, "SYNTHETIC TIMING TEST", 20);
    memcpy(rom + 0x3b, cart_id, 4);
    count_per_op = 2;
    g_vi_refresh_rate = 2200;
    if (open_rom(rom, sizeof rom) != M64ERR_SUCCESS || count_per_op != expected) {
        fprintf(stderr, "%08" PRIx32 " %08" PRIx32 ": expected CountPerOp %u, got %u\n",
                crc1, crc2, expected, count_per_op);
        exit(EXIT_FAILURE);
    }
    if (g_vi_refresh_rate != expected_vi) {
        fprintf(stderr, "%08" PRIx32 " %08" PRIx32 ": expected VI override %d, got %d\n",
                crc1, crc2, expected_vi, g_vi_refresh_rate);
        exit(EXIT_FAILURE);
    }
    if (close_rom() != M64ERR_SUCCESS)
        exit(EXIT_FAILURE);
}

int main(void)
{
    /* Without a preset the loader keeps the 1500 default half-line period. */
    expect_timing(0x9ba10c4e, 0x0408abd3, "NKMJ", 1, 1500); /* Kiwame Rev 0 */
    expect_timing(0x0ae2b37c, 0xfbc174f0, "NKMJ", 1, 1500); /* Kiwame Rev 1 */
    expect_timing(0x12345678, 0x9abcdef0, "NZZJ", 2, 1500); /* Unknown ROM keeps the defaults. */
    expect_timing(0x46a3f7af, 0x0f7591d0, "NCXE", 2, 1542); /* Cruis'n Exotica */
    expect_timing(0x19c553a7, 0xa70f4b52, "NPNE", 2, 1500); /* Puzzle League */
    expect_timing(0x519ea4e1, 0xeb7584e8, "NZZJ", 2, 1600); /* King Hill */
    expect_timing(0xaf9dcc15, 0x1a723d88, "NZZE", 1, 2200); /* Indiana Jones */
    return EXIT_SUCCESS;
}
