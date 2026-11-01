/* Synthetic headers exercise the real loader without copyrighted ROM data. */
#undef NDEBUG
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../mupen64plus-core/src/main/rom.c"
#include "si/eeprom.h"

unsigned int count_per_op;
int g_MemHasBeenBSwapped, g_delay_si;
bool frame_dupe;
void DebugMessage(int level, const char *message, ...) { (void)level; (void)message; }

static void put_be32(unsigned char *out, uint32_t value)
{
   out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value;
}

static void expect_header(const char *code, unsigned revision, uint64_t crc,
      unsigned save, unsigned cpop, unsigned disableextramem, unsigned order)
{
   unsigned char rom[4096] = {0x80, 0x37, 0x12, 0x40};
   unsigned i;
   put_be32(rom + 0x10, crc >> 32);
   put_be32(rom + 0x14, crc);
   memcpy(rom + 0x20, "SYNTHETIC SETTINGS  ", 20);
   memcpy(rom + 0x3b, code, 4);
   rom[0x3f] = revision;
   if (order)
      for (i = 0; i < sizeof(rom); i += order)
      {
         unsigned j;
         for (j = 0; j < order / 2; ++j)
         {
            unsigned char byte = rom[i + j];
            rom[i + j] = rom[i + order - 1 - j];
            rom[i + order - 1 - j] = byte;
         }
      }
   assert(open_rom(rom, sizeof(rom)) == M64ERR_SUCCESS);
   assert(ROM_SETTINGS.savetype == save);
   assert(count_per_op == cpop);
   assert(ROM_PARAMS.disableextramem == disableextramem);
   assert(strlen(ROM_SETTINGS.MD5) == 32);
   assert(close_rom() == M64ERR_SUCCESS);
}

static void expect_md5(const char *md5, unsigned save, unsigned cpop, unsigned disableextramem)
{
   md5_byte_t digest[16];
   struct romdatabase_settings result;
   assert(parse_hex(md5, digest, sizeof(digest)));
   /* MD5 wins even when a conflicting known CRC is supplied (Robopon). */
   assert(romdatabase_lookup(digest, 0x272B690FAD0A7A77ULL, &result));
   assert(result.settings.savetype == save);
   assert(result.countperop == cpop);
   assert(result.disableextramem == disableextramem);
   assert(result.settings.goodname[0]);
}

static void expect_eeprom(void)
{
   uint8_t storage[0x800] = {0}, cmd[13] = {0};
   struct eeprom eeprom;
   init_eeprom(&eeprom, NULL, NULL, storage, 0, 0);
   eeprom_status_command(&eeprom, cmd); assert(cmd[1] == 0x80);
   cmd[1] = 8; eeprom_read_command(&eeprom, cmd); assert(cmd[1] == 0x88);
   cmd[1] = 1; eeprom_write_command(&eeprom, cmd); assert(cmd[1] == 0x81);
   init_eeprom(&eeprom, NULL, NULL, storage, 0x800, 0xc000);
   cmd[1] = 3; eeprom_status_command(&eeprom, cmd);
   assert(cmd[1] == 3 && cmd[3] == 0 && cmd[4] == 0xc0);
   init_eeprom(&eeprom, NULL, NULL, storage, 0x200, 0x8000);
   cmd[1] = 3; eeprom_status_command(&eeprom, cmd);
   assert(cmd[1] == 3 && cmd[4] == 0x80);
}

static void expect_legacy_eeprom(void)
{
   uint8_t storage[0x800];
   memset(storage, 0xff, sizeof(storage));
   assert(eeprom_size_with_legacy_save(0, storage) == 0);
   assert(eeprom_size_with_legacy_save(0x200, storage) == 0x200);
   assert(eeprom_size_with_legacy_save(0x800, storage) == 0x800);
   storage[0x1ff] = 0;
   assert(eeprom_size_with_legacy_save(0, storage) == 0x200);
   storage[0x200] = 0;
   assert(eeprom_size_with_legacy_save(0, storage) == 0x800);
   assert(eeprom_size_with_legacy_save(0x200, storage) == 0x200);
   memset(storage, 0xff, sizeof(storage));
   storage[0x7ff] = 0;
   assert(eeprom_size_with_legacy_save(0, storage) == 0x800);
}

int main(void)
{
   struct romdatabase_settings result;
   /* Known catalog MD5s, including missing CRC revisions and RefMD5 inheritance. */
   expect_md5("21954E4E404D9E87DBDB87DD309F3E94", EEPROM_16KB, 1, 0); /* Excitebike Rev1 */
   expect_md5("B42F62483F7CA2AAC5AF911175463DB8", EEPROM_4KB, 1, 0); /* Kiwame Rev1 */
   expect_md5("444F70A655AC89CA900F6FAFAF926B16", EEPROM_16KB, 2, 0); /* Robopon */
   expect_md5("115118DD5E0F02D82BA1BF070A7B78F1", EEPROM_16KB, 2, 0); /* Custom Robo V2 */
   expect_md5("464211ABB602EE1005974D2D835A3BCF", NONE, 2, 1); /* Iggy's */
   expect_md5("FB19AFD5E8C49978E6E6AE3622E0498A", NONE, 1, 0); /* Bottom of the 9th */
   expect_md5("8595BA0A0D2EA73944228BDED4A969AD", FLASH_RAM, 1, 0); /* PM translation RefMD5 */
   {
      md5_byte_t digest[16];
      assert(parse_hex("8D3D9F294B6E174BC7B1D2FD1C727530", digest, 16));
      assert(romdatabase_lookup(digest, 0, &result));
      assert(!strcmp(result.settings.goodname, "64DD IPL (JPN)"));
   }

   /* Real CRC fallback and cartridge fallback both survive byte order conversion. */
   expect_header("NMXE", 1, 0xF9D411E37CB29BC0ULL, EEPROM_16KB, 1, 0, 0);
   expect_header("NKMJ", 1, 0x0AE2B37CFBC174F0ULL, EEPROM_4KB, 1, 0, 2);
   expect_header("NR7J", 0, 0x272B690FAD0A7A77ULL, EEPROM_16KB, 2, 0, 4);
   expect_header("NCZJ", 0, 0x079501B9AB0232ABULL, EEPROM_16KB, 2, 0, 0);
   expect_header("NMXE", 3, 0x123456789ABCDEF0ULL, EEPROM_16KB, 1, 0, 2);
   expect_header("NKMJ", 2, 0x123456789ABCDEF0ULL, EEPROM_4KB, 1, 0, 4);
   expect_header("NBOE", 0, 0x123456789ABCDEF0ULL, EEPROM_4KB, 1, 0, 0);
   expect_header("NK4J", 1, 0x123456789ABCDEF0ULL, SRAM, 2, 0, 0);
   expect_header("NK4J", 2, 0x123456789ABCDEF0ULL, EEPROM_16KB, 2, 0, 0);
   expect_header("NWRJ", 1, 0x123456789ABCDEF0ULL, EEPROM_4KB, 2, 0, 0);
   expect_header("NWRJ", 2, 0x123456789ABCDEF0ULL, EEPROM_4KB, 3, 0, 0);
   /* Conflicting catalog entries must not replace cartridge save/timing rows. */
   assert(romdatabase_lookup(NULL, 0x34AEB154A4253B86ULL, &result));
   assert(result.countperop == 2); /* Conker translation inherits the catalog default. */
   expect_header("NFUE", 0, 0x34AEB154A4253B86ULL, EEPROM_16KB, 3, 0, 0);
   expect_header("NKIE", 1, 0x9E8FCDFA49F5652BULL, EEPROM_4KB, 1, 0, 0);
   expect_header("NIJE", 0, 0xAF9DCC151A723D88ULL, EEPROM_4KB, 1, 0, 0);
   assert(romdatabase_lookup(NULL, 0x0A1667C7293346A6ULL, &result));
   assert(result.countperop == 4);
   expect_header("NXFP", 0, 0x0A1667C7293346A6ULL, NONE, 3, 0, 0);
   /* Deliberately conflicting Robopon CRC: catalog fallback timing loses. */
   expect_header("NNCE", 0, 0x272B690FAD0A7A77ULL, EEPROM_16KB, 1, 0, 0);
   expect_header("NDQE", 0, 0xC16C421BA21580F7ULL, EEPROM_4KB, 3, 0, 0);
   expect_header("NDQP", 0, 0x3DF17480193DED5AULL, EEPROM_4KB, 3, 0, 0);
   expect_header("NMGD", 0, 0x2877AC2DC3DC139AULL, EEPROM_4KB, 2, 0, 0);
   /* Top Gear Rally uses NTR in Japan/Europe, but NGR in the US. */
   expect_header("NTRE", 0, 0x62269B3DFE11B1E8ULL, EEPROM_4KB, 2, 0, 0);
   /* The US CRC is ambiguous: an unmatched synthetic ROM gets the default. */
   expect_header("NGRE", 0, 0x62269B3DFE11B1E8ULL, EEPROM_4KB, 2, 0, 0);
   /* A unique catalog None entry (Donald's CRC) proves NGR has no save row. */
   expect_header("NGRE", 0, 0xC16C421BA21580F7ULL, NONE, 3, 0, 0);
   /* No timing row: RE2 and Smash use catalog CountPerOp=1. */
   expect_header("NREE", 1, 0xAA18B1A507DB6AEBULL, SRAM, 1, 0, 0);
   expect_header("NALE", 0, 0x8CF1CECBC8902FF4ULL, SRAM, 1, 0, 0);
   /* CRC-specific timing still wins over an ID row. */
   expect_header("NFUE", 0, 0xD52FE29D8EA6A759ULL, EEPROM_16KB, 1, 0, 0);
   /* Loading an unknown ROM after patched titles must reset timing and memory. */
   expect_header("NZZE", 0, 0x123456789ABCDEF0ULL, EEPROM_4KB, 2, 0, 0);
   assert(!romdatabase_lookup(NULL, 0x123456789ABCDEF0ULL, &result));
   /* Iggy's CRC is duplicated in the catalog, so it cannot resolve alone. */
   assert(!romdatabase_lookup(NULL, 0xE616B5BCC9658B88ULL, &result));
   expect_eeprom();
   expect_legacy_eeprom();
   return 0;
}
