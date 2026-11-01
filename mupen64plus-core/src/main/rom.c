/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rom.c                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 Tillin9                                            *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <boolean.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/config.h"
#include "api/m64p_config.h"

#include "md5.h"
#include "rom.h"
#include "mupen64plus.ini.h"
#include "main.h"
#include "util.h"

#include "memory/memory.h"
#include "osal/preproc.h"

#include "../r4300/r4300.h"

#define DEFAULT 16

/* Amount of cpu cycles per vi scanline - empirically determined */
enum { DEFAULT_COUNT_PER_SCANLINE = 1500 };
/* by default, alternate VI timing is disabled */
enum { DEFAULT_ALTERNATE_VI_TIMING = 0 };
/* by default, fixed audio position is disabled */
enum { DEFAULT_FIXED_AUDIO_POS = 0 };
/* by default, Audio Signal is disabled */
enum { DEFAULT_AUDIO_SIGNAL = 0 };

/* Global loaded rom memory space. */
unsigned char* g_rom = NULL;
/* Global loaded rom size. */
int g_rom_size = 0;
unsigned alternate_vi_timing = 0;
int           g_vi_refresh_rate = DEFAULT_COUNT_PER_SCANLINE;

extern bool frame_dupe;

m64p_rom_header   ROM_HEADER;
rom_params        ROM_PARAMS;
m64p_rom_settings ROM_SETTINGS = { "", "", 0, 0, 0, 0, 0x900 };


static const uint8_t Z64_SIGNATURE[4] = { 0x80, 0x37, 0x12, 0x40 };
static const uint8_t V64_SIGNATURE[4] = { 0x37, 0x80, 0x40, 0x12 };
static const uint8_t N64_SIGNATURE[4] = { 0x40, 0x12, 0x37, 0x80 };

// Get the system type associated to a ROM country code.
static m64p_system_type rom_country_code_to_system_type(char country_code)
{
   switch (country_code)
   {
      // PAL codes
      case 0x44:
      case 0x46:
      case 0x49:
      case 0x50:
      case 0x53:
      case 0x55:
      case 0x58:
      case 0x59:
         return SYSTEM_PAL;

         // NTSC codes
      case 0x37:
      case 0x41:
      case 0x45:
      case 0x4a:
      default: // Fallback for unknown codes
         return SYSTEM_NTSC;
   }
}

/* Tests if a file is a valid N64 rom by checking the first 4 bytes. */
static int is_valid_rom(const unsigned char *buffer)
{
   if (memcmp(buffer, Z64_SIGNATURE, sizeof(Z64_SIGNATURE)) == 0
         || memcmp(buffer, V64_SIGNATURE, sizeof(V64_SIGNATURE)) == 0
         || memcmp(buffer, N64_SIGNATURE, sizeof(N64_SIGNATURE)) == 0)
      return 1;
   else
      return 0;
}

/* If rom is a .v64 or .n64 image, byteswap or wordswap loadlength amount of
 * rom data to native .z64 before forwarding. Makes sure that data extraction
 * and MD5ing routines always deal with a .z64 image.
 */
static void swap_rom(unsigned char* localrom, unsigned char* imagetype, int loadlength)
{
   unsigned char temp;
   int i;

   /* Btyeswap if .v64 image. */
   if(localrom[0]==0x37)
   {
      *imagetype = V64IMAGE;
      for (i = 0; i < loadlength; i+=2)
      {
         temp=localrom[i];
         localrom[i]=localrom[i+1];
         localrom[i+1]=temp;
      }
   }
   /* Wordswap if .n64 image. */
   else if(localrom[0]==0x40)
   {
      *imagetype = N64IMAGE;
      for (i = 0; i < loadlength; i+=4)
      {
         temp=localrom[i];
         localrom[i]=localrom[i+3];
         localrom[i+3]=temp;
         temp=localrom[i+1];
         localrom[i+1]=localrom[i+2];
         localrom[i+2]=temp;
      }
   }
   else
      *imagetype = Z64IMAGE;
}

/* The upstream catalog is immutable and small enough to scan at ROM open.
 * Keep its MD5-first / unique-CRC fallback and RefMD5 inheritance without
 * introducing a runtime data-file search path or a second database lifetime. */
struct romdatabase_settings
{
   m64p_rom_settings settings;
   unsigned int countperop;
   unsigned int disableextramem;
};

/* Join bounded catalog literals once; all existing parser cursors stay valid. */
static void romdatabase_init(void)
{
   size_t i;
   char *cursor = inifile;
   if (inifile[0])
      return;
   for (i = 0; i < sizeof(inifile_chunks)/sizeof(inifile_chunks[0]); ++i)
   {
      size_t length = strlen(inifile_chunks[i]);
      memcpy(cursor, inifile_chunks[i], length);
      cursor += length;
   }
   *cursor = '\0';
}

static ini_line romdatabase_next_line(const char **cursor, char *buffer, size_t size)
{
   const char *nl = strchr(*cursor, '\n');
   size_t length = nl ? (size_t)(nl - *cursor) : strlen(*cursor);
   char *line = buffer;
   size_t copied = length < size - 1 ? length : size - 1;
   memcpy(buffer, *cursor, copied);
   buffer[copied] = '\0';
   *cursor += length + (nl != NULL);
   return ini_parse_line(&line);
}

static const char *romdatabase_find(const md5_byte_t *md5, uint64_t crc)
{
   const char *cursor = inifile;
   const char *section = NULL;
   const char *found = NULL;
   char buffer[256];
   while (*cursor)
   {
      ini_line line = romdatabase_next_line(&cursor, buffer, sizeof(buffer));
      if (line.type == INI_SECTION)
      {
         md5_byte_t digest[16];
         section = cursor;
         if (md5 && parse_hex(line.name, digest, sizeof(digest))
               && !memcmp(md5, digest, sizeof(digest)))
            found = section; /* Upstream indexes the last duplicate MD5 first. */
      }
      else if (!md5 && section && line.type == INI_PROPERTY && !strcmp(line.name, "CRC"))
      {
         unsigned int crc1, crc2;
         char extra;
         if (sscanf(line.value, "%X %X%c", &crc1, &crc2, &extra) == 2
               && (((uint64_t)crc1 << 32) | crc2) == crc)
         {
            /* Upstream rejects CRCs shared by multiple catalog entries. */
            if (found)
               return NULL;
            found = section;
         }
      }
   }
   return found;
}

static int romdatabase_lookup(const md5_byte_t *md5, uint64_t crc,
      struct romdatabase_settings *result)
{
   const char *section;
   const char *visited[32];
   unsigned int depth = 0, flags = 0;
   enum { GOODNAME = 1, SAVE = 2, STATUS = 4, PLAYERS = 8, RUMBLE = 16,
      COUNT = 32, EXTRAMEM = 64, SIDMA = 128 };
   romdatabase_init();
   section = md5 ? romdatabase_find(md5, 0) : NULL;
   if (!section)
      section = romdatabase_find(NULL, crc);
   if (!section)
      return 0;

   memset(result, 0, sizeof(*result));
   result->settings.savetype = EEPROM_4KB;
   result->settings.players = 4;
   result->settings.rumble = 1;
   result->settings.sidmaduration = 0x900;
   result->countperop = 2;

   /* Most-specific properties win; missing properties inherit from RefMD5. */
   while (section)
   {
      const char *cursor = section;
      md5_byte_t refmd5[16];
      int has_ref = 0;
      unsigned int i;
      char buffer[256];
      if (depth == sizeof(visited)/sizeof(visited[0]))
         return 0;
      for (i = 0; i < depth; ++i)
         if (visited[i] == section)
            return 0;
      visited[depth++] = section;
      while (*cursor)
      {
         int value;
         ini_line line = romdatabase_next_line(&cursor, buffer, sizeof(buffer));
         if (line.type == INI_SECTION)
            break;
         if (line.type != INI_PROPERTY)
            continue;
         if (!strcmp(line.name, "RefMD5"))
            has_ref = parse_hex(line.value, refmd5, sizeof(refmd5));
         else if (!(flags & GOODNAME) && !strcmp(line.name, "GoodName"))
         {
            strncpy(result->settings.goodname, line.value, sizeof(result->settings.goodname) - 1);
            flags |= GOODNAME;
         }
         else if (!(flags & SAVE) && !strcmp(line.name, "SaveType"))
         {
            static const struct { const char *name; unsigned char type; } types[] = {
               { "Eeprom 4KB", EEPROM_4KB }, { "Eeprom 16KB", EEPROM_16KB },
               { "SRAM", SRAM }, { "Flash RAM", FLASH_RAM },
               { "Controller Pack", CONTROLLER_PACK }, { "None", NONE }
            };
            for (i = 0; i < sizeof(types)/sizeof(types[0]); ++i)
               if (!strcmp(line.value, types[i].name))
               {
                  result->settings.savetype = types[i].type;
                  flags |= SAVE;
                  break;
               }
         }
         else if (!(flags & STATUS) && !strcmp(line.name, "Status")
               && string_to_int(line.value, &value) && value >= 0 && value <= 5)
         {
            result->settings.status = value;
            flags |= STATUS;
         }
         else if (!(flags & PLAYERS) && !strcmp(line.name, "Players")
               && string_to_int(line.value, &value) && value >= 0 && value <= 7)
         {
            result->settings.players = value;
            flags |= PLAYERS;
         }
         else if (!(flags & RUMBLE) && !strcmp(line.name, "Rumble")
               && (!strcmp(line.value, "Yes") || !strcmp(line.value, "No")))
         {
            result->settings.rumble = !strcmp(line.value, "Yes");
            flags |= RUMBLE;
         }
         else if (!(flags & COUNT) && !strcmp(line.name, "CountPerOp")
               && string_to_int(line.value, &value) && value >= 1 && value <= 4)
         {
            result->countperop = value;
            flags |= COUNT;
         }
         else if (!(flags & EXTRAMEM) && !strcmp(line.name, "DisableExtraMem"))
         {
            result->disableextramem = atoi(line.value);
            flags |= EXTRAMEM;
         }
         else if (!(flags & SIDMA) && !strcmp(line.name, "SiDmaDuration")
               && string_to_int(line.value, &value) && value >= 0 && value <= 0x10000)
         {
            result->settings.sidmaduration = value;
            flags |= SIDMA;
         }
      }
      section = has_ref ? romdatabase_find(refmd5, 0) : NULL;
   }
   return 1;
}

m64p_error open_rom(const unsigned char* romimage, unsigned int size)
{
#include "rom_luts.c"
   md5_state_t state;
   md5_byte_t digest[16];
   char buffer[256];
   unsigned char imagetype;
   int i;
   uint64_t lut_id;
   char cart_id[4];
   struct romdatabase_settings database;
   int database_matched;
   int cartridge_save_matched = 0, cartridge_cpop_matched = 0;
   int patch_applied = 0;

   /* check input requirements */
   if (g_rom != NULL)
   {
      DebugMessage(M64MSG_ERROR, "open_rom(): previous ROM image was not freed");
      return M64ERR_INTERNAL;
   }
   if (romimage == NULL || !is_valid_rom(romimage))
   {
      DebugMessage(M64MSG_ERROR, "open_rom(): not a valid ROM image");
      return M64ERR_INPUT_INVALID;
   }

   /* Clear Byte-swapped flag, since ROM is now deleted. */
   g_MemHasBeenBSwapped = 0;
   /* allocate new buffer for ROM and copy into this buffer */
   g_rom_size = size;
   g_rom = (unsigned char *) malloc(size);
   alternate_vi_timing = 0;
   g_vi_refresh_rate = DEFAULT_COUNT_PER_SCANLINE;
   if (g_rom == NULL)
      return M64ERR_NO_MEMORY;
   memcpy(g_rom, romimage, size);
   swap_rom(g_rom, &imagetype, g_rom_size);

   memcpy(&ROM_HEADER, g_rom, sizeof(m64p_rom_header));

   /* Calculate MD5 hash  */
   md5_init(&state);
   md5_append(&state, (const md5_byte_t*)g_rom, g_rom_size);
   md5_finish(&state, digest);
   for ( i = 0; i < 16; ++i )
      sprintf(buffer+i*2, "%02X", digest[i]);
   buffer[32] = '\0';
   memset(&ROM_SETTINGS, 0, sizeof(ROM_SETTINGS));
   strcpy(ROM_SETTINGS.MD5, buffer);
   ROM_SETTINGS.savetype = EEPROM_4KB;
   ROM_SETTINGS.players = 4;
   ROM_SETTINGS.rumble = 1;
   ROM_SETTINGS.sidmaduration = 0x900;
   count_per_op = 2;
   ROM_PARAMS.disableextramem = 0;

   /* add some useful properties to ROM_PARAMS */
   ROM_PARAMS.systemtype = rom_country_code_to_system_type(ROM_HEADER.destination_code);
   ROM_PARAMS.fixedaudiopos = DEFAULT_FIXED_AUDIO_POS;
   ROM_PARAMS.audiosignal = DEFAULT_AUDIO_SIGNAL;

   memcpy(ROM_PARAMS.headername, ROM_HEADER.Name, 20);
   ROM_PARAMS.headername[20] = '\0';
   trim(ROM_PARAMS.headername); /* Remove trailing whitespace from ROM name. */

   lut_id = (((uint64_t)sl(ROM_HEADER.CRC1)) << 32) | sl(ROM_HEADER.CRC2);

   memcpy(cart_id, g_rom + 0x3b, 3);
   cart_id[3] = '\0';

   /* Cartridge rows take precedence, including revisions and catalogued images. */
   if (!strcmp(cart_id, "NK4") && ROM_HEADER.destination_code == 'J'
         && ROM_HEADER.mask_ROM_version < 2)
   {
      cartridge_save_matched = 1;
      ROM_SETTINGS.savetype = SRAM;
      patch_applied = 1;
   }
   else
   {
      for (i = 0; i < sizeof(lut_cart_save)/sizeof(lut_cart_save[0]); ++i)
      {
         if (!strcmp(lut_cart_save[i].id, cart_id) &&
               (!lut_cart_save[i].region || lut_cart_save[i].region == ROM_HEADER.destination_code))
         {
            cartridge_save_matched = 1;
            ROM_SETTINGS.savetype = lut_cart_save[i].savetype;
            patch_applied = 1;
            break;
         }
      }
   }

   for (i = 0; i < sizeof(lut_audiosignal_id)/sizeof(lut_audiosignal_id[0]); ++i)
   {
      if (!strcmp(lut_audiosignal_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         ROM_PARAMS.audiosignal = 1;
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);

         patch_applied = 1;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_fixedaudiopos_id)/sizeof(lut_fixedaudiopos_id[0]); ++i)
   {
      if (!strcmp(lut_fixedaudiopos_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         ROM_PARAMS.fixedaudiopos = 1;
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);

         patch_applied = 1;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_alternate_vi_id)/sizeof(lut_alternate_vi_id[0]); ++i)
   {
      if (!strcmp(lut_alternate_vi_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         alternate_vi_timing = 1;
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);

         patch_applied = 1;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_vi_clock_1500_id)/sizeof(lut_vi_clock_1500_id[0]); ++i)
   {
      if (!strcmp(lut_vi_clock_1500_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);
	 g_vi_refresh_rate = 1500;

         patch_applied = 1;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_vi_clock_1600_id)/sizeof(lut_vi_clock_1600_id[0]); ++i)
   {
      if (!strcmp(lut_vi_clock_1600_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);
	 g_vi_refresh_rate = 1600;

         patch_applied = 1;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_vi_clock_2200_id)/sizeof(lut_vi_clock_2200_id[0]); ++i)
   {
      if (!strcmp(lut_vi_clock_2200_id[i], cart_id))
      {
         strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
         DebugMessage(M64MSG_INFO, "%s INI patches applied.", ROM_PARAMS.headername);
	 g_vi_refresh_rate = 2200;

         patch_applied = 1;
         break;
      }
   }

   if (!patch_applied)
   {
      strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);
      strcat(ROM_SETTINGS.goodname, " (unknown rom)");
   }

   for (i = 0; i < sizeof(lut_cpop_id)/sizeof(lut_cpop_id[0]); ++i)
   {
      if (!strcmp(lut_cpop_id[i].id, cart_id))
      {
         cartridge_cpop_matched = 1;
         count_per_op = lut_cpop_id[i].cpop;
         break;
      }
   }

   /* Shindou's timing differs from the original Wave Race releases. */
   if (!strcmp(cart_id, "NWR") && ROM_HEADER.destination_code == 'J'
         && ROM_HEADER.mask_ROM_version >= 2)
   {
      cartridge_cpop_matched = 1;
      count_per_op = 3;
   }

   for (i = 0; i < sizeof(lut_cpop)/sizeof(lut_cpop[0]); ++i)
   {
      if (lut_cpop[i][0] == lut_id)
      {
         cartridge_cpop_matched = 1;
         count_per_op = lut_cpop[i][1];
         DebugMessage(M64MSG_INFO, "CountPerOp set to %u.", count_per_op);
         break;
      }
   }

   for (i = 0; i < sizeof(lut_sidmaduration_id)/sizeof(lut_sidmaduration_id[0]); ++i)
   {
      if (!strcmp(lut_sidmaduration_id[i].id, cart_id))
      {
         ROM_SETTINGS.sidmaduration = lut_sidmaduration_id[i].sidmaduration;
         DebugMessage(M64MSG_INFO, "SI DMA Duration set to %u.", ROM_SETTINGS.sidmaduration);
         break;
      }
   }

   /* As upstream: exact MD5 first, then CRC only when it is unambiguous. */
   database_matched = romdatabase_lookup(digest, lut_id, &database);
   if (database_matched)
   {
      char md5[33];
      strcpy(md5, ROM_SETTINGS.MD5);
      if (cartridge_save_matched)
         database.settings.savetype = ROM_SETTINGS.savetype;
      ROM_SETTINGS = database.settings;
      strcpy(ROM_SETTINGS.MD5, md5);
      if (!cartridge_cpop_matched)
         count_per_op = database.countperop;
      ROM_PARAMS.disableextramem = database.disableextramem;
   }
   else if (patch_applied)
      strcpy(ROM_SETTINGS.goodname, ROM_PARAMS.headername);

   if (frame_dupe)
      count_per_op = 1;

   DebugMessage(M64MSG_INFO, "CountPerOp set to %u.", count_per_op);
   DebugMessage(M64MSG_INFO, "Save type: %u; DisableExtraMem: %u; ROM catalog: %s.",
         ROM_SETTINGS.savetype, ROM_PARAMS.disableextramem,
         database_matched ? "matched" : "unmatched (cartridge fallback)");

   g_delay_si = 1; /* default */

   for (i = 0; i < sizeof(lut_delaysi_id)/sizeof(lut_delaysi_id[0]); ++i)
   {
      if (!strcmp(lut_delaysi_id[i].id, cart_id))
      {
         g_delay_si = lut_delaysi_id[i].delaysi;
         break;
      }
   }

   for (i = 0; i < sizeof(lut_delaysi)/sizeof(lut_delaysi[0]); ++i)
   {
      if (lut_delaysi[i][0] == lut_id)
      {
         g_delay_si = lut_delaysi[i][1];
         DebugMessage(M64MSG_INFO, "DelaySI set to %u.", g_delay_si);
         break;
      }
   }

   /* print out a bunch of info about the ROM */
   DebugMessage(M64MSG_INFO, "Goodname: %s", ROM_SETTINGS.goodname);
   DebugMessage(M64MSG_INFO, "Headername: %s", ROM_PARAMS.headername);
   DebugMessage(M64MSG_INFO, "Name: %.20s", ROM_HEADER.Name);
   imagestring(imagetype, buffer);
   DebugMessage(M64MSG_INFO, "MD5: %s", ROM_SETTINGS.MD5);
   DebugMessage(M64MSG_INFO, "CRC: %x %x", sl(ROM_HEADER.CRC1), sl(ROM_HEADER.CRC2));
   DebugMessage(M64MSG_INFO, "Imagetype: %s", buffer);
   DebugMessage(M64MSG_INFO, "Rom size: %d bytes (or %d Mb or %d Megabits)", g_rom_size, g_rom_size/1024/1024, g_rom_size/1024/1024*8);
   DebugMessage(M64MSG_VERBOSE, "ClockRate = %x", sl(ROM_HEADER.ClockRate));
   DebugMessage(M64MSG_INFO, "Version: %x", sl(ROM_HEADER.Release));
   if(sl(ROM_HEADER.Manufacturer_ID) == 'N')
      DebugMessage(M64MSG_INFO, "Manufacturer: Nintendo");
   else
      DebugMessage(M64MSG_INFO, "Manufacturer: %x", sl(ROM_HEADER.Manufacturer_ID));
   DebugMessage(M64MSG_VERBOSE, "Cartridge_ID: %x", ROM_HEADER.Cartridge_ID);
   countrycodestring(ROM_HEADER.destination_code, buffer);
   DebugMessage(M64MSG_INFO, "Country: %s", buffer);
   DebugMessage(M64MSG_VERBOSE, "PC = %x", sl((unsigned int)ROM_HEADER.PC));
   DebugMessage(M64MSG_VERBOSE, "Save type: %d", ROM_SETTINGS.savetype);

   if (!strcmp(ROM_PARAMS.headername, "GOLDENEYE"))
      ROM_PARAMS.special_rom = GOLDEN_EYE;
   else if (!strcmp(ROM_PARAMS.headername, "RAT ATTACK"))
      ROM_PARAMS.special_rom = RAT_ATTACK;
   else if (!strcmp(ROM_PARAMS.headername, "Perfect Dark"))
      ROM_PARAMS.special_rom = PERFECT_DARK;
   else
      ROM_PARAMS.special_rom = NORMAL_ROM;

   return M64ERR_SUCCESS;
}

m64p_error close_rom(void)
{
   if (g_rom == NULL)
      return M64ERR_INVALID_STATE;

   free(g_rom);
   g_rom = NULL;

   /* Clear Byte-swapped flag, since ROM is now deleted. */
   g_MemHasBeenBSwapped = 0;
   DebugMessage(M64MSG_STATUS, "Rom closed.");

   return M64ERR_SUCCESS;
}
