/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_crc32.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <encodings/crc32.h>
#include <zlib.h>

/* zlib's CRC-32 is the same IEEE CRC (identical results) and much faster than
 * the byte-at-a-time table this file used to carry. */
uint32_t encoding_crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
   /* crc32() takes a uInt length; chunk so size_t inputs work with every zlib,
    * including Android's before API 28, which lacks crc32_z(). */
   while (len > 0)
   {
      uInt chunk = len > UINT_MAX ? UINT_MAX : (uInt)len;
      crc = (uint32_t)crc32(crc, buf, chunk);
      buf += chunk;
      len -= chunk;
   }
   return crc;
}
