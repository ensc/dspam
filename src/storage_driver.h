/* $Id: storage_driver.h,v 1.7 2004/12/24 17:24:25 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2004 NETWORK DWEEBS CORPORATION

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

#ifndef _STORAGE_DRIVER_H
#  define _STORAGE_DRIVER_H

#include "libdspam_objects.h"
#include "lht.h"
#ifdef PREFERENCES_EXTENSION
#include "pref.h"
#include "config_shared.h"
#endif
#ifdef DAEMON
#include <pthread.h>
#endif

struct _ds_drv_connection
{
  void *dbh;
#ifdef DAEMON
  pthread_mutex_t lock;
#endif
};

typedef struct {
  DSPAM_CTX *CTX;				/* IN */
  int status;					/* OUT */
  int flags;					/* IN */
  int connection_cache;				/* IN */
  struct _ds_drv_connection **connections;	/* OUT */
} DRIVER_CTX;

struct _ds_storage_record
{
  unsigned long long token;
  long spam_hits;
  long innocent_hits;
  time_t last_hit;
};

struct _ds_storage_signature
{
  char signature[256];
  void *data;
  long length;
  time_t created_on;
};

struct _ds_storage_decision
{
  char signature[256];
  void *data;
  long length; 
  time_t created_on;
};

/* dspam_init_driver: called by the application to initialize the storage
       driver.  should only be called once. */

/* dspam_shutdown_driver: called by the application to shutdown the storage
       driver.  should only be called once. */

/* _ds_init_storage: called prior to any storage calls; 
       opens database and performs any necessary locking.
       this function is performed by dspam_init */

/* _ds_shutdown_storage: called after all storage calls 
       have completed; closes database and unlocks. 
       this function is performed by dspam_init */

int dspam_init_driver (DRIVER_CTX *DTX);
int dspam_shutdown_driver (DRIVER_CTX *DTX);
int _ds_init_storage (DSPAM_CTX * CTX, void *dbh);
int _ds_shutdown_storage (DSPAM_CTX * CTX);

/* Standardized database calls */
int _ds_getall_spamrecords (DSPAM_CTX * CTX, struct lht *lht);
int _ds_setall_spamrecords (DSPAM_CTX * CTX, struct lht *lht);
int _ds_delall_spamrecords (DSPAM_CTX * CTX, struct lht *lht);

int _ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                        struct _ds_spam_stat *stat);
int _ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                        struct _ds_spam_stat *stat);
int _ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token);

/* Iteration calls */
struct _ds_storage_record *_ds_get_nexttoken (DSPAM_CTX * CTX);
struct _ds_storage_signature *_ds_get_nextsignature (DSPAM_CTX * CTX);
char *_ds_get_nextuser (DSPAM_CTX * CTX);

/* Signature processing calls */
int _ds_delete_signature (DSPAM_CTX * CTX, const char *signature);
int _ds_get_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                       const char *signature);
int _ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                       const char *signature);
int _ds_verify_signature (DSPAM_CTX * CTX, const char *signature);
int _ds_create_signature_id (DSPAM_CTX * CTX, char *buf, size_t len);

/* Neural network calls */
int _ds_get_node(DSPAM_CTX * CTX, char *user, struct _ds_neural_record *node);
int _ds_set_node(DSPAM_CTX * CTX, char *user, struct _ds_neural_record *node);

int _ds_get_decision (DSPAM_CTX * CTX, struct _ds_neural_decision *DEC,
                      const char *signature);
int _ds_set_decision (DSPAM_CTX * CTX, struct _ds_neural_decision *DEC,
                      const char *signature);
int _ds_delete_decision (DSPAM_CTX * CTX, const char *signature);
void *_ds_connect (DSPAM_CTX *CTX);

/*
   Storage Driver Preferences Extension
   When defined, the built-in preferences functions are overridden with
   functions specific to the storage driver. This allows preferences to be
   alternatively stored in the storage facility instead of flat files. 
*/

#ifdef PREFERENCES_EXTENSION
AGENT_PREF	_ds_pref_load(attribute_t **config, const char *user,
                              const char *home, void *dbh);
int		_ds_pref_save(attribute_t **config, const char *user, 
                              const char *home, AGENT_PREF PTX, void *dbh);

int	_ds_pref_set(attribute_t **config, const char *user, const char *home,
                     const char *attrib, const char *value, void *dbh);
int	_ds_pref_del(attribute_t **, const char *user, const char *home, 
                     const char *attrib, void *dbh);
#endif

/* Driver context flags */

#define DRF_STATEFUL	0x01

/* Driver statii */

#define DRS_ONLINE	0x01
#define DRS_OFFLINE	0x02
#define DRS_UNKNOWN	0xFF

#endif /* _STORAGE_DRIVER_H */

