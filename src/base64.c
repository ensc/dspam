/* $Id: base64.c,v 1.99 2011/06/28 00:13:48 sbajic Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2012 DSPAM PROJECT

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * base64.c - base64 encoding/decoding routines
 *
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "base64.h"

char *
base64decode (const char *buf)
{
#ifdef VERBOSE
  LOGDEBUG ("decoding Base64 encoded buffer");
#endif
  unsigned char alphabet[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static char first_time = 1,inalphabet[256], decoder[256];
  int bits, c, char_count;
  int pos = 0, dpos = 0;
  char *decoded;

  decoded = malloc ((strlen (buf) * 2) + 2);
  if (decoded == NULL)
    return NULL;
  decoded[0] = 0;

  if(first_time) {
    int i;
    for (i = (sizeof alphabet) - 1; i >= 0; i--)
    {
      inalphabet[alphabet[i]] = 1;
      decoder[alphabet[i]] = i;
    }
    first_time = 0;
  }
  char_count = 0;
  bits = 0;
  while (buf[pos] != 0)
  {
    c = buf[pos];
    if (c == '=')
      break;
    if (c > 255 || !inalphabet[c])
    {
      pos++;
      continue;
    }
    bits += decoder[c];
    char_count++;
    if (char_count == 4)
    {
      decoded[dpos] = (bits >> 16);
      decoded[dpos + 1] = ((bits >> 8) & 0xff);
      decoded[dpos + 2] = (bits & 0xff);
      decoded[dpos + 3] = 0;
      dpos += 3;
      bits = 0;
      char_count = 0;
    }
    else
    {
      bits <<= 6;
    }
    pos++;
  }
  c = buf[pos];
  if (c == 0)
  {
    if (char_count)
    {
      LOGDEBUG ("base64 encoding incomplete: at least %d bits truncated",
                ((4 - char_count) * 6));
    }
  }
  else
  {                             /* c == '=' */
    switch (char_count)
    {
    case 1:
      LOGDEBUG ("base64 encoding incomplete: at least 2 bits missing");
      break;
    case 2:
      decoded[dpos] = (bits >> 10);
      decoded[dpos + 1] = 0;
      break;
    case 3:
      decoded[dpos] = (bits >> 16);
      decoded[dpos + 1] = ((bits >> 8) & 0xff);
      decoded[dpos + 2] = 0;
      break;
    }
  }
  if (strlen(decoded) > 0 && decoded[strlen(decoded)-1] != '\n')
    strcat(decoded, "\n");
  return decoded;
}

char *
base64encode (const char *buf)
{
#ifdef VERBOSE
  LOGDEBUG ("encoding buffer to Base64");
#endif
  unsigned char alphabet[64] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int cols, bits, c, char_count;
  char *out;
  long rpos = 0, wpos = 0;

  out = malloc (strlen (buf) * 2);
  if (out == NULL)
    return NULL;

  out[0] = 0;

  char_count = 0;
  bits = 0;
  cols = 0;
  c = buf[rpos];
  while (c != 0)
  {
    bits += c;
    char_count++;
    if (char_count == 3)
    {
      out[wpos] = (alphabet[bits >> 18]);
      out[wpos + 1] = (alphabet[(bits >> 12) & 0x3f]);
      out[wpos + 2] = (alphabet[(bits >> 6) & 0x3f]);
      out[wpos + 3] = (alphabet[bits & 0x3f]);
      wpos += 4;
      cols += 4;
      if (cols == 72)
      {
        out[wpos] = '\n';
        wpos++;
        cols = 0;
      }
      out[wpos] = 0;
      bits = 0;
      char_count = 0;
    }
    else
    {
      bits <<= 8;
    }
    rpos++;
    c = buf[rpos];
  }
  if (char_count != 0)
  {
    bits <<= 16 - (8 * char_count);
    out[wpos] = (alphabet[bits >> 18]);
    out[wpos + 1] = (alphabet[(bits >> 12) & 0x3f]);
    wpos += 2;
    if (char_count == 1)
    {
      out[wpos] = '=';
      out[wpos + 1] = '=';
    }
    else
    {
      out[wpos] = (alphabet[(bits >> 6) & 0x3f]);
      out[wpos + 1] = '=';
    }
    wpos += 2;
    if (cols > 0)
    {
      out[wpos] = '\n';
      wpos++;
    }
    out[wpos] = 0;
  }

  if (out[strlen (out) - 1] != '\n')
    strcat (out, "\n");

  return out;
}
