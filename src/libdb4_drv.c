/* $Id: libdb4_drv.c,v 1.9 2005/04/22 21:11:35 jonz Exp $ */

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

/*
 * libdb4_drv.c - Berkeley DB v3 storage driver *
 * DESCRIPTION
 *   Do not use this driver. BDB is terrible, and will likely screw you
 *   over royally at some point. Please consider using the SQLite driver
 *   before attempting to use BDB.
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <db.h>

#ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   ifdef HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif

#include "storage_driver.h"
#include "libdb4_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"

int
dspam_init_driver (DRIVER_CTX *DTX)
{
  return 0;
}

int
dspam_shutdown_driver (DRIVER_CTX *DTX)
{
  return 0;
}

int
_libdb4_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  DBT key, data;
  char hashkey[32];
  int ret;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;

  if (s->db == NULL)
    return EINVAL;

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  strcpy (hashkey, "_TOTALS");
  key.data = hashkey;
  key.size = strlen (hashkey);

  ret = (*s->db->get) (s->db, NULL, &key, &data, 0);
  if (ret)
  {
    if (ret == DB_RUNRECOVERY)
    {
      if (!_libdb4_drv_recover (CTX, 1))
      {
        return _libdb4_drv_get_spamtotals (CTX);
      }
      else
      {
        return EUNKNOWN;
      }
    }
    else
    {
      LOGDEBUG ("_ds_get_totals: db->get failed: %s", db_strerror (ret));
      return EFAILURE;
    }
  }

  memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
  memcpy (&CTX->totals, data.data, data.size);

  return 0;
}

int
_libdb4_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  DBT key, data;
  char hashkey[32];
  int ret;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;

  if (s->db == NULL)
    return EINVAL;

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _libdb4_drv_get_spamtotals (CTX);   /* undo changes to in memory totals */
    return 0;
  }

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  strcpy (hashkey, "_TOTALS");
  key.data = hashkey;
  key.size = strlen (hashkey);
  data.data = &CTX->totals;
  data.size = sizeof (struct _ds_spam_totals);

  ret = (*s->db->put) (s->db, NULL, &key, &data, 0);
  if (ret)
  {
    if (ret == DB_RUNRECOVERY)
    {
      if (!_libdb4_drv_recover (CTX, 1))
      {
        return _libdb4_drv_set_spamtotals (CTX);
      }
      else
      {
        return EUNKNOWN;
      }
    }
    else
    {
      LOGDEBUG ("_ds_set_totals: db->set failed: %s", db_strerror (ret));
      return EFAILURE;
    }
  }

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  struct _ds_spam_stat stat;
  int ret = 0, x = 0;

  if (diction == NULL || CTX == NULL)
    return EINVAL;

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    ds_term->s.spam_hits = 0;
    ds_term->s.innocent_hits = 0;
    x = _ds_get_spamrecord (CTX, ds_term->key, &stat);
    if (!x)
    {
      ds_diction_setstat(diction, ds_term->key, &stat);
      if (ds_term->s.spam_hits > CTX->totals.spam_learned)
        ds_term->s.spam_hits = CTX->totals.spam_learned;
      if (ds_term->s.innocent_hits > CTX->totals.innocent_learned)
        ds_term->s.innocent_hits = CTX->totals.innocent_learned;
    }
    else if (x != EFAILURE)
    {
      ret = x;
    }

    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);

  if (ret) {
    LOGDEBUG("_ds_getall_spamtotals returning %d", ret);
  }

  return ret;
}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  int ret = EUNKNOWN;

  if (diction == NULL || CTX == NULL)
    return EINVAL;

  if (CTX->operating_mode == DSM_CLASSIFY &&
        (CTX->training_mode != DST_TOE ||
          (diction->whitelist_token == 0 && (!(CTX->flags & DSF_NOISE)))))
  {
    return 0;
  }
                                                                                
  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    if (CTX->training_mode == DST_TOE           &&
        CTX->classification == DSR_NONE         &&
        CTX->operating_mode == DSM_CLASSIFY     &&
        diction->whitelist_token != ds_term->key  &&
        (!ds_term->name || strncmp(ds_term->name, "bnr.", 4)))
    {
      ds_term = ds_diction_next(ds_c);
      continue;
    }

    if (!_ds_set_spamrecord (CTX, ds_term->key, &ds_term->s))
      ret = 0;
    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);

  return ret;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _libdb4_drv_spam_record record;
  unsigned long long value = token;
  DBT key, data;
  int ret;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;

  if (s->db == NULL)
    return EINVAL;

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  key.data = &value;
  key.size = sizeof (unsigned long long);

  ret = (*s->db->get) (s->db, NULL, &key, &data, 0);

  if (ret)
  {
    if (ret == DB_RUNRECOVERY)
    {
      if (!_libdb4_drv_recover (CTX, 1))
      {
        return _ds_get_spamrecord (CTX, token, stat);
      }
      else
      {
        LOGDEBUG("recovery failure");
        return EUNKNOWN;
      }
    }
    else
    {
      return EFAILURE;
    }
  }

  if (data.size == sizeof (struct _libdb4_drv_spam_record))
  {
    memcpy (&record, data.data, data.size);
    stat->spam_hits = record.spam_hits;
    stat->innocent_hits = record.innocent_hits;
  }
  else
  {
    LOG (LOG_WARNING,
         "_ds_get_spamrecord: record size (%d) doesn't match sizeof(struct _libdb4_drv_spam_record) (%d)",
         data.size, sizeof (struct _libdb4_drv_spam_record));
    return EUNKNOWN;
  }

  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _libdb4_drv_spam_record record;
  unsigned long long value = token;
  DBT key, data;
  int ret;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;

  if (s->db == NULL)
    return EINVAL;

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  if (CTX->_process_start == 0)
    CTX->_process_start = time (NULL);

  record.spam_hits = (stat->spam_hits > 0) ? stat->spam_hits : 0;
  record.innocent_hits = (stat->innocent_hits > 0) ? stat->innocent_hits : 0;
  record.last_hit = CTX->_process_start;

  key.data = &value;
  key.size = sizeof (unsigned long long);
  data.data = &record;
  data.size = sizeof (struct _libdb4_drv_spam_record);

  ret = (*s->db->put) (s->db, NULL, &key, &data, 0);

  if (ret)
  {
    if (ret == DB_RUNRECOVERY)
    {
      if (!_libdb4_drv_recover (CTX, 1))
      {
        return _ds_set_spamrecord (CTX, token, stat);
      }
      else
      {
        return EUNKNOWN;
      }
    }
    else
    {
      LOGDEBUG ("_ds_set_spamrecord: db->put failed: %s", db_strerror (ret));
      return EFAILURE;
    }
  }

  return 0;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _libdb4_drv_storage *s = NULL;
  int dbflag = DB_CREATE;
  static int recovery = 0;

  if (CTX == NULL)
    return EINVAL;

  if (!CTX->home) {
    LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
    return EINVAL;
  }

  if (dbh != NULL) {
    LOG(LOG_ERR, ERR_DRV_NO_ATTACH);
    return EINVAL;
  }

  if (CTX->flags & DSF_MERGED) {
    LOG(LOG_ERR, ERR_DRV_NO_MERGED);
    return EINVAL;
  }

  if (!recovery)
  {
    /* don't init if we're already initted */
    if (CTX->storage != NULL)
      return EINVAL;

    if (CTX->operating_mode == DSM_CLASSIFY)
      dbflag = DB_RDONLY;

    s = malloc (sizeof (struct _libdb4_drv_storage));
    if (s == NULL)
    {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      LOG (LOG_CRIT, ERR_MEM_ALLOC);
      return EUNKNOWN;
    }

    s->db = NULL;
    s->sig = NULL;
    s->env = NULL;

  }
  else
  {
    LOGDEBUG ("entering recovery mode");
    free (CTX->storage);
    s = malloc (sizeof (struct _libdb4_drv_storage));
    if (s == NULL)
    {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      LOG (LOG_CRIT, ERR_MEM_ALLOC);
      return EUNKNOWN;
    }

    s->db = NULL;
    s->sig = NULL;
    s->env = NULL;

  }

  if (CTX->username != NULL)
  {
    char db_home[MAX_FILENAME_LENGTH];
    int lock_result;

    if (CTX->group == NULL)
    {
      _ds_userdir_path(s->dictionary, CTX->home, CTX->username, "dict");
      _ds_userdir_path(s->signature, CTX->home, CTX->username, "sig");
      _ds_userdir_path(db_home, CTX->home, CTX->username, NULL);
    }
    else
    {
      _ds_userdir_path(s->dictionary, CTX->home, CTX->group, "dict");
      _ds_userdir_path(s->signature, CTX->home, CTX->group, "sig");
      _ds_userdir_path(db_home, CTX->home, CTX->group, NULL);
    }

    _ds_prepare_path_for(s->dictionary);

    if ((CTX->result = db_env_create (&s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_env_create failed: %s",
           db_strerror (CTX->result));
      goto BAIL;
    }

/*
    if (s->env->
        set_cachesize (s->env, 0, 1024 * 1024 * DB_CACHESIZE_MB, 1) != 0)
    {
      LOG (LOG_WARNING, "DB_ENV->set_cachesize failed: %s",
           db_strerror (CTX->result));
    }
*/

    s->env->set_errfile(s->env, stderr);

    if (!recovery)
    {
      CTX->result =
        s->env->open (s->env, db_home, DB_CREATE | DB_INIT_LOCK | 
                      DB_INIT_MPOOL, 0660);
    }
    else if (recovery == 1)
    {
      CTX->result =
        s->env->open (s->env, db_home, DB_CREATE | DB_INIT_LOCK |
                      DB_INIT_MPOOL | DB_RECOVER_FATAL, 0660);
    }
    else
    {

      CTX->result =
        s->env->open (s->env, db_home,
                      DB_CREATE | DB_INIT_LOCK | 
                      DB_INIT_MPOOL | DB_RECOVER, 0660);
    }

    if (CTX->result)
    {
      if (recovery || CTX->result != DB_RUNRECOVERY)
      {
        LOG (LOG_WARNING, "DB_ENV->open (1) failed: %s: %s",
             db_home, db_strerror (CTX->result));
        goto BAIL;
      }
      else
      {
        recovery = 1;
        s->env->close(s->env, 0);
        free(s);
        return _ds_init_storage (CTX, NULL);
      }
    }

    if ((CTX->result = db_create (&s->db, s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_create failed: %s", db_strerror (CTX->result));
      s->db = NULL;
      goto BAIL;
    }

    if ((CTX->result =
         s->db->DBOPEN (s->db, NULL, s->dictionary, NULL,
                        DB_BTREE, dbflag, 0)) != 0)
    {
      if (recovery || CTX->result != DB_RUNRECOVERY)
      {
        LOG (LOG_WARNING, "db->open failed on error %d: %s: %s",
             CTX->result, s->dictionary, db_strerror (CTX->result));
        s->db = NULL;
        goto BAIL;
      }
      else
      {
        recovery = 1;
        s->env->close (s->env, 0);
        free(s);
        return _ds_init_storage (CTX, NULL);
      }
    }

    if ((CTX->result = db_create (&s->sig, s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_create failed: %s", db_strerror (CTX->result));
      s->sig = NULL;
      _ds_shutdown_storage (CTX);
      goto BAIL;
    }

    if ((CTX->result =
         s->sig->DBOPEN (s->sig, NULL, s->signature, NULL,
                         DB_BTREE, DB_CREATE, 0)) != 0)
    {
      if (recovery || CTX->result != DB_RUNRECOVERY)
      {
        LOG (LOG_WARNING, "db->open failed on error %d: %s: %s",
             CTX->result, s->signature, db_strerror (CTX->result));
        s->sig = NULL;
        _ds_shutdown_storage (CTX);
        goto BAIL;
      }
      else
      {
        recovery = 1;
        s->db->close (s->db, 0);
        s->env->close (s->env, 0);
        free(s);
        return _ds_init_storage (CTX, NULL);
      }
    }

    /* Obtain a lock for this user */

    lock_result =
      _libdb4_drv_lock_get (CTX, s, (CTX->group ==
                             NULL) ? CTX->username : CTX->group);
    if (lock_result < 0)
    {
      LOGDEBUG ("locking subsystem returned error");
      goto BAIL;
    }

  }
  else
  {
    s->db = NULL;
    s->sig = NULL;
    s->env = NULL;
  }

  s->c = NULL;
  s->dir_handles = nt_create (NT_INDEX);
  CTX->storage = s;

  if (CTX->username != NULL)
  {
    if (_libdb4_drv_get_spamtotals (CTX))
    {
      LOGDEBUG ("unable to load totals.  using zero values.");
      memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
    }
  }
  else
  {
    memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
  }

  if (recovery)
  {
    recovery = 0;
    _ds_shutdown_storage (CTX);
    return _ds_init_storage (CTX, NULL);
  }
  return 0;

BAIL:
  if (s->env) 
    s->env->close(s->env, 0);
  free(s);
  return EFAILURE;
    
}

int
_ds_shutdown_storage (DSPAM_CTX * CTX)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  int ret = 0;

  if (s == NULL)
    return EINVAL;

  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
    _libdb4_drv_set_spamtotals (CTX);
  }
  node_nt = c_nt_first (s->dir_handles, &c_nt);
  while (node_nt != NULL)
  {
    DIR *dir;
    dir = (DIR *) node_nt->ptr;
    closedir (dir);
    node_nt = c_nt_next (s->dir_handles, &c_nt);
  }

  nt_destroy (s->dir_handles);

  /* cursor from _ds_get_nexttoken */
  if (s->c)
    s->c->c_close (s->c);

  /* token database */
  if (s->db)
  {
    int lock_result;

    lock_result =
      _libdb4_drv_lock_free (s, (CTX->group ==
                             NULL) ? CTX->username : CTX->group);
    if (lock_result < 0)
    {
      LOGDEBUG ("locking subsystem returned error");
      return EUNKNOWN;
    }

    ret = s->db->close (s->db, 0);
    if (ret)
      return EUNKNOWN;
  }

  /* signature database */
  if (s->sig)
    ret = s->sig->close (s->sig, 0);

  if (s && s->env)
  {
    ret = s->env->close (s->env, 0);
  }

  if (!ret)
  {
    free (CTX->storage);
    CTX->storage = NULL;
  }

  return (!ret) ? 0 : EUNKNOWN;
}

int
_ds_create_signature_id (DSPAM_CTX * CTX, char *buf, size_t len)
{
  char session[64];
  char digit[6];
  int pid, j;

  pid = getpid ();
  snprintf (session, sizeof (session), "%8lx%d", (long) time (NULL), pid);

  for (j = 0; j < 2; j++)
  {
    snprintf (digit, 6, "%d", rand ());
    strlcat (session, digit, 64);
  }

  strlcpy (buf, session, len);
  return 0;
}

int
_ds_get_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;
  char *hashkey;
  void *mem;
  int ret;

  hashkey = malloc (strlen (signature) + 1);
  if (!hashkey)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  memcpy (hashkey, signature, strlen (signature) + 1);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  key.data = hashkey;
  key.size = strlen (hashkey);

  ret = (*s->sig->get) (s->sig, NULL, &key, &data, 0);

  if (ret)
  {
    free (hashkey);
    LOGDEBUG("_ds_get_signature failure");
    return EFAILURE;
  }

  mem = (void *) calloc (1, data.size - sizeof (time_t));
  if (mem == NULL)
  {
    free (hashkey);
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }
  memcpy (mem, (char *) data.data + sizeof (time_t),
          data.size - sizeof (time_t));
  SIG->data = mem;
  SIG->length = data.size - sizeof (time_t);

  free (hashkey);
  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;
  char *hashkey;
  void *mem;
  int ret;
  time_t t = time (NULL);

  hashkey = malloc (strlen (signature) + 1);
  if (!hashkey)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  memcpy (hashkey, signature, strlen (signature) + 1);
  mem = malloc (SIG->length + sizeof (time_t));
  if (mem == NULL)
  {
    free (hashkey);
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  memcpy (mem, &t, sizeof (time_t));
  memcpy ((char *) mem + sizeof (time_t), SIG->data, SIG->length);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  key.data = hashkey;
  key.size = strlen (hashkey);
  data.data = mem;
  data.size = SIG->length + sizeof (time_t);

  ret = (*s->sig->put) (s->sig, NULL, &key, &data, 0);

  if (ret)
  {
    free (hashkey);
    free (mem);
    LOG (LOG_WARNING, "set_signature: sig->put failed: %s",
         db_strerror (ret));
    return EFAILURE;
  }

  free (hashkey);
  free (mem);
  return 0;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;
  char *hashkey;
  int ret;

  hashkey = malloc (strlen (signature) + 1);
  if (!hashkey)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  LOGDEBUG ("deleting signature %s", signature);
  memcpy (hashkey, signature, strlen (signature) + 1);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  key.data = hashkey;
  key.size = strlen (hashkey);

  ret = (*s->sig->del) (s->sig, NULL, &key, 0);

  if (ret)
  {
    free (hashkey);
    LOGDEBUG ("delete_signature: sig->del failed: %s", db_strerror (ret));
    return EFAILURE;
  }

  free (hashkey);
  return 0;
}

/**
 *  @return zero on success or non-zero if signature not found or
 *  something fails in the underlining database.
 */
int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;
  char *hashkey;
  int rc;
  size_t signature_len;

  if (s->sig == NULL)
    return 0;

  signature_len = strlen (signature);

  hashkey = malloc (signature_len + 1);
  if (!hashkey)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  memcpy (hashkey, signature, signature_len + 1);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  key.data = hashkey;
  key.size = signature_len;

  rc = ((*s->sig->get) (s->sig, NULL, &key, &data, 0));
  free (hashkey);
  return rc;
}

char *
_ds_get_nextuser (DSPAM_CTX * CTX)
{
  static char user[MAX_FILENAME_LENGTH];
  static char path[MAX_FILENAME_LENGTH];
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  struct nt_node *node_nt, *prev;
  struct nt_c c_nt;
  char *x = NULL, *y;
  DIR *dir = NULL;

  struct dirent *entry;

  if (s->dir_handles->items == 0)
  {
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, MAX_FILENAME_LENGTH, "%s/data", CTX->home);
    dir = opendir (filename);
    if (dir == NULL)
    {
      LOG (LOG_WARNING,
           "unable to open directory '%s' for reading: %s",
           CTX->home, strerror (errno));
      return NULL;
    }

    nt_add (s->dir_handles, (void *) dir);
    strlcpy (path, filename, sizeof (path));
  }
  else
  {
    node_nt = c_nt_first (s->dir_handles, &c_nt);
    while (node_nt != NULL)
    {
      if (node_nt->next == NULL)
        dir = (DIR *) node_nt->ptr;
      node_nt = c_nt_next (s->dir_handles, &c_nt);
    }
  }

  while ((entry = readdir (dir)) != NULL)
  {
    struct stat st;
    char filename[MAX_FILENAME_LENGTH];
    snprintf (filename, sizeof (filename), "%s/%s", path, entry->d_name);

    if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
      continue;

    if (stat (filename, &st)) {
      continue;
    }

    /* push a new directory */
    if (st.st_mode & S_IFDIR)
    {
      DIR *ndir;

      ndir = opendir (filename);
      if (ndir == NULL)
        continue;
      strlcat (path, "/", sizeof (path));
      strlcat (path, entry->d_name, sizeof (path));
      nt_add (s->dir_handles, (void *) ndir);
      return _ds_get_nextuser (CTX);
    }
    else if (!strncmp
             (entry->d_name + strlen (entry->d_name) - 5, ".dict", 5))
    {
      strlcpy (user, entry->d_name, sizeof (user));
      user[strlen (user) - 5] = 0;
      return user;
    }
  }

  /* pop current directory */
  y = strchr (path, '/');
  while (y != NULL)
  {
    x = y;
    y = strchr (x + 1, '/');
  }
  if (x)
    x[0] = 0;

  /* pop directory handle from list */
  node_nt = c_nt_first (s->dir_handles, &c_nt);
  prev = NULL;
  while (node_nt != NULL)
  {
    if (node_nt->next == NULL)
    {
      dir = (DIR *) node_nt->ptr;
      closedir (dir);
      if (prev != NULL) {
        prev->next = NULL;
        s->dir_handles->insert = NULL;
      }
      else
        s->dir_handles->first = NULL;
      free (node_nt);
      s->dir_handles->items--;
      prev = node_nt;
      break;
    }
    prev = node_nt;
    node_nt = c_nt_next (s->dir_handles, &c_nt);
  }
  if (s->dir_handles->items > 0)
    return _ds_get_nextuser (CTX);

  /* done */

  user[0] = 0;
  return NULL;
}

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _ds_storage_record *sr;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;

  sr = malloc (sizeof (struct _ds_storage_record));
  if (sr == NULL)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->c == NULL)
    s->db->cursor (s->db, NULL, &s->c, 0);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  while (s->c->c_get (s->c, &key, &data, DB_NEXT) == 0 && key.size < 64)
  {
    struct _libdb4_drv_spam_record record;
    char keyname[64];

    memcpy (&keyname, key.data, key.size);
    keyname[key.size] = 0;
    if (strcmp (keyname, "_TOTALS"))
    {
      snprintf (keyname, sizeof (keyname), "%llu",
                *(unsigned long long *) key.data);
      memcpy (&record, data.data, sizeof (struct _libdb4_drv_spam_record));
      sr->token = *(unsigned long long *) key.data;
      sr->spam_hits = record.spam_hits;
      sr->innocent_hits = record.innocent_hits;
      sr->last_hit = record.last_hit;
      return sr;
    }
  }

  s->c->c_close (s->c);
  s->c = NULL;
  return NULL;
}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  struct _ds_storage_signature *ss;
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  DBT key, data;

  ss = malloc (sizeof (struct _ds_storage_signature));
  if (ss == NULL)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->c == NULL)
    s->sig->cursor (s->sig, NULL, &s->c, 0);

  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));

  while (s->c->c_get (s->c, &key, &data, DB_NEXT) == 0)
  {
    ss->data = (void *) calloc (1, data.size - sizeof (time_t));
    if (ss->data == NULL)
    {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      LOG (LOG_CRIT, ERR_MEM_ALLOC);
      free (ss);
      return NULL;
    }

    memcpy (ss->signature, key.data, key.size);
    ss->signature[key.size] = 0;
    ss->length = data.size - sizeof (time_t);
    memcpy (ss->data, (char *) data.data + sizeof (time_t),
            data.size - sizeof (time_t));
    memcpy (&ss->created_on, data.data, sizeof (time_t));
    return ss;
  }

  s->c->c_close (s->c);
  s->c = NULL;
  return NULL;
}

/* Returns: 0: Lock acquired
*/

int
_libdb4_drv_lock_get (DSPAM_CTX *CTX, struct _libdb4_drv_storage *s, const char *username)
{
  char filename[MAX_FILENAME_LENGTH];
  int r;

  _ds_userdir_path(filename, CTX->home, username, "dict");

  s->lock = fopen(filename, "a");
  r = _ds_get_fcntl_lock(fileno(s->lock));
  if (r)
    fclose(s->lock);
  return r;
}

int
_libdb4_drv_lock_free (struct _libdb4_drv_storage *s, const char *username)
{
  int r;

  r = _ds_free_fcntl_lock(fileno(s->lock));
  if (!r)
    fclose(s->lock);

  return r;
}

int
_libdb4_drv_recover (DSPAM_CTX * CTX, int fatal)
{
  struct _libdb4_drv_storage *s = NULL;
  int dbflag = DB_CREATE;

  LOGDEBUG ("recovering database");

  if (CTX == NULL)
    return EINVAL;

  _ds_shutdown_storage (CTX);

  s = malloc (sizeof (struct _libdb4_drv_storage));
  if (s == NULL)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s->db = NULL;
  s->sig = NULL;
  s->env = NULL;

  if (CTX->username != NULL)
  {
    char db_home[MAX_FILENAME_LENGTH];

    if (CTX->group == NULL)
    {
      _ds_userdir_path(s->dictionary, CTX->home, CTX->username, "dict");
      _ds_userdir_path(s->signature, CTX->home, CTX->username, "sig");
      _ds_userdir_path(db_home, CTX->home, CTX->username, NULL);
    }
    else
    {
      _ds_userdir_path(s->dictionary, CTX->home, CTX->group, "dict");
      _ds_userdir_path(s->signature, CTX->home, CTX->group, "sig");
      _ds_userdir_path(db_home, CTX->home, CTX->group, NULL);
    }

    _ds_prepare_path_for(s->dictionary);

    if ((CTX->result = db_env_create (&s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_env_create failed: %s",
           db_strerror (CTX->result));
      goto BAIL;
    }

    if (fatal)
    {
      CTX->result =
        s->env->open (s->env, db_home,
                      DB_CREATE | DB_INIT_MPOOL | DB_INIT_LOCK |
                      DB_RECOVER_FATAL, 0660);
    }
    else
    {
      CTX->result =
        s->env->open (s->env, db_home,
                      DB_CREATE | DB_INIT_MPOOL | DB_INIT_LOCK |
                      DB_RECOVER, 0660);
    }

    if (CTX->result)
    {
      LOG (LOG_WARNING, "DB_ENV->open (2) failed: %s", db_strerror (CTX->result));
      goto BAIL;
    }

    if ((CTX->result = db_create (&s->db, s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_create failed: %s", db_strerror (CTX->result));
      s->db = NULL;
      goto BAIL;
    }

    if ((CTX->result =
         s->db->DBOPEN (s->db, NULL, s->dictionary, NULL,
                        DB_BTREE, dbflag, 0)) != 0)
    {
      LOG (LOG_WARNING, "db->open failed on error %d: %s: %s",
           CTX->result, s->dictionary, db_strerror (CTX->result));
      s->db = NULL;
      goto BAIL;
    }

    if ((CTX->result = db_create (&s->sig, s->env, 0)) != 0)
    {
      LOG (LOG_WARNING, "db_create failed: %s", db_strerror (CTX->result));
      s->sig = NULL;
      _ds_shutdown_storage (CTX);
      goto BAIL;
    }

    if ((CTX->result =
         s->sig->DBOPEN (s->sig, NULL, s->signature, NULL,
                         DB_BTREE, DB_CREATE, 0)) != 0)
    {
      LOG (LOG_WARNING, "db->open failed on error %d: %s: %s",
           CTX->result, s->signature, db_strerror (CTX->result));
      s->sig = NULL;
      _ds_shutdown_storage (CTX);
      goto BAIL;
    }

  }
  else
  {
    s->db = NULL;
    s->sig = NULL;
    s->env = NULL;
  }

  s->c = NULL;
  s->dir_handles = nt_create (NT_INDEX);
  CTX->storage = s;

  _ds_shutdown_storage (CTX);
  _ds_init_storage (CTX, NULL);
  return 0;

BAIL: 
  if (s->env)
    s->env->close(s->env, 0);
  free(s);
  return EFAILURE;
}

int
_ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token)
{
  struct _libdb4_drv_storage *s = (struct _libdb4_drv_storage *) CTX->storage;
  unsigned long long value = token;
  DBT key, data;
  int ret;
                                                                                
  memset (&key, 0, sizeof (DBT));
  memset (&data, 0, sizeof (DBT));
                                                                                
  key.data = &value;
  key.size = sizeof (unsigned long long);
                                                                                
  ret = (*s->db->del) (s->db, NULL, &key, 0);
                                                                                
  if (ret)
  {
    LOGDEBUG ("del_spamrecord: sig->del failed: %s", db_strerror (ret));
    return EFAILURE;
  }
                                                                                
  return 0;
}
                                                                                
int
_ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  int ret = 0, x = 0;

  if (diction == NULL || CTX == NULL)
    return EINVAL;

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    x = _ds_del_spamrecord (CTX, ds_term->key);
    if (x)
      ret = x;
                                                                                
    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);
                                                                                
  return ret;
}

void *_ds_connect (DSPAM_CTX *CTX)
{
  return NULL;
}
