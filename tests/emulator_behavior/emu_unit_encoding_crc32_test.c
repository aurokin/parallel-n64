#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "libretro-common/include/encodings/crc32.h"

/* Bit-by-bit IEEE CRC-32, the definition encoding_crc32 must keep matching:
 * the core compares these values to detect changed code pages. */
static uint32_t reference_crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
   crc = ~crc;
   while (len--)
   {
      crc ^= *buf++;
      for (int bit = 0; bit < 8; bit++)
         crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
   }
   return ~crc;
}

static int failures;

static void check(int condition, const char *message)
{
   if (!condition)
   {
      fprintf(stderr, "FAIL: %s\n", message);
      failures++;
   }
}

int main(void)
{
   check(encoding_crc32(0, (const uint8_t *)"123456789", 9) == 0xcbf43926u, "standard check value");
   check(encoding_crc32(0x12345678u, NULL, 0) == 0x12345678u, "empty input returns the seed");

   static uint8_t page[0x1000 + 7];
   uint32_t state = 1;
   for (size_t i = 0; i < sizeof(page); i++)
   {
      state = state * 1103515245u + 12345u;
      page[i] = (uint8_t)(state >> 16);
   }
   /* Whole and partial pages, unaligned starts, and chained seeds. */
   for (size_t offset = 0; offset < 8; offset++)
      for (size_t len = 0; len + offset <= sizeof(page); len += 509)
      {
         const uint32_t seed = (uint32_t)(offset * 0x9e3779b9u);
         check(encoding_crc32(seed, page + offset, len) == reference_crc32(seed, page + offset, len),
               "matches the bit-by-bit CRC-32");
      }
   check(encoding_crc32(0, page, 0x1000) == reference_crc32(0, page, 0x1000), "4 KiB page");

   if (failures)
      return 1;
   puts("encoding_crc32 matches IEEE CRC-32");
   return 0;
}
