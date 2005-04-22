/* $Id: libdb4_drv.h,v 1.2 2005/04/22 21:11:35 jonz Exp $ */

*
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

#ifndef _LIBDB4_DRV_H
#  define _LIBDB4_DRV_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <db.h>
#include "config.h"
#include "nodetree.h"
#include "libdspam.h"

#define DB_CACHESIZE_MB	4

#if DB_VERSION_MINOR == 0
#       define DBOPEN(A, B, C, D, E, F, G) open(A, C, D, E, F, G)
#else
#       define DBOPEN(A, B, C, D, E, F, G) open(A, B, C, D, E, F, G)
#endif

/* Our storage structure */

struct _libdb4_drv_storage
{
  DB *db, *sig;
  DBC *c;
  DB_ENV *env;
  FILE *lock;
  char dictionary[MAX_FILENAME_LENGTH];
  char signature[MAX_FILENAME_LENGTH];
  struct nt *dir_handles;
};

struct _libdb4_drv_spam_record
{
  long spam_hits;
  long innocent_hits;
  time_t last_hit;
};

/* Private, driver-specific functions */

int _libdb4_drv_get_spamtotals	(DSPAM_CTX * CTX);
int _libdb4_drv_set_spamtotals	(DSPAM_CTX * CTX);
int _libdb4_drv_lock_get	(DSPAM_CTX * CTX, struct _libdb4_drv_storage *s, const char *username);
int _libdb4_drv_lock_free	(struct _libdb4_drv_storage *s, const char *username);
int _libdb4_drv_recover		(DSPAM_CTX *CTX, int fatal);

#endif /* _LIBDB4_DRV_H */
