/* $Id: ora_drv.c,v 1.5 2004/12/01 17:29:11 jonz Exp $ */

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

#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <oci.h>

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
#include "ora_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"
#include "lht.h"
#include "config_shared.h"

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
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _ora_drv_storage *s;
  FILE *file;
  char dblink[1024];
  char buffer[1024];
  char username[64] = "";
  char password[32] = "";
  char schema[32] = "";
  char filename[MAX_FILENAME_LENGTH];
  int i = 0;

  if (CTX == NULL)
    return EINVAL;
                                                                                
  if (dbh != NULL) {
    report_error (ERROR_NO_ATTACH);
    return EINVAL;
  }

  if (CTX->flags & DSF_MERGED) {
    report_error (ERROR_NO_MERGED);
    return EINVAL;
  }

  /* don't init if we're already initted */
  if (CTX->storage != NULL)
  {
    LOGDEBUG ("_ds_init_storage: storage already initialized");
    return EINVAL;
  }

  s = malloc (sizeof (struct _ora_drv_storage));
  if (s == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s->control_token = 0;
  s->control_ih = 0;
  s->control_sh = 0;
  s->authp = NULL;
  s->schema = NULL;
  s->iter_user = NULL;
  s->iter_token = NULL;
  s->iter_sig = NULL;

  if (_ds_read_attribute(CTX->config->attributes, "OraServer")) {
    char *p;

    strlcpy(dblink,
            _ds_read_attribute(CTX->config->attributes, "OraServer"), 
            sizeof(dblink));

    if ((p = _ds_read_attribute(CTX->config->attributes, "OraUser")))
      strlcpy(username, p, sizeof(username));

    if ((p = _ds_read_attribute(CTX->config->attributes, "OraPass")))
      strlcpy(password, p, sizeof(password));

    if ((p = _ds_read_attribute(CTX->config->attributes, "OraSchema")))
      strlcpy(schema, p, sizeof(schema));

  } else {
    snprintf (filename, MAX_FILENAME_LENGTH, "%s/oracle.data", CTX->home);
    file = fopen (filename, "r");
    if (file == NULL)
    {
      LOG (LOG_WARNING, "unable to open %s for reading: %s",
           filename, strerror (errno));
      return EFAILURE;
    }
  
    while (fgets (buffer, sizeof (buffer), file) != NULL)
    {
      chomp (buffer);
      if (!i)
        strlcpy (dblink, buffer, sizeof (dblink));
      else if (i == 1)
        strlcpy (username, buffer, sizeof (username));
      else if (i == 2)
        strlcpy (password, buffer, sizeof (password));
      else if (i == 3)
        strlcpy (schema, buffer, sizeof (schema));
      i++;
    }
    fclose (file);
  }

  if (schema[0] == 0)
  {
    LOG (LOG_WARNING, "file %s: incomplete Oracle connect data", filename);
    return EINVAL;
  }

  s->schema = strdup (schema);

  /* establish an Oracle session */

  OCIInitialize ((ub4) OCI_DEFAULT, (dvoid *) 0,
                 (dvoid * (*)(dvoid *, size_t)) 0,
                 (dvoid * (*)(dvoid *, dvoid *, size_t)) 0,
                 (void (*)(dvoid *, dvoid *)) 0);

  OCIEnvInit ((OCIEnv **) & s->envhp, OCI_DEFAULT, (size_t) 0, (dvoid **) 0);

  OCIHandleAlloc ((dvoid *) s->envhp, (dvoid **) & s->errhp, OCI_HTYPE_ERROR,
                  (size_t) 0, (dvoid **) 0);

  OCIHandleAlloc ((dvoid *) s->envhp, (dvoid **) & s->srvhp,
                  OCI_HTYPE_SERVER, (size_t) 0, (dvoid **) 0);

  OCIHandleAlloc ((dvoid *) s->envhp, (dvoid **) & s->svchp,
                  OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0);

  if (_ora_drv_checkerr
      (dblink, s->errhp,
       OCIServerAttach (s->srvhp, s->errhp, (text *) dblink, strlen (dblink),
                        0)) != OCI_SUCCESS)
    return EFAILURE;

  OCIAttrSet ((dvoid *) s->svchp, OCI_HTYPE_SVCCTX, (dvoid *) s->srvhp,
              (ub4) 0, OCI_ATTR_SERVER, (OCIError *) s->errhp);

  OCIHandleAlloc ((dvoid *) s->envhp, (dvoid **) & s->authp,
                  (ub4) OCI_HTYPE_SESSION, (size_t) 0, (dvoid **) 0);

  OCIAttrSet ((dvoid *) s->authp, (ub4) OCI_HTYPE_SESSION,
              (dvoid *) username, (ub4) strlen ((char *) username),
              (ub4) OCI_ATTR_USERNAME, s->errhp);

  OCIAttrSet ((dvoid *) s->authp, (ub4) OCI_HTYPE_SESSION,
              (dvoid *) password, (ub4) strlen ((char *) password),
              (ub4) OCI_ATTR_PASSWORD, s->errhp);

  if (_ora_drv_checkerr (NULL, s->errhp, OCISessionBegin (s->svchp, s->errhp,
                                                          s->authp,
                                                          OCI_CRED_RDBMS,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    return EFAILURE;

  (void) OCIAttrSet ((dvoid *) s->svchp, (ub4) OCI_HTYPE_SVCCTX,
                     (dvoid *) s->authp, (ub4) 0,
                     (ub4) OCI_ATTR_SESSION, s->errhp);

  CTX->storage = s;

  /* get spam totals on successful init */
  if (CTX->username != NULL)
  {
    if (_ora_drv_get_spamtotals (CTX))
    {
      LOGDEBUG ("unable to load totals.  using zero values.");
      memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
      memset (&s->control_totals, 0, sizeof (struct _ds_spam_totals));
    }
    else
    {
      memcpy (&s->control_totals, &CTX->totals,
              sizeof (struct _ds_spam_totals));
    }
  }
  else
  {
    memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
    memset (&s->control_totals, 0, sizeof (struct _ds_spam_totals));
  }

  return 0;
}

int
_ds_shutdown_storage (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: storage not initialized");
    return EINVAL;
  }

  /* Store spam totals on shutdown */
  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
    _ora_drv_set_spamtotals (CTX);
  }
  OCISessionEnd (s->svchp, s->errhp, s->authp, (ub4) OCI_DEFAULT);
  OCIServerDetach (s->srvhp, s->errhp, (ub4) OCI_DEFAULT);
  OCIHandleFree ((dvoid *) s->srvhp, (ub4) OCI_HTYPE_SERVER);
  OCIHandleFree ((dvoid *) s->svchp, (ub4) OCI_HTYPE_SVCCTX);
  OCIHandleFree ((dvoid *) s->errhp, (ub4) OCI_HTYPE_ERROR);
  OCIHandleFree ((dvoid *) s->envhp, OCI_HTYPE_ENV);

  free (s->schema);
  free (CTX->storage);
  CTX->storage = NULL;

  return 0;
}

void
_ora_drv_query_error (const char *error, const char *query)
{
  FILE *file;
  time_t tm = time (NULL);
  char ct[128];
  char fn[MAX_FILENAME_LENGTH];

  snprintf (fn, sizeof (fn), "%s/sql.errors", LOGDIR);

  snprintf (ct, sizeof (ct), "%s", ctime (&tm));
  chomp (ct);

  file = fopen (fn, "a");

  if (file == NULL)
  {
    file_error (ERROR_FILE_WRITE, fn, strerror (errno));
    return;
  }
  if (query != NULL)
    fprintf (file, "[%s] %d: %s: %s\n", ct, getpid (), error, query);
  else
    fprintf (file, "[%s] %d: %s\n", ct, getpid (), error);
  fclose (file);

#ifdef DEBUG
  if (query != NULL)
    fprintf (stderr, "[%s] %d: %s: %s\n", ct, getpid (), error, query);
  else
    fprintf (stderr, "[%s] %d: %s\n", ct, getpid (), error);
#endif

  return;
}

sword
_ora_drv_checkerr (const char *query, OCIError * errhp, sword status)
{
  text errbuf[512];
  sb4 errcode = 0;

  switch (status)
  {
  case OCI_SUCCESS:
    break;
  case OCI_SUCCESS_WITH_INFO:
    _ora_drv_query_error ("Oracle error: OCI_SUCCESS_WITH_INFO", query);
    break;
  case OCI_NEED_DATA:
    _ora_drv_query_error ("Oracle error: OCI_NEED_DATA", query);
    break;
  case OCI_NO_DATA:
    _ora_drv_query_error ("Oracle error: OCI_NO_DATA", query);
    break;
  case OCI_ERROR:
    (void) OCIErrorGet ((dvoid *) errhp, (ub4) 1, (text *) NULL, &errcode,
                        errbuf, (ub4) sizeof (errbuf), OCI_HTYPE_ERROR);
    _ora_drv_query_error ((const char *) errbuf, query);
    break;
  case OCI_INVALID_HANDLE:
    _ora_drv_query_error ("Oracle error: OCI_INVALID_HANDLE", query);
    break;
  case OCI_STILL_EXECUTING:
    _ora_drv_query_error ("Oracle error: OCI_STILL_EXECUTE", query);
    break;
  case OCI_CONTINUE:
    _ora_drv_query_error ("Oracle error: OCI_CONTINUE", query);
    break;
  default:
    break;
  }

  return status;
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
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  OCIStmt *stmthp;
  OCIDefine *defn = (OCIDefine *) 0;
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_get_signature: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_signature: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT LENGTH FROM %s.DSPAM_SIGNATURE_DATA"
            " WHERE SIGNATURE = '%s' AND USER_ID = %d",
            s->schema, signature, p->pw_uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query, s->errhp,
       OCIDefineByPos (stmthp, &defn, s->errhp, 1, (dvoid *) & SIG->length,
                       (sword) sizeof (long), SQLT_INT, (dvoid *) 0,
                       (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return EFAILURE;
  }

  if (status != OCI_SUCCESS)
    goto bail;

  SIG->data = malloc (SIG->length);
  if (SIG->data == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    goto bail;
  }

  snprintf (query, sizeof (query),
            "SELECT DATA FROM %s.DSPAM_SIGNATURE_DATA"
            " WHERE SIGNATURE = '%s' AND USER_ID = %d",
            s->schema, signature, p->pw_uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query, s->errhp,
       OCIDefineByPos (stmthp, &defn, s->errhp, 1, (dvoid *) SIG->data,
                       (sword) SIG->length, SQLT_LNG, (dvoid *) 0, (ub2 *) 0,
                       (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = _ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                               stmthp,
                                                               s->errhp,
                                                               (ub4) 1,
                                                               (ub4) 0,
                                                               (OCISnapshot *)
                                                               NULL,
                                                               (OCISnapshot *)
                                                               NULL,
                                                               (ub4)
                                                               OCI_DEFAULT));

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return EFAILURE;
  }

  if (status != OCI_SUCCESS)
    goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  OCIStmt *stmthp;
  OCIBind *phBindSignature = NULL;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_set_signature: storage not initialized");
    return EINVAL;
  }

  if (SIG == NULL || SIG->data == NULL || !SIG->length)
    return 0;

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_signature: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "INSERT INTO %s.DSPAM_SIGNATURE_DATA(USER_ID, SIGNATURE, LENGTH, CREATED_ON"
            ", DATA) VALUES(%d, '%s', %ld, SYSDATE, :signature)",
            s->schema, p->pw_uid, signature, SIG->length);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIBindByName (stmthp,
                                                         &phBindSignature,
                                                         s->errhp,
                                                         ":signature", -1,
                                                         (dvoid *) SIG->data,
                                                         SIG->length,
                                                         SQLT_LNG, NULL, NULL,
                                                         NULL, 0, NULL,
                                                         OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                          stmthp, s->errhp,
                                                          (ub4) 1, (ub4) 0,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  OCIStmt *stmthp;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_signature: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM %s.DSPAM_SIGNATURE_DATA"
            " WHERE SIGNATURE = '%s' AND USER_ID = %d",
            s->schema, signature, p->pw_uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                          stmthp, s->errhp,
                                                          (ub4) 1, (ub4) 0,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;
}

int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[1024], sig[32];
  struct passwd *p;
  OCIStmt *stmthp;
  OCIDefine *defn = (OCIDefine *) 0;
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT SIGNATURE FROM %s.DSPAM_SIGNATURE_DATA"
            " WHERE SIGNATURE = '%s' AND USER_ID = %d",
            s->schema, signature, p->pw_uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query, s->errhp,
       OCIDefineByPos (stmthp, &defn, s->errhp, 1, (dvoid *) & sig,
                       (sword) sizeof (sig), SQLT_STR, (dvoid *) 0, (ub2 *) 0,
                       (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return EFAILURE;
  }

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;
}

char *
_ds_get_nextuser (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  uid_t uid;
  static OCIDefine *defn = (OCIDefine *) 0;
  static char user[MAX_FILENAME_LENGTH] = { 0 };
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return NULL;
  }

  /* Execute initial query */
  if (s->iter_user == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT DISTINCT USER_ID FROM %s.DSPAM_STATS", s->schema);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) & s->
                                                           iter_user,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return NULL;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (s->iter_user,
                                                            s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_user, &defn, s->errhp, 1, (dvoid *) & uid,
                         (sword) sizeof (uid_t), SQLT_UIN, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                            s->iter_user,
                                                            s->errhp, (ub4) 0,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;
  }

  status =
    OCIStmtFetch (s->iter_user, s->errhp, (ub4) 1, (ub4) OCI_FETCH_NEXT,
                  (ub4) OCI_DEFAULT);

  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO)
    goto bail;

  p = _ora_drv_getpwuid (CTX, uid);
  if (p != NULL)
  {
    strlcpy (user, p->pw_name, sizeof (user));
    return user;
  }

bail:

  OCIHandleFree ((dvoid *) s->iter_user, (ub4) OCI_HTYPE_STMT);
  s->iter_user = NULL;
  return NULL;

}

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct _ds_storage_record *st;
  struct passwd *p;
  char query[1024], token_c[32];
  static OCIDefine *defn1 = NULL, *defn2 = NULL, *defn3 = NULL, *defn4 = NULL;
  long sh, ih, lh;
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return NULL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return NULL;
  }

  if (s->iter_token == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT TOKEN, SPAM_HITS, INNOCENT_HITS, "
              "TO_NUMBER(LAST_HIT-TO_DATE('01011970','DDMMYYYY'))*60*60*24"
              "  FROM %s.DSPAM_TOKEN_DATA WHERE USER_ID = %d", s->schema,
              p->pw_uid);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) & s->
                                                           iter_token,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return NULL;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (s->iter_token,
                                                            s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_token, &defn1, s->errhp, 1,
                         (dvoid *) & token_c, (sword) sizeof (token_c),
                         SQLT_STR, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0,
                         OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_token, &defn2, s->errhp, 2, (dvoid *) & sh,
                         (sword) sizeof (sh), SQLT_INT, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_token, &defn3, s->errhp, 3, (dvoid *) & ih,
                         (sword) sizeof (ih), SQLT_INT, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_token, &defn4, s->errhp, 4, (dvoid *) & lh,
                         (sword) sizeof (lh), SQLT_INT, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                            s->iter_token,
                                                            s->errhp, (ub4) 0,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;
  }

  status =
    OCIStmtFetch (s->iter_token, s->errhp, (ub4) 1, (ub4) OCI_FETCH_NEXT,
                  (ub4) OCI_DEFAULT);

  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO)
    goto bail;

  st = malloc (sizeof (struct _ds_storage_record));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  memset (st, 0, sizeof (struct _ds_storage_record));
  st->token = strtoull (token_c, NULL, 0);
  st->spam_hits = sh;
  st->innocent_hits = ih;
  st->last_hit = (time_t) lh;

  return st;

bail:

  OCIHandleFree ((dvoid *) s->iter_user, (ub4) OCI_HTYPE_STMT);
  s->iter_token = NULL;
  return NULL;

}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct _ds_storage_signature *st;
  struct passwd *p;
  char query[1024], signature[128];
  static OCIDefine *defn1 = NULL, *defn2 = NULL, *defn3 = NULL, *defn4 = NULL;
  long sl, lh;
  sword status;
  OCIStmt *stmthp;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return NULL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return NULL;
  }

  if (s->iter_sig == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT SIGNATURE, LENGTH, "
              "  TO_NUMBER(CREATED_ON-TO_DATE('01011970','MMDDYYYY'))*60*60*24"
              "  FROM %s.DSPAM_SIGNATURE_DATA WHERE USER_ID = %d",
              s->schema, p->pw_uid);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) & s->
                                                           iter_sig,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return NULL;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (s->iter_sig,
                                                            s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_sig, &defn1, s->errhp, 1,
                         (dvoid *) & signature, (sword) sizeof (signature),
                         SQLT_STR, (dvoid *) 0, (ub2 *) 0, (ub2 *) 0,
                         OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_sig, &defn2, s->errhp, 2, (dvoid *) & sl,
                         (sword) sizeof (sl), SQLT_INT, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr
        (query, s->errhp,
         OCIDefineByPos (s->iter_sig, &defn3, s->errhp, 3, (dvoid *) & lh,
                         (sword) sizeof (lh), SQLT_INT, (dvoid *) 0,
                         (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                            s->iter_sig,
                                                            s->errhp, (ub4) 0,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;
  }

  status = OCIStmtFetch (s->iter_sig, s->errhp, (ub4) 1, (ub4) OCI_FETCH_NEXT,
                         (ub4) OCI_DEFAULT);

  if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO)
    goto bail;

  st = malloc (sizeof (struct _ds_storage_signature));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    goto bail;
  }

  memset (st, 0, sizeof (struct _ds_storage_signature));

  st->length = sl;
  st->created_on = (time_t) lh;
  strlcpy (st->signature, signature, sizeof (st->signature));
  st->data = malloc (st->length);

  if (st->data == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    goto bail;
  }

  snprintf (query, sizeof (query),
            "SELECT DATA FROM %s.DSPAM_SIGNATURE_DATA WHERE USER_ID = %d"
            "  AND SIGNATURE = '%s'", s->schema, p->pw_uid, st->signature);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp,
                                                          s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    free (st);
    goto bail;
  }

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 1,
                                                          (dvoid *) & st->
                                                          data,
                                                          (sword) st->length,
                                                          SQLT_LNG,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    free (st);
    goto bail;
  }

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp,
                                                          stmthp, s->errhp,
                                                          (ub4) 0, (ub4) 0,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    free (st);
    goto bail;
  }
  else
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return st;
  }

bail:

  OCIHandleFree ((dvoid *) s->iter_sig, (ub4) OCI_HTYPE_STMT);
  s->iter_sig = NULL;
  return NULL;
}

struct passwd *
_ora_drv_getpwnam (DSPAM_CTX * CTX, const char *name)
{

#ifndef VIRTUAL_USERS
  static struct passwd p = { NULL, NULL, 0, 0, NULL, NULL, NULL };
  struct passwd *q;

  if (p.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (p.pw_name, name))
      return &p;

    free (p.pw_name);
    p.pw_name = NULL;
    p.pw_uid = 0;
  }

  q = getpwnam (name);
  if (q == NULL)
    return NULL;
  p.pw_uid = q->pw_uid;
  p.pw_name = strdup (q->pw_name);

  return &p;
#else
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  static struct passwd p = { NULL, NULL, 0, 0, NULL, NULL, NULL };
  char query[256];
  OCIStmt *stmthp;
  OCIDefine *defn = (OCIDefine *) 0;
  sword status;
  uid_t uid = 0;

  if (p.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (p.pw_name, name))
      return &p;

    free (p.pw_name);
    p.pw_name = NULL;
  }

  snprintf (query, sizeof (query), "SELECT USER_ID FROM %s."
            "DSPAM_VIRTUAL_USER_IDS WHERE USERNAME = '%s'", s->schema, name);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return NULL;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query, s->errhp,
       OCIDefineByPos (stmthp, &defn, s->errhp, 1, (dvoid *) & uid,
                       (sword) sizeof (uid_t), SQLT_UIN, (dvoid *) 0,
                       (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS)
      return NULL;
    return _ora_drv_setpwnam (CTX, name);
  }
  else if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO)
  {
    _ora_drv_checkerr (query, s->errhp, status);
    goto bail;
  }

  p.pw_uid = uid;
  p.pw_name = strdup (name);

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return &p;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return NULL;

#endif
}

struct passwd *
_ora_drv_getpwuid (DSPAM_CTX * CTX, uid_t uid)
{
#ifndef VIRTUAL_USERS
  static struct passwd p = { NULL, NULL, 0, 0, NULL, NULL, NULL };
  struct passwd *q;

  if (p.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (p.pw_uid == uid)
    {
      return &p;
    }
    p.pw_name = NULL;
  }

  q = getpwuid (uid);
  memcpy (&p, q, sizeof (struct passwd));

  return &p;
#else
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  static struct passwd p = { NULL, NULL, 0, 0, NULL, NULL, NULL };
  char query[256];
  char user[128];
  OCIStmt *stmthp;
  OCIDefine *defn = (OCIDefine *) 0;
  sword status;

  if (p.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (p.pw_uid == uid)
      return &p;
    free (p.pw_name);
    p.pw_name = NULL;
  }

  memset (user, 0, sizeof (user));

  snprintf (query, sizeof (query),
            "SELECT USERNAME FROM %s.DSPAM_VIRTUAL_USER_IDS WHERE USER_ID = %d",
            s->schema, uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return NULL;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query, s->errhp,
       OCIDefineByPos (stmthp, &defn, s->errhp, 1, (dvoid *) & user,
                       (sword) sizeof (user), SQLT_STR, (dvoid *) 0,
                       (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  if (status == OCI_NO_DATA)
    goto bail;

  p.pw_uid = uid;
  p.pw_name = strdup (user);

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return &p;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return NULL;
#endif
}

#ifdef VIRTUAL_USERS
struct passwd *
_ora_drv_setpwnam (DSPAM_CTX * CTX, const char *name)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[256];
  OCIStmt *stmthp;

  snprintf (query, sizeof (query),
            "INSERT INTO %s.DSPAM_VIRTUAL_USER_IDS (USER_ID, USERNAME) VALUES "
            "(%s.VIRTUAL_USER.NEXTVAL, '%s')", s->schema, s->schema, name);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return NULL;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp, stmthp,
                                                          s->errhp, (ub4) 1,
                                                          (ub4) 0,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (OCISnapshot *)
                                                          NULL,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return _ora_drv_getpwnam (CTX, name);

bail:
  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return NULL;

}
#endif

int
_ora_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  OCIStmt *stmthp;
  OCIDefine *defn1 = NULL, *defn2 = NULL, *defn3 = NULL, *defn4 = NULL;
  long ts, ti, sm, fp, sc, ic, sf, icf;
  int status;

  ts = 0;
  ti = 0;
  sm = 0;
  fp = 0;
  sc = 0;
  ic = 0;
  sf = 0;
  icf = 0;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ora_drv_get_spamtotals: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ora_drv_get_spamtotals: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT TOTAL_SPAM, TOTAL_INNOCENT, SPAM_MISSES, FALSE_POSITIVES, "
            " SPAM_CORPUSFED, INNOCENT_CORPUSFED, SPAM_CLASSIFIED, "
            " INNOCENT_CLASSIFIED "
            " FROM %s.DSPAM_STATS WHERE USER_ID = %d", s->schema, p->pw_uid);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn1,
                                                          s->errhp, 1,
                                                          (dvoid *) & ts,
                                                          (sword) sizeof (ts),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn2,
                                                          s->errhp, 2,
                                                          (dvoid *) & ti,
                                                          (sword) sizeof (ti),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn3,
                                                          s->errhp, 3,
                                                          (dvoid *) & sm,
                                                          (sword) sizeof (sm),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 4,
                                                          (dvoid *) & fp,
                                                          (sword) sizeof (fp),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 5,
                                                          (dvoid *) & sc,
                                                          (sword) sizeof (sc),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 6,
                                                          (dvoid *) & ic,
                                                          (sword) sizeof (ic),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 7,
                                                          (dvoid *) & sf,
                                                          (sword) sizeof (sf),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn4,
                                                          s->errhp, 7,
                                                          (dvoid *) & icf,
                                                          (sword) sizeof (icf),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;


  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return EFAILURE;
  }
  else if (status != OCI_SUCCESS && status != OCI_SUCCESS_WITH_INFO)
  {
    _ora_drv_checkerr (query, s->errhp, status);
    goto bail;
  }

  CTX->totals.spam_learned = ts;
  CTX->totals.innocent_learned = ti;
  CTX->totals.spam_misclassified = sm;
  CTX->totals.innocent_misclassified = fp;
  CTX->totals.spam_corpusfed = sc;
  CTX->totals.innocent_corpusfed = ic;
  CTX->totals.spam_classified = sf;
  CTX->totals.innocent_classified = icf;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;
}

int
_ora_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  OCIStmt *stmthp;
  int bad = 0;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ora_drv_set_spamtotals: storage not initialized");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _ora_drv_get_spamtotals (CTX);      /* undo changes to in memory totals */
    return 0;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ora_drv_get_spamtotals: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  if (s->control_totals.innocent_learned == 0)
  {
    snprintf (query, sizeof (query),
              "INSERT INTO %s.DSPAM_STATS(USER_ID, TOTAL_SPAM, TOTAL_INNOCENT,"
              "  SPAM_MISSES, FALSE_POSITIVES, SPAM_CORPUSFED, "
              "  INNOCENT_CORPUSFED, SPAM_CLASSIFIED, INNOCENT_CLASSIFIED) "
              "  VALUES(%d, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld)",
              s->schema,
              p->pw_uid, CTX->totals.spam_learned,
              CTX->totals.innocent_learned, CTX->totals.spam_misclassified,
              CTX->totals.innocent_misclassified,
              CTX->totals.spam_corpusfed, CTX->totals.innocent_corpusfed,
              CTX->totals.spam_classified, CTX->totals.innocent_classified);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp, stmthp,
                                                            s->errhp, (ub4) 1,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      bad = 1;
  }

  if (s->control_totals.innocent_learned != 0 || bad)
  {
    snprintf (query, sizeof (query),
              "UPDATE DSPAM_STATS SET TOTAL_SPAM = TOTAL_SPAM %s %d, "
              "TOTAL_INNOCENT = TOTAL_INNOCENT %s %d, "
              "SPAM_MISSES = SPAM_MISSES %s %d, "
              "FALSE_POSITIVES = FALSE_POSITIVES %s %d, "
              "SPAM_CORPUSFED = SPAM_CORPUSFED %s %d, "
              "INNOCENT_CORPUSFED = INNOCENT_CORPUSFED %s %d, "
              "SPAM_CLASSIFIED = SPAM_CLASSIFIED %s %d, "
              "INNOCENT_CLASSIFIED = INNOCENT_CLASSIFIED %s %d "
              "WHERE USER_ID = %d",
              (CTX->totals.spam_learned >
               s->control_totals.spam_learned) ? "+" : "-",
              abs (CTX->totals.spam_learned -
                   s->control_totals.spam_learned),
              (CTX->totals.innocent_learned >
               s->control_totals.innocent_learned) ? "+" : "-",
              abs (CTX->totals.innocent_learned -
                   s->control_totals.innocent_learned),
              (CTX->totals.spam_misclassified >
               s->control_totals.spam_misclassified) ? "+" : "-",
              abs (CTX->totals.spam_misclassified -
                   s->control_totals.spam_misclassified),
              (CTX->totals.innocent_misclassified >
               s->control_totals.innocent_misclassified) ? "+" : "-",
              abs (CTX->totals.innocent_misclassified -
                   s->control_totals.innocent_misclassified),
              (CTX->totals.spam_corpusfed >
               s->control_totals.spam_corpusfed) ? "+" : "-",
              abs (CTX->totals.spam_corpusfed -
                   s->control_totals.spam_corpusfed),
              (CTX->totals.innocent_corpusfed >
               s->control_totals.innocent_corpusfed) ? "+" : "-",
              abs (CTX->totals.innocent_corpusfed -
                   s->control_totals.innocent_corpusfed), 
              (CTX->totals.spam_classified >
               s->control_totals.spam_classified) ? "+" : "-",
              abs (CTX->totals.spam_classified -
                   s->control_totals.spam_classified),
              (CTX->totals.innocent_classified >
               s->control_totals.innocent_classified) ? "+" : "-",
              abs (CTX->totals.innocent_classified -
                   s->control_totals.innocent_classified), p->pw_uid);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp, stmthp,
                                                            s->errhp, (ub4) 1,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;
  }

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;

}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  buffer *query;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  char scratch[1024], token_c[32] = { 0 };
  struct _ds_spam_stat stat;
  OCIStmt *stmthp;
  OCIDefine *defn1 = NULL, *defn2 = NULL, *defn3 = NULL;
  unsigned long long token = 0;
  long sh = 0, ih = 0;
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  stat.spam_hits = 0;
  stat.innocent_hits = 0;

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (scratch, sizeof (scratch),
            "SELECT TOKEN, SPAM_HITS, INNOCENT_HITS FROM %s.DSPAM_TOKEN_DATA WHERE "
            "  USER_ID = %d AND TOKEN IN(", s->schema, p->pw_uid);
  buffer_cat (query, scratch);
  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    snprintf (scratch, sizeof (scratch), "%llu", node_lht->key);
    buffer_cat (query, scratch);
    node_lht->s.innocent_hits = 0;
    node_lht->s.spam_hits = 0;
    node_lht->s.probability = 0;
    node_lht->s.status &= ~TST_DISK;
    node_lht = c_lht_next (freq, &c_lht);
    if (node_lht != NULL)
      buffer_cat (query, ",");
  }
  buffer_cat (query, ")");
#ifdef VERBOSE
  LOGDEBUG ("oracle query length: %ld\n", query->used);
#endif

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr
      (query->data, s->errhp,
       OCIStmtPrepare (stmthp, s->errhp, (text *) query->data,
                       (ub4) strlen (query->data), (ub4) OCI_NTV_SYNTAX,
                       (ub4) OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query->data, s->errhp,
       OCIDefineByPos (stmthp, &defn1, s->errhp, 1, (dvoid *) & token_c,
                       (sword) sizeof (token_c), SQLT_STR, (dvoid *) 0,
                       (ub2 *) 0, (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query->data, s->errhp,
       OCIDefineByPos (stmthp, &defn2, s->errhp, 2, (dvoid *) & sh,
                       (sword) sizeof (sh), SQLT_INT, (dvoid *) 0, (ub2 *) 0,
                       (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr
      (query->data, s->errhp,
       OCIDefineByPos (stmthp, &defn3, s->errhp, 3, (dvoid *) & ih,
                       (sword) sizeof (ih), SQLT_INT, (dvoid *) 0, (ub2 *) 0,
                       (ub2 *) 0, OCI_DEFAULT)) != OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 0, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  stat.probability = 0;

  while ((status =
          OCIStmtFetch (stmthp, s->errhp, (ub4) 1, (ub4) OCI_FETCH_NEXT, (ub4)
                        OCI_DEFAULT)) == OCI_SUCCESS
         || status == OCI_SUCCESS_WITH_INFO)
  {

    token = strtoull (token_c, NULL, 0);

    if (token)
    {

      if (sh < 0)
        sh = 0;
      if (ih < 0)
        ih = 0;

      stat.spam_hits = sh;
      stat.innocent_hits = ih;
      stat.status |= TST_DISK;

      /* our control token will tell us how each token was altered */
      if (stat.spam_hits && stat.innocent_hits && !s->control_token)
      {
        s->control_token = token;
        s->control_sh = stat.spam_hits;
        s->control_ih = stat.innocent_hits;
      }
      lht_setspamstat (freq, token, &stat);
    }
  }

  if (!s->control_token)
  {
    s->control_token = token;
    s->control_sh = stat.spam_hits;
    s->control_ih = stat.innocent_hits;
  }

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  buffer_destroy (query);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;

}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct _ds_spam_stat stat;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  buffer *query;
  char scratch[1024];
  struct passwd *p;
  int update_one = 0;
  OCIStmt *stmthp;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: storage not initialized");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY &&
        (CTX->training_mode != DST_TOE ||
          (freq->whitelist_token == 0 && (!(CTX->flags & DSF_NOISE)))))
    return 0;

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  if (s->control_token == 0)
  {
    node_lht = c_lht_first (freq, &c_lht);
    if (node_lht == NULL)
    {
      stat.spam_hits = 0;
      stat.innocent_hits = 0;
    }
    else
    {
      stat.spam_hits = node_lht->s.spam_hits;
      stat.innocent_hits = node_lht->s.innocent_hits;
    }
  }
  else
  {
    lht_getspamstat (freq, s->control_token, &stat);
  }
  snprintf (scratch, sizeof (scratch),
            "UPDATE %s.DSPAM_TOKEN_DATA SET LAST_HIT = SYSDATE, "
            "  SPAM_HITS = GREATEST(0, SPAM_HITS %s %d), "
            "  INNOCENT_HITS = GREATEST(0, INNOCENT_HITS %s %d) "
            "  WHERE USER_ID = %d AND TOKEN IN(",
            s->schema,
            (stat.spam_hits > s->control_sh) ? "+" : "-",
            abs (stat.spam_hits - s->control_sh),
            (stat.innocent_hits > s->control_ih) ? "+" : "-",
            abs (stat.innocent_hits - s->control_ih), p->pw_uid);

  buffer_cat (query, scratch);

  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    int wrote_this = 0;
    if (CTX->training_mode == DST_TOE           &&
        CTX->classification == DSR_NONE         &&
	CTX->operating_mode == DSM_CLASSIFY	&&
        freq->whitelist_token != node_lht->key  &&
        (!node_lht->token_name || strncmp(node_lht->token_name, "bnr.", 4)))
    {
      node_lht = c_lht_next(freq, &c_lht);
      continue;
    }

    if (!(node_lht->s.status & TST_DIRTY)) {
      node_lht = c_lht_next(freq, &c_lht);
      continue;
    }

    lht_getspamstat (freq, node_lht->key, &stat);

    if (!(stat.status & TST_DISK))
    {
      char insert[1024];

      snprintf (insert, sizeof (insert),
                "INSERT INTO %s.DSPAM_TOKEN_DATA(USER_ID, TOKEN, SPAM_HITS, "
                "INNOCENT_HITS, LAST_HIT) VALUES(%d, %llu, %ld, %ld, SYSDATE)",
                s->schema, p->pw_uid, node_lht->key,
                stat.spam_hits, stat.innocent_hits);

      if (_ora_drv_checkerr
          (NULL, s->errhp,
           OCIHandleAlloc ((dvoid *) s->envhp, (dvoid **) & stmthp,
                           (ub4) OCI_HTYPE_STMT, (size_t) 0,
                           (dvoid **) 0)) != OCI_SUCCESS)
        return EUNKNOWN;

      if (_ora_drv_checkerr
          (insert, s->errhp,
           OCIStmtPrepare (stmthp, s->errhp, (text *) insert,
                           (ub4) strlen (insert), (ub4) OCI_NTV_SYNTAX,
                           (ub4) OCI_DEFAULT)) != OCI_SUCCESS)
        goto bail;

      if (_ora_drv_checkerr
          (insert, s->errhp,
           OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT)) != OCI_SUCCESS)
        stat.status |= TST_DISK;
    }

    if ((stat.status & TST_DISK))
    {
      snprintf (scratch, sizeof (scratch), "%llu", node_lht->key);
      buffer_cat (query, scratch);
      update_one = 1;
      wrote_this = 1;
      node_lht->s.status |= TST_DISK;
    }
    node_lht = c_lht_next (freq, &c_lht);
    if (node_lht != NULL && wrote_this)
      buffer_cat (query, ",");
  }

  if (query->used && query->data[strlen (query->data) - 1] == ',')
  {
    query->used--;
    query->data[strlen (query->data) - 1] = 0;
                                                                                
  }

  buffer_cat (query, ")");

  if (update_one)
  {
    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query->data, s->errhp, OCIStmtPrepare (stmthp,
                                                                  s->errhp,
                                                                  (text *)
                                                                  query->data,
                                                                  (ub4)
                                                                  strlen
                                                                  (query->
                                                                   data),
                                                                  (ub4)
                                                                  OCI_NTV_SYNTAX,
                                                                  (ub4)
                                                                  OCI_DEFAULT))
        != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query->data, s->errhp, OCIStmtExecute (s->svchp,
                                                                  stmthp,
                                                                  s->errhp,
                                                                  (ub4) 1,
                                                                  (ub4) 0,
                                                                  (OCISnapshot
                                                                   *) NULL,
                                                                  (OCISnapshot
                                                                   *) NULL,
                                                                  (ub4)
                                                                  OCI_DEFAULT))
        != OCI_SUCCESS)
      goto bail;
  }

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  buffer_destroy (query);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EFAILURE;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  OCIStmt *stmthp;
  OCIDefine *defn1 = NULL, *defn2 = NULL;
  sword status;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT SPAM_HITS, INNOCENT_HITS FROM %s.DSPAM_TOKEN_DATA WHERE "
            "  USER_ID = %d AND TOKEN = %llu", s->schema, p->pw_uid, token);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn1,
                                                          s->errhp, 1,
                                                          (dvoid *) & stat->
                                                          spam_hits,
                                                          (sword)
                                                          sizeof (long),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  if (_ora_drv_checkerr (query, s->errhp, OCIDefineByPos (stmthp, &defn2,
                                                          s->errhp, 2,
                                                          (dvoid *) & stat->
                                                          innocent_hits,
                                                          (sword)
                                                          sizeof (long),
                                                          SQLT_INT,
                                                          (dvoid *) 0,
                                                          (ub2 *) 0,
                                                          (ub2 *) 0,
                                                          OCI_DEFAULT)) !=
      OCI_SUCCESS)
    goto bail;

  status = OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                           (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                           (ub4) OCI_DEFAULT);

  stat->probability = 0;

  if (status == OCI_NO_DATA)
  {
    OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
    return EFAILURE;
  }

  if (_ora_drv_checkerr (query, s->errhp, status) != OCI_SUCCESS)
    goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;

}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  OCIStmt *stmthp;

  if (CTX->operating_mode == DSM_CLASSIFY)
    return 0;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  /* Try to insert first; will fail if exists because of unique index */

  snprintf (query, sizeof (query),
            "INSERT INTO %s.DSPAM_TOKEN_DATA (USER_ID, TOKEN, INNOCENT_HITS,"
            "  SPAM_HITS, LAST_HIT) VALUES(%d, %llu, %ld, %ld, SYSDATE)",
            s->schema, p->pw_uid, token, 
            stat->innocent_hits > 0 ? stat->innocent_hits : 0,
            stat->spam_hits > 0 ? stat->spam_hits : 0);

  if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                         (dvoid **) & stmthp,
                                                         (ub4) OCI_HTYPE_STMT,
                                                         (size_t) 0,
                                                         (dvoid **) 0)) !=
      OCI_SUCCESS)
    return EUNKNOWN;

  if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                          (text *) query,
                                                          (ub4)
                                                          strlen (query),
                                                          (ub4)
                                                          OCI_NTV_SYNTAX,
                                                          (ub4) OCI_DEFAULT))
      != OCI_SUCCESS)
    goto bail;


  /* If insert fails, try an update */

  if (OCIStmtExecute (s->svchp, stmthp, s->errhp, (ub4) 1, (ub4) 0,
                      (OCISnapshot *) NULL, (OCISnapshot *) NULL,
                      (ub4) OCI_DEFAULT) != OCI_SUCCESS)
  {

    snprintf (query, sizeof (query),
              "UPDATE %s.DSPAM_TOKEN_DATA SET INNOCENT_HITS = %ld, SPAM_HITS = %ld"
              "  WHERE USER_ID = %d AND TOKEN = %llu", s->schema,
              stat->innocent_hits, stat->spam_hits, p->pw_uid, token);
  if (CTX->training_mode == DST_TUM) {
    if (CTX->classification == DSR_NONE)
      strlcat(query, " AND INNOCENT_HITS + SPAM_HITS < 50", sizeof(query));
  }

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp, stmthp,
                                                            s->errhp, (ub4) 1,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;
  }

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;

}

int _ds_delall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  buffer *query;
  char scratch[1024];
  char queryhead[1024];
  struct passwd *p;
  OCIStmt *stmthp;

  if (freq == NULL || freq->items == 0)
    return 0;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (queryhead, sizeof (queryhead),
            "DELETE FROM %s.DSPAM_TOKEN_DATA "
            "  WHERE USER_ID = %d AND TOKEN IN(",
            s->schema, p->pw_uid);

  buffer_cat (query, queryhead);

  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
      snprintf (scratch, sizeof (scratch), "%llu", node_lht->key);
      buffer_cat (query, scratch);
      node_lht = c_lht_next (freq, &c_lht);
      if (node_lht != NULL) 
        buffer_cat (query, ",");
  }
  buffer_cat (query, ")");

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query->data, s->errhp, OCIStmtPrepare (stmthp,
                                                                  s->errhp,
                                                                  (text *)
                                                                  query->data,
                                                                  (ub4)
                                                                  strlen
                                                                  (query->
                                                                   data),
                                                                  (ub4)
                                                                  OCI_NTV_SYNTAX,
                                                                  (ub4)
                                                                  OCI_DEFAULT))
        != OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query->data, s->errhp, OCIStmtExecute (s->svchp,
                                                                  stmthp,
                                                                  s->errhp,
                                                                  (ub4) 1,
                                                                  (ub4) 0,
                                                                  (OCISnapshot
                                                                   *) NULL,
                                                                  (OCISnapshot
                                                                   *) NULL,
                                                                  (ub4)
                                                                  OCI_DEFAULT))
        != OCI_SUCCESS)
      goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  buffer_destroy (query);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EFAILURE;
}


int _ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token)
{
  struct _ora_drv_storage *s = (struct _ora_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  OCIStmt *stmthp;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: storage not initialized");
    return EINVAL;
  }

  p = _ora_drv_getpwnam (CTX, CTX->username);
  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _ora_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM %s.DSPAM_TOKEN_DATA "
            "  WHERE USER_ID = %d AND TOKEN = %llu", 
            s->schema, p->pw_uid, token);

    if (_ora_drv_checkerr (NULL, s->errhp, OCIHandleAlloc ((dvoid *) s->envhp,
                                                           (dvoid **) &
                                                           stmthp,
                                                           (ub4)
                                                           OCI_HTYPE_STMT,
                                                           (size_t) 0,
                                                           (dvoid **) 0)) !=
        OCI_SUCCESS)
      return EUNKNOWN;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtPrepare (stmthp, s->errhp,
                                                            (text *) query,
                                                            (ub4)
                                                            strlen (query),
                                                            (ub4)
                                                            OCI_NTV_SYNTAX,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

    if (_ora_drv_checkerr (query, s->errhp, OCIStmtExecute (s->svchp, stmthp,
                                                            s->errhp, (ub4) 1,
                                                            (ub4) 0,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (OCISnapshot *)
                                                            NULL,
                                                            (ub4)
                                                            OCI_DEFAULT)) !=
        OCI_SUCCESS)
      goto bail;

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return 0;

bail:

  OCIHandleFree ((dvoid *) stmthp, (ub4) OCI_HTYPE_STMT);
  return EUNKNOWN;

}

