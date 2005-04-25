/* $Id: libdspam.h,v 1.13 2005/04/25 13:05:48 jonz Exp $ */

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

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#ifndef _LIBDSPAM_H
#  define _LIBDSPAM_H

#include "diction.h"
#include "nodetree.h"
#include "error.h"
#include "storage_driver.h"
#include "decode.h"
#include "libdspam_objects.h"
#include "heap.h"

#define LIBDSPAM_VERSION_MAJOR	3
#define LIBDSPAM_VERSION_MINOR	5
#define LIBDSPAM_VERSION_PATCH	0

/* Public API */

int libdspam_init(void);
int libdspam_shutdown(void);

DSPAM_CTX * dspam_init	(const char *username, const char *group,
            const char *home, int operating_mode, u_int32_t flags);

DSPAM_CTX * dspam_create (const char *username, const char *group,
            const char *home, int operating_mode, u_int32_t flags);

int dspam_attach  (DSPAM_CTX *CTX, void *dbh);
int dspam_detach  (DSPAM_CTX *CTX);
int dspam_process (DSPAM_CTX * CTX, const char *message);

int dspam_getsource       (DSPAM_CTX * CTX, char *buf, size_t size);
int dspam_addattribute    (DSPAM_CTX * CTX, const char *key, const char *value);
int dspam_clearattributes (DSPAM_CTX * CTX);

void dspam_destroy        (DSPAM_CTX * CTX);


/* Private functions */

int _ds_calc_stat (DSPAM_CTX * CTX, unsigned long long token, 
     struct _ds_spam_stat *, int type, struct _ds_spam_stat *bnr_tot);
int _ds_calc_result (DSPAM_CTX * CTX, ds_heap_t sort, ds_diction_t diction); 
int _ds_process_header_token (DSPAM_CTX * CTX, char *joined_token, 
    const char *previous_token, ds_diction_t diction, const char *heading);
int _ds_process_body_token (DSPAM_CTX * CTX, char *joined_token, 
    const char *previous_token, ds_diction_t diction);
int _ds_map_header_token (DSPAM_CTX * CTX, char *token, char **previous_tokens,
    ds_diction_t diction, const char *heading);
int _ds_map_body_token          (DSPAM_CTX * CTX, char *token, 
    char **previous_tokens, ds_diction_t diction);
int  _ds_operate               (DSPAM_CTX * CTX, char *headers, char *body);
int  _ds_process_signature     (DSPAM_CTX * CTX);
int  _ds_degenerate_message    (DSPAM_CTX *CTX, buffer *header, buffer *body);
int  _ds_factor                (struct nt *set, char *token_name, float value);
int _ds_instantiate_bnr        (DSPAM_CTX *CTX, ds_diction_t patterns, 
    struct nt *order, char identifier);
void _ds_sbph_clear            (char **previous_tokens);
void _ds_factor_destroy        (struct nt *factors);

/* Standard Return Codes */

#ifndef EINVAL
#       define EINVAL                   -0x01
  /* Invalid call or parms - already initialized or shutdown */
#endif
#define EUNKNOWN                -0x02   /* Unknown/unexpected error */
#define EFILE                   -0x03   /* File error */
#define ELOCK                   -0x04   /* Lock error */
#define EFAILURE                -0x05   /* Failed to perform operation */

#define FALSE_POSITIVE(A) \
  (A->classification == DSR_ISINNOCENT && A->source == DSS_ERROR)
#define SPAM_MISS(A) \
  (A->classification == DSR_ISSPAM && A->source == DSS_ERROR)
#define SIGNATURE_NULL(A)	(A->signature == NULL)

#endif /* _LIBDSPAM_H */

