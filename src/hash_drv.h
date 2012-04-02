/* $Id: hash_drv.h,v 1.19 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _HASH_DRV_H
#  define _HASH_DRV_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "config.h"
#include "nodetree.h"
#include "libdspam.h"

#define HASH_REC_MAX	98317
#define HASH_EXTENT_MAX 49157
#define HASH_SEEK_MAX   100

typedef struct _hash_drv_header
{
  unsigned long hash_rec_max;
  struct _ds_spam_totals totals;
  char padding[4]; /* Keep 8-byte alignment */
} *hash_drv_header_t;

typedef struct _hash_drv_map
{
  void *addr;
  int fd;
  size_t file_len;
  hash_drv_header_t header;
  char filename[MAX_FILENAME_LENGTH];
  unsigned long max_seek;
  unsigned long max_extents;
  unsigned long extent_size;
  int pctincrease;
  int flags;
} *hash_drv_map_t;

struct _hash_drv_storage
{
  hash_drv_map_t map;
  FILE *lock;
  int dbh_attached;

  unsigned long offset_nexttoken;
  hash_drv_header_t offset_header;
  unsigned long hash_rec_max;
  unsigned long max_seek;
  unsigned long max_extents;
  unsigned long extent_size;
  int pctincrease;
  int flags;

  struct nt *dir_handles;
} *hash_drv_storage_t;

typedef struct _hash_drv_spam_record
{
  unsigned long long hashcode;
  unsigned long nonspam;
  unsigned long spam;
} *hash_drv_spam_record_t;

int _hash_drv_get_spamtotals 
  (DSPAM_CTX * CTX);

int _hash_drv_set_spamtotals
  (DSPAM_CTX * CTX);

int _hash_drv_lock_get (
  DSPAM_CTX *CTX,
  struct _hash_drv_storage *s, 
  const char *username);

int _hash_drv_lock_free (
  struct _hash_drv_storage *s,
  const char *username);

/* lock variant used by css tools */
FILE* _hash_tools_lock_get (const char *cssfilename);

int _hash_tools_lock_free (
  const char *cssfilename,
  FILE* lockfile);

int _hash_drv_open(
  const char *filename,
  hash_drv_map_t map,
  unsigned long recmaxifnew,
  unsigned long max_seek,
  unsigned long max_extents,
  unsigned long extent_size,
  int pctincrease,
  int flags);

int _hash_drv_close
  (hash_drv_map_t map);

int _hash_drv_autoextend
  (hash_drv_map_t map, int extents, unsigned long last_extent_size);

unsigned long _hash_drv_seek(
  hash_drv_map_t map,
  unsigned long offset,
  unsigned long long hashcode,
  int flags);

int
_hash_drv_set_spamrecord (
  hash_drv_map_t map,
  hash_drv_spam_record_t wrec,
  unsigned long map_offset);

unsigned long
_hash_drv_get_spamrecord (
  hash_drv_map_t map,
  hash_drv_spam_record_t wrec);

#define HSEEK_INSERT	0x01

#define HMAP_AUTOEXTEND	0x01

#endif /* _HASH_DRV_H */
