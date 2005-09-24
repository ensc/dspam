/* $Id: hash_drv.h,v 1.2 2005/09/24 01:18:58 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef _HASH_DRV_H
#  define _HASH_DRV_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "config.h"
#include "nodetree.h"
#include "libdspam.h"

/* Default number of total records (per concept) to support */

#define HASH_REC_MAX	2000000

typedef struct _hash_drv_header
{
  unsigned long hash_rec_max;
  struct _ds_spam_totals totals;
} *hash_drv_header_t;

typedef struct _hash_drv_map
{
  void *addr;
  int fd;
  size_t file_len;
  hash_drv_header_t header;
} *hash_drv_map_t;

struct _hash_drv_storage
{
  struct _hash_drv_map *db;
  FILE *lock;
  int dbh_attached;

  unsigned long offset_nexttoken;
  struct nt *dir_handles;
};

typedef struct _hash_drv_spam_record
{
  unsigned long long hashcode;
  long nonspam;
  long spam;
} *hash_drv_spam_record_t;

int _hash_drv_get_spamtotals (DSPAM_CTX * CTX);
int _hash_drv_set_spamtotals (DSPAM_CTX * CTX);
int _hash_drv_lock_get (DSPAM_CTX * CTX, struct _hash_drv_storage *s, 
  const char *username);
int _hash_drv_lock_free (struct _hash_drv_storage *s, const char *username);
int _hash_drv_open(const char *filename, hash_drv_map_t map, unsigned long recmaxifnew);
int _hash_drv_close(hash_drv_map_t map);

#endif /* _HASH_DRV_H */
