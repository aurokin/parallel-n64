/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eeprom.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
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

#include "eeprom.h"

#include "../api/m64p_types.h"
#include "../api/callbacks.h"

#include <string.h>

void init_eeprom(struct eeprom *eeprom,
      void *user_data,
      void (*save)(void*),
      uint8_t *data,
      size_t size,
      uint16_t id)
{
   /* connect saved_memory.eeprom to eeprom */
   eeprom->user_data = user_data;
   eeprom->save      = save;
   eeprom->data      = data;
   eeprom->size      = size;
   eeprom->id        = id;
}

size_t eeprom_size_with_legacy_save(size_t configured_size, const uint8_t data[0x800])
{
   size_t i;
   if (configured_size != 0)
      return configured_size;
   for (i = 0x800; i > 0; --i)
      if (data[i - 1] != 0xff)
         return i > 0x200 ? 0x800 : 0x200;
   return 0;
}

void eeprom_save(struct eeprom* eeprom)
{
   eeprom->save(eeprom->user_data);
}

void format_eeprom(uint8_t* eeprom, size_t size)
{
   memset(eeprom, 0xff, size);
}

void eeprom_status_command(struct eeprom* eeprom, uint8_t* cmd)
{
   if (eeprom->size == 0)
   {
      cmd[1] |= 0x80;
      return;
   }

   /* check size */
   if (cmd[1] != 3)
   {
      cmd[1] |= 0x40;
      if ((cmd[1] & 3) > 0)
         cmd[3] = (eeprom->id & 0xff);
      if ((cmd[1] & 3) > 1)
         cmd[4] = (eeprom->id >> 8);
      if ((cmd[1] & 3) > 2)
         cmd[5] = 0;
   }
   else
   {
      cmd[3] = (eeprom->id & 0xff);
      cmd[4] = (eeprom->id >> 8);
      cmd[5] = 0;
   }
}

void eeprom_read_command(struct eeprom* eeprom, uint8_t* cmd)
{
   uint16_t address = cmd[3] * 8;
   uint8_t* data = &cmd[4];
   if (eeprom->size == 0)
   {
      cmd[1] |= 0x80;
      return;
   }
   /* read 8-byte block */
   if (address < eeprom->size)
   {
      memcpy(data, &eeprom->data[address], 8);
   }
   else
   {
      DebugMessage(M64MSG_WARNING, "Invalid access to eeprom address=%04x", address);
   }
}

void eeprom_write_command(struct eeprom* eeprom, uint8_t* cmd)
{
   uint16_t address = cmd[3] * 8;
   const uint8_t* data = &cmd[4];
   if (eeprom->size == 0)
   {
      cmd[1] |= 0x80;
      return;
   }
   /* write 8-byte block */
   if (address < eeprom->size)
   {
      memcpy(&eeprom->data[address], data, 8);
      eeprom_save(eeprom);
   }
   else
   {
      DebugMessage(M64MSG_WARNING, "Invalid access to eeprom address=%04x", address);
   }
}
