/* $Id: css_drv.h,v 1.3 2005/05/18 16:12:05 jonz Exp $ */

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

#ifndef _CSS_DRV_H
#  define _CSS_DRV_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "config.h"
#include "nodetree.h"
#include "libdspam.h"

/* Support up to a million tokens (16M x 2) */

#define CSS_REC_MAX	1000000

struct _css_drv_storage
{
  void *nonspam;	/* mmap'd nonspam css */
  void *spam;		/* mmap'd spam css */

  FILE *lock;

  unsigned long offset_nexttoken;

};

typedef struct _css_drv_spam_record
{
  unsigned long long hashcode;
  long counter;
} *css_drv_spam_record_t;

int _css_drv_get_spamtotals (DSPAM_CTX * CTX);
int _css_drv_set_spamtotals (DSPAM_CTX * CTX);
int _css_drv_lock_get (DSPAM_CTX * CTX, struct _css_drv_storage *s, 
  const char *username);
int _css_drv_lock_free (struct _css_drv_storage *s, const char *username);
void * _css_drv_open(DSPAM_CTX *CTX, const char *filename);
int _css_drv_close(void *map);

#endif /* _CSS_DRV_H */
