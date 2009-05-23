/* $Id: buffer.c,v 1.9 2006/05/13 01:12:59 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2009 DSPAM PROJECT

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2
 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/* buffer.c - dynamic string data structure */

#include <string.h>
#include <stdlib.h>

#include "buffer.h"

buffer *
buffer_create (const char *s)
{
  buffer *b;
  long len;

  b = malloc (sizeof (buffer));
  if (!b)
    return NULL;

  if (!s)
  {
    b->size = 1024;
    b->used = 0;
    b->data = malloc(b->size);
    if (!b->data) 
      return NULL;
    b->data[0] = 0;
    return b;
  }

  len = strlen (s);
  b->size = len + 1;
  b->used = len;
  b->data = malloc (b->size);
  if (b->data == NULL)
  {
    free (b);
    return NULL;
  }

  memcpy (b->data, s, len);
  b->data[len] = 0;
  return b;
}

buffer *
buffer_ncreate (const char *s, long plen)
{
  buffer *b;
  long len;

  b = malloc (sizeof (buffer));
  if (!b)
    return NULL;

  if (!s)
  {
    if(plen == 0)
      b->size = 1024;
    else
      b->size = plen;
    b->used = 0;
    b->data = malloc(b->size);
    if (!b->data)
      return NULL;
    b->data[0] = 0;
    return b;
  }

  if(plen == 0)
    len = strlen (s);
  else
    len = plen;
  b->size = len + 1;
  b->used = len;
  b->data = malloc (b->size);
  if (b->data == NULL)
  {
    free (b);
    return NULL;
  }

  memcpy (b->data, s, len);
  b->data[len] = 0;
  return b;
}

int
buffer_clear (buffer * b)
{
  if (b == NULL)
    return -1;

  free (b->data);
  b->size = 0;
  b->used = 0;
  b->data = NULL;

  return 0;
}

void
buffer_destroy (buffer * b)
{
  if (!b)
    return;

  if (b->data != NULL)
  {
    free (b->data);
  }
  free (b);
  return;
}

int
buffer_copy (buffer * b, const char *s)
{
  char *new_data;
  long len;

  if (s == NULL)
    return -1;

  len = strlen (s);
  new_data = malloc (len + 1);
  if (new_data == NULL)
    return -1;

  memcpy (new_data, s, len);
  new_data[len] = 0;

  if (b->data != NULL)
  {
    free (b->data);
  }
  b->size = len + 1;
  b->used = len;
  b->data = new_data;

  return 0;
}

int
buffer_ncopy (buffer * b, const char *s, long plen)
{
  if (s == NULL)
    return -1;

  char *new_data;
  long len;

  if(plen == 0)
    len = strlen (s);
  else
    len = plen;
  new_data = malloc (len + 1);
  if (new_data == NULL)
    return -1;

  memcpy (new_data, s, len);
  new_data[len] = 0;

  if (b->data != NULL)
    free (b->data);
  b->size = len + 1;
  b->used = len;
  b->data = new_data;

  return 0;
}

int
buffer_cat (buffer * b, const char *s)
{
  char *new_data;
  long size;
  long len, used;

  if (!b || !s)
    return -1;

  size = b->size;
  len = strlen (s);
  if (! b->data)
    return buffer_copy (b, s);

  used = b->used + len;
  if (used >= size)
  {
    size *= 2;
    size += len;
    new_data = realloc (b->data, size);
    if (!new_data)
      return -1;
    b->data = new_data;
    b->size = size;
  }
  memcpy (b->data + b->used, s, len);
  b->used = used;
  b->data[b->used] = 0;
  return 0;
}

int
buffer_ncat (buffer * b, const char *s, long plen)
{
  if (!b || !s)
    return -1;
  if (! b->data)
    return buffer_ncopy (b, s, 0);

  char *new_data;
  long size;
  long len, used;

  size = b->size;
  if(plen == 0)
    len = strlen (s);
  else
    len = plen;

  used = b->used + len;
  if (used >= size)
  {
    size *= 2;
    size += len;
    new_data = realloc (b->data, size);
    if (!new_data)
      return -1;
    b->data = new_data;
    b->size = size;
  }
  memcpy (b->data + b->used, s, len);
  b->used = used;
  b->data[b->used] = 0;
  return 0;
}
