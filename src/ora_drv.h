/* $Id: ora_drv.h,v 1.3 2005/09/24 17:48:59 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

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

#ifndef _ORA_DRV_H
#  define _ORA_DRV_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <oci.h>

struct _ora_drv_storage
{
  OCIEnv *envhp;
  OCIServer *srvhp;
  OCISvcCtx *svchp;
  OCIError *errhp;
  OCISession *authp;

  char *schema;                 /* which schema DSPAM objects reside under */
  struct _ds_spam_totals control_totals;        /* totals at storage init */

  /* control token data; used to measure deltas from getall to setall 
   * enabling us to code a sql query based on increments/decrements 
   * instead of query-coded data */

  unsigned long long control_token;     /* control token crc */
  long control_sh;              /* control token spam hits at getall */
  long control_ih;              /* control token innocent hits at getall */

  OCIStmt *iter_user;           /* get_nextuser iteration statement */
  OCIStmt *iter_token;          /* get_nexttoken iteration statement */
  OCIStmt *iter_sig;            /* get_nextsignature iteration statement */

};

/* Driver-specific functions */

int	_ora_drv_get_spamtotals	(DSPAM_CTX * CTX);
int	_ora_drv_set_spamtotals	(DSPAM_CTX * CTX);
struct passwd *_ora_drv_getpwnam(DSPAM_CTX * CTX, const char *name);
struct passwd *_ora_drv_getpwuid(DSPAM_CTX * CTX, uid_t uid);
void	_ora_drv_query_error	(const char *error, const char *query);
sword	_ora_drv_checkerr	(const char *query, 
                                 OCIError * errhp, 
                                 sword status);

#ifdef VIRTUAL_USERS
struct passwd *_ora_drv_setpwnam(DSPAM_CTX * CTX, const char *name);
#endif

#endif /* _ORA_DRV_H */
