/* $Id: pgsql_drv.c,v 1.753 2011/09/30 20:52:05 sbajic Exp $ */

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
#include <libpq-fe.h>
#ifdef DAEMON
#include <pthread.h>
#endif

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
#include "pgsql_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"
#include "pref.h"
#include "config_shared.h"


#ifdef HAVE_PQFREEMEM
#	define PQFREEMEM(A) PQfreemem(A)
#else
#	define PQFREEMEM(A) free(A)
#endif


#ifndef NUMERICOID
#    define NUMERICOID 1700
#endif
#ifndef INT8OID
#    define INT8OID    20
#endif
#define BIGINTOID INT8OID


/* Notice pocessor function for PostgreSQL */
static void
_pgsql_drv_notice_processor(void *arg, const PGresult *res)
{
  /* nothing */
}


/* Notice receiver function for PostgreSQL */
static void
_pgsql_drv_notice_receiver(void *arg, const char *message)
{
  arg = arg;   /* Keep compiler happy */
  LOGDEBUG ("pgsql_drv: %s", message);
}

int
dspam_init_driver (DRIVER_CTX *DTX)
{

  if (DTX == NULL)
    return 0;

  /* Establish a series of stateful connections */

  if (DTX->flags & DRF_STATEFUL) {
    int i, connection_cache = 3;

    if (_ds_read_attribute(DTX->CTX->config->attributes, "PgSQLConnectionCache"))
      connection_cache = atoi(_ds_read_attribute(DTX->CTX->config->attributes, "PgSQLConnectionCache"));

    DTX->connection_cache = connection_cache;
    DTX->connections = calloc(1, sizeof(struct _ds_drv_connection *)*connection_cache);
    if (DTX->connections == NULL) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      return EUNKNOWN;
    }

    for(i=0;i<connection_cache;i++) {
      DTX->connections[i] = calloc(1, sizeof(struct _ds_drv_connection));
      if (DTX->connections[i]) {
#ifdef DAEMON
        LOGDEBUG("dspam_init_driver: initializing lock %d", i);
        pthread_mutex_init(&DTX->connections[i]->lock, NULL);
#endif
        DTX->connections[i]->dbh = (void *) _ds_connect(DTX->CTX);
      }
    }
  }

  return 0;
}

int
dspam_shutdown_driver (DRIVER_CTX *DTX)
{
  if (DTX != NULL) {
    if (DTX->flags & DRF_STATEFUL && DTX->connections) {
      int i;

      for(i=0;i<DTX->connection_cache;i++) {
        if (DTX->connections[i]) {
          if (DTX->connections[i]->dbh) {
            PQfinish((PGconn *)DTX->connections[i]->dbh);
          }
#ifdef DAEMON
          LOGDEBUG("dspam_shutdown_driver: destroying lock %d", i);
          pthread_mutex_destroy(&DTX->connections[i]->lock);
#endif
          free(DTX->connections[i]);
        }
      }

      free(DTX->connections);
      DTX->connections = NULL;
    }
  }

  return 0;
}

int
_pgsql_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  char query[512];
  struct passwd *p;
  char *name;
  PGresult *result;
  struct _ds_spam_totals user, group;
  int uid = -1, gid = -1;
  int i, ntuples;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_pgsql_drv_get_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  memset(&s->control_totals, 0, sizeof(struct _ds_spam_totals));
  if (CTX->flags & DSF_MERGED) {
    memset(&s->merged_totals, 0, sizeof(struct _ds_spam_totals));
    memset(&group, 0, sizeof(struct _ds_spam_totals));
  }

  memset(&CTX->totals, 0, sizeof(struct _ds_spam_totals));
  memset(&user, 0, sizeof(struct _ds_spam_totals));

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_pgsql_drv_get_spamtotals: unable to _pgsql_drv_getpwnam(%s)",
              name);
    if (!(CTX->flags & DSF_MERGED))
      return EINVAL;
  } else {
    uid = (int) p->pw_uid;
  }

  if (CTX->group != NULL && CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_pgsql_drv_getspamtotals: unable to _pgsql_drv_getpwnam(%s)",
                CTX->group);
      return EINVAL;
    }
    gid = (int) p->pw_uid;
  }

  if (gid < 0) gid = uid;

  if (gid != uid)
    snprintf (query, sizeof (query),
	      "SELECT uid,spam_learned,innocent_learned,"
	      "spam_misclassified,innocent_misclassified,"
	      "spam_corpusfed,innocent_corpusfed,"
	      "spam_classified,innocent_classified"
	      " FROM dspam_stats WHERE uid IN ('%d','%d')",
	      (int) uid, (int) gid);
  else
    snprintf (query, sizeof (query),
	      "SELECT uid,spam_learned,innocent_learned,"
	      "spam_misclassified,innocent_misclassified,"
	      "spam_corpusfed,innocent_corpusfed,"
	      "spam_classified,innocent_classified"
	      " FROM dspam_stats WHERE uid=%d",
	      (int) uid);

  result = PQexec(s->dbh, query);

  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if ( PQntuples(result) < 1 )
  {
    LOGDEBUG("_pgsql_drv_get_spamtotals: failed PQntuples()");
    if (result) PQclear(result);
    return EFAILURE;
  }

  ntuples = PQntuples(result);

  for (i=0; i<ntuples; i++)
  {
    int rid = atoi(PQgetvalue(result,i,0));
    if (rid == INT_MAX && errno == ERANGE) {
      LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to rid", PQgetvalue(result,i,0));
      goto FAIL;
    }
    if (rid == uid) {
      user.spam_learned			= strtoul (PQgetvalue(result,i,1), NULL, 0);
      if (user.spam_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.spam_learned", PQgetvalue(result,i,1));
        goto FAIL;
      }
      user.innocent_learned		= strtoul (PQgetvalue(result,i,2), NULL, 0);
      if (user.innocent_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.innocent_learned", PQgetvalue(result,i,2));
        goto FAIL;
      }
      user.spam_misclassified		= strtoul (PQgetvalue(result,i,3), NULL, 0);
      if (user.spam_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.spam_misclassified", PQgetvalue(result,i,3));
        goto FAIL;
      }
      user.innocent_misclassified	= strtoul (PQgetvalue(result,i,4), NULL, 0);
      if (user.innocent_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.innocent_misclassified", PQgetvalue(result,i,4));
        goto FAIL;
      }
      user.spam_corpusfed		= strtoul (PQgetvalue(result,i,5), NULL, 0);
      if (user.spam_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.spam_corpusfed", PQgetvalue(result,i,5));
        goto FAIL;
      }
      user.innocent_corpusfed		= strtoul (PQgetvalue(result,i,6), NULL, 0);
      if (user.innocent_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.innocent_corpusfed", PQgetvalue(result,i,6));
        goto FAIL;
      }
      if (PQgetvalue(result,i,7) != NULL && PQgetvalue(result,i,8) != NULL) {
        user.spam_classified		= strtoul (PQgetvalue(result,i,7), NULL, 0);
        if (user.spam_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.spam_classified", PQgetvalue(result,i,7));
          goto FAIL;
        }
        user.innocent_classified	= strtoul (PQgetvalue(result,i,8), NULL, 0);
        if (user.innocent_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to user.innocent_classified", PQgetvalue(result,i,8));
          goto FAIL;
        }
      } else {
        user.spam_classified		= 0;
        user.innocent_classified	= 0;
      }
    } else {
      group.spam_learned		= strtoul (PQgetvalue(result,i,1), NULL, 0);
      if (group.spam_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.spam_learned", PQgetvalue(result,i,1));
        goto FAIL;
      }
      group.innocent_learned		= strtoul (PQgetvalue(result,i,2), NULL, 0);
      if (group.innocent_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.innocent_learned", PQgetvalue(result,i,2));
        goto FAIL;
      }
      group.spam_misclassified		= strtoul (PQgetvalue(result,i,3), NULL, 0);
      if (group.spam_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.spam_misclassified", PQgetvalue(result,i,3));
        goto FAIL;
      }
      group.innocent_misclassified	= strtoul (PQgetvalue(result,i,4), NULL, 0);
      if (group.innocent_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.innocent_misclassified", PQgetvalue(result,i,4));
        goto FAIL;
      }
      group.spam_corpusfed		= strtoul (PQgetvalue(result,i,5), NULL, 0);
      if (group.spam_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.spam_corpusfed", PQgetvalue(result,i,5));
        goto FAIL;
      }
      group.innocent_corpusfed		= strtoul (PQgetvalue(result,i,6), NULL, 0);
      if (group.innocent_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.innocent_corpusfed", PQgetvalue(result,i,6));
        goto FAIL;
      }
      if (PQgetvalue(result,i,7) != NULL && PQgetvalue(result,i,8) != NULL) {
        group.spam_classified		= strtoul (PQgetvalue(result,i,7), NULL, 0);
        if (group.spam_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.spam_classified", PQgetvalue(result,i,7));
          goto FAIL;
        }
        group.innocent_classified	= strtoul (PQgetvalue(result,i,8), NULL, 0);
        if (group.innocent_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_pgsql_drv_get_spamtotals: failed converting %s to group.innocent_classified", PQgetvalue(result,i,8));
          goto FAIL;
        }
      } else {
        group.spam_classified		= 0;
        group.innocent_classified	= 0;
      }
    }
  }

  if (result) PQclear(result);
  result = NULL;

  if (CTX->flags & DSF_MERGED) {
    memcpy(&s->merged_totals, &group, sizeof(struct _ds_spam_totals));
    memcpy(&s->control_totals, &user, sizeof(struct _ds_spam_totals));
    CTX->totals.spam_learned
      = user.spam_learned + group.spam_learned;
    CTX->totals.innocent_learned
      = user.innocent_learned + group.innocent_learned;
    CTX->totals.spam_misclassified
      = user.spam_misclassified + group.spam_misclassified;
    CTX->totals.innocent_misclassified
      = user.innocent_misclassified + group.innocent_misclassified;
    CTX->totals.spam_corpusfed
      = user.spam_corpusfed + group.spam_corpusfed;
    CTX->totals.innocent_corpusfed
      = user.innocent_corpusfed + group.innocent_corpusfed;
    CTX->totals.spam_classified
      = user.spam_classified + group.spam_classified;
    CTX->totals.innocent_classified
      = user.innocent_classified + group.innocent_classified;
  } else {
    memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));
    memcpy(&s->control_totals, &user, sizeof(struct _ds_spam_totals));
  }

  return 0;

FAIL:
  if (result) PQclear(result);
  result = NULL;
  return EFAILURE;
}

int
_pgsql_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[1024];
  PGresult *result;
  int update_any = 0;
  struct _ds_spam_totals user;
  result = NULL;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_pgsql_drv_set_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _pgsql_drv_get_spamtotals (CTX);    /* undo changes to in memory totals */
    return 0;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_pgsql_drv_set_spamtotals: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  /* Subtract the group totals from our active set */
  if (CTX->flags & DSF_MERGED) {
    memcpy(&user, &CTX->totals, sizeof(struct _ds_spam_totals));
    CTX->totals.innocent_learned -= s->merged_totals.innocent_learned;
    CTX->totals.spam_learned -= s->merged_totals.spam_learned;
    CTX->totals.innocent_misclassified -= s->merged_totals.innocent_misclassified;
    CTX->totals.spam_misclassified -= s->merged_totals.spam_misclassified;
    CTX->totals.innocent_corpusfed -= s->merged_totals.innocent_corpusfed;
    CTX->totals.spam_corpusfed -= s->merged_totals.spam_corpusfed;
    CTX->totals.innocent_classified -= s->merged_totals.innocent_classified;
    CTX->totals.spam_classified -= s->merged_totals.spam_classified;

    if (CTX->totals.innocent_learned < 0) CTX->totals.innocent_learned = 0;
    if (CTX->totals.spam_learned < 0) CTX->totals.spam_learned = 0;
    if (CTX->totals.innocent_misclassified < 0) CTX->totals.innocent_misclassified = 0;
    if (CTX->totals.spam_misclassified < 0) CTX->totals.spam_misclassified = 0;
    if (CTX->totals.innocent_corpusfed < 0) CTX->totals.innocent_corpusfed = 0;
    if (CTX->totals.spam_corpusfed < 0) CTX->totals.spam_corpusfed = 0;
    if (CTX->totals.innocent_classified < 0) CTX->totals.innocent_classified = 0;
    if (CTX->totals.spam_classified < 0) CTX->totals.spam_classified = 0;

  }

  if (s->control_totals.innocent_learned == 0)
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_stats (uid,spam_learned,innocent_learned,"
              "spam_misclassified,innocent_misclassified,"
              "spam_corpusfed,innocent_corpusfed,"
              "spam_classified,innocent_classified)"
              " VALUES (%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu)",
              (int) p->pw_uid, CTX->totals.spam_learned,
              CTX->totals.innocent_learned, CTX->totals.spam_misclassified,
              CTX->totals.innocent_misclassified, CTX->totals.spam_corpusfed,
              CTX->totals.innocent_corpusfed, CTX->totals.spam_classified,
              CTX->totals.innocent_classified);
    result = PQexec(s->dbh, query);
  }

  if ( s->control_totals.innocent_learned != 0 || result == NULL || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    if (result) PQclear(result);
    result = NULL;

    /* Do not update stats if all values are zero (aka: no update needed) */
    if (!(abs(CTX->totals.spam_learned           - s->control_totals.spam_learned) == 0 &&
          abs(CTX->totals.innocent_learned       - s->control_totals.innocent_learned) == 0 &&
          abs(CTX->totals.spam_misclassified     - s->control_totals.spam_misclassified) == 0 &&
          abs(CTX->totals.innocent_misclassified - s->control_totals.innocent_misclassified) == 0 &&
          abs(CTX->totals.spam_corpusfed         - s->control_totals.spam_corpusfed) == 0 &&
          abs(CTX->totals.innocent_corpusfed     - s->control_totals.innocent_corpusfed) == 0 &&
          abs(CTX->totals.spam_classified        - s->control_totals.spam_classified) == 0 &&
          abs(CTX->totals.innocent_classified    - s->control_totals.innocent_classified) == 0)) {

      buffer *buf;
      buf = buffer_create (NULL);
      if (buf == NULL) {
        LOG (LOG_CRIT, ERR_MEM_ALLOC);
        if (CTX->flags & DSF_MERGED)
          memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));
        return EUNKNOWN;
      }

      snprintf (query, sizeof (query),
                "UPDATE dspam_stats SET ");
      buffer_copy (buf, query);

      /* Do not update spam learned if no change is needed */
      if (abs (CTX->totals.spam_learned - s->control_totals.spam_learned) != 0) {
        snprintf (query, sizeof (query),
                  "%sspam_learned=spam_learned%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.spam_learned > s->control_totals.spam_learned) ? "+" : "-",
                  abs (CTX->totals.spam_learned - s->control_totals.spam_learned));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update innocent learned if no change is needed */
      if (abs (CTX->totals.innocent_learned - s->control_totals.innocent_learned) != 0) {
        snprintf (query, sizeof (query),
                  "%sinnocent_learned=innocent_learned%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.innocent_learned > s->control_totals.innocent_learned) ? "+" : "-",
                  abs (CTX->totals.innocent_learned - s->control_totals.innocent_learned));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update spam misclassified if no change is needed */
      if (abs (CTX->totals.spam_misclassified - s->control_totals.spam_misclassified) != 0) {
        snprintf (query, sizeof (query),
                  "%sspam_misclassified=spam_misclassified%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.spam_misclassified > s->control_totals.spam_misclassified) ? "+" : "-",
                  abs (CTX->totals.spam_misclassified - s->control_totals.spam_misclassified));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update innocent misclassified if no change is needed */
      if (abs (CTX->totals.innocent_misclassified - s->control_totals.innocent_misclassified) != 0) {
        snprintf (query, sizeof (query),
                  "%sinnocent_misclassified=innocent_misclassified%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.innocent_misclassified > s->control_totals.innocent_misclassified) ? "+" : "-",
                  abs (CTX->totals.innocent_misclassified - s->control_totals.innocent_misclassified));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update spam corpusfed if no change is needed */
      if (abs (CTX->totals.spam_corpusfed - s->control_totals.spam_corpusfed) != 0) {
        snprintf (query, sizeof (query),
                  "%sspam_corpusfed=spam_corpusfed%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.spam_corpusfed > s->control_totals.spam_corpusfed) ? "+" : "-",
                  abs (CTX->totals.spam_corpusfed - s->control_totals.spam_corpusfed));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update innocent corpusfed if no change is needed */
      if (abs (CTX->totals.innocent_corpusfed - s->control_totals.innocent_corpusfed) != 0) {
        snprintf (query, sizeof (query),
                  "%sinnocent_corpusfed=innocent_corpusfed%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.innocent_corpusfed > s->control_totals.innocent_corpusfed) ? "+" : "-",
                  abs (CTX->totals.innocent_corpusfed - s->control_totals.innocent_corpusfed));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update spam classified if no change is needed */
      if (abs (CTX->totals.spam_classified - s->control_totals.spam_classified) != 0) {
        snprintf (query, sizeof (query),
                  "%sspam_classified=spam_classified%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.spam_classified > s->control_totals.spam_classified) ? "+" : "-",
                  abs (CTX->totals.spam_classified - s->control_totals.spam_classified));
        update_any = 1;
        buffer_cat (buf, query);
      }
      /* Do not update innocent classified if no change is needed */
      if (abs (CTX->totals.innocent_classified - s->control_totals.innocent_classified) != 0) {
        snprintf (query, sizeof (query),
                  "%sinnocent_classified=innocent_classified%s%d",
                  (update_any) ? "," : "",
                  (CTX->totals.innocent_classified > s->control_totals.innocent_classified) ? "+" : "-",
                  abs (CTX->totals.innocent_classified - s->control_totals.innocent_classified));
        buffer_cat (buf, query);
      }

      snprintf (query, sizeof (query),
                " WHERE uid=%d",
                (int) p->pw_uid);
      buffer_cat (buf, query);

      result = PQexec(s->dbh, buf->data);

      if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
      {
        _pgsql_drv_query_error (PQresultErrorMessage(result), buf->data);
        if (result) PQclear(result);
        if (CTX->flags & DSF_MERGED)
          memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));
        buffer_destroy (buf);
        return EFAILURE;
      }
      buffer_destroy (buf);
    }
  }

  if (result) PQclear(result);

  if (CTX->flags & DSF_MERGED)
    memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  buffer *query;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  char scratch[1024];
  PGresult *result;
  struct _ds_spam_stat stat;
  unsigned long long token = 0;
  int get_one = 0;
  int uid = -1, gid = -1;
  int i, ntuples;

  if (diction->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  uid = (int) p->pw_uid;

  if (CTX->group != NULL && CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_ds_getall_spamrecords: unable to _pgsql_drv_getpwnam(%s)",
                CTX->group);
      return EINVAL;
    }
    gid = (int) p->pw_uid;
  }

  if (gid < 0) gid = uid;

  stat.spam_hits     = 0;
  stat.innocent_hits = 0;
  stat.probability   = 0.00000;

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  if (uid != gid) {
    snprintf (scratch, sizeof (scratch),
              "SELECT * FROM lookup_tokens(%d,%d,'{", (int) uid, (int) gid);
  } else {
    snprintf (scratch, sizeof (scratch),
              "SELECT * FROM lookup_tokens(%d,'{", (int) uid);
  }
  buffer_copy (query, scratch);

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term) {
    _pgsql_drv_token_write (s->pg_token_type, ds_term->key, scratch, sizeof(scratch));
    buffer_cat (query, scratch);
    ds_term->s.innocent_hits = 0;
    ds_term->s.spam_hits = 0;
    ds_term->s.probability = 0.00000;
    ds_term->s.status = 0;
    ds_term = ds_diction_next(ds_c);
    if (ds_term)
      buffer_cat (query, ",");
    get_one = 1;
  }
  ds_diction_close(ds_c);

  buffer_cat(query, "}')");

#ifdef VERBOSE
  LOGDEBUG ("pgsql query length: %ld\n", query->used);
  _pgsql_drv_query_error ("VERBOSE DEBUG (INFO ONLY - NOT AN ERROR)", query->data);
#endif

  if (!get_one)
    return 0;

  result = PQexec(s->dbh, query->data);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query->data);
    if (result) PQclear(result);
    buffer_destroy(query);
    return EFAILURE;
  }

  buffer_destroy(query);
  ntuples = PQntuples(result);

  for (i=0; i<ntuples; i++)
  {
    int rid = atoi(PQgetvalue(result,i,0));
    if (rid == INT_MAX && errno == ERANGE) {
      LOGDEBUG("_ds_getall_spamrecords: failed converting %s to rid", PQgetvalue(result,i,0));
      goto FAIL;
    }
    token = _pgsql_drv_token_read (s->pg_token_type, PQgetvalue(result,i,1));
    stat.spam_hits = strtoul (PQgetvalue(result,i,2), NULL, 0);
    if (stat.spam_hits == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.spam_hits", PQgetvalue(result,i,2));
      goto FAIL;
    }
    stat.innocent_hits = strtoul (PQgetvalue(result,i,3), NULL, 0);
    if (stat.innocent_hits == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.innocent_hits", PQgetvalue(result,i,3));
      goto FAIL;
    }
    stat.status = 0;

    if (rid == uid)
      stat.status |= TST_DISK;

    ds_diction_addstat(diction, token, &stat);
  }

  if (result) PQclear(result);

  /* Control token */
  stat.spam_hits = 10;
  stat.innocent_hits = 10;
  stat.status = 0;
  ds_diction_touch(diction, CONTROL_TOKEN, "$$CONTROL$$", 0);
  ds_diction_addstat(diction, CONTROL_TOKEN, &stat);
  s->control_token = CONTROL_TOKEN;
  s->control_ih = 10;
  s->control_sh = 10;

  return 0;

FAIL:
  if (result) PQclear(result);
  return EFAILURE;
}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct _ds_spam_stat control, stat;
  ds_term_t ds_term;
  ds_cursor_t ds_c = NULL;
  buffer *prepare;
  buffer *update;
  PGresult *result;
  char scratch[1024];
  struct passwd *p;
  char *name;
  int update_one = 0;

  if (diction->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY &&
     (CTX->training_mode != DST_TOE ||
     (diction->whitelist_token == 0 && (!(CTX->flags & DSF_NOISE)))))
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  prepare = buffer_create (NULL);
  if (prepare == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }
  update = buffer_create (NULL);
  if (update == NULL)
  {
    buffer_destroy(prepare);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  ds_diction_getstat(diction, s->control_token, &control);
  snprintf (scratch, sizeof (scratch),
            "PREPARE dspam_update_plan (%s) AS UPDATE dspam_token_data"
            " SET last_hit=CURRENT_DATE",
            s->pg_token_type == 0 ? "numeric" : "bigint");
  buffer_cat (prepare, scratch);

  /* Do not update spam hits if no change is needed */
  if (abs(control.spam_hits - s->control_sh) != 0)
  {
    if (control.spam_hits > s->control_sh)
    {
      snprintf (scratch, sizeof (scratch),
                ",spam_hits=spam_hits+%d",
                abs (control.spam_hits - s->control_sh) );
    } else {
      snprintf (scratch, sizeof (scratch),
                ",spam_hits="
                             "CASE WHEN spam_hits-%d<=0 THEN 0"
                             " ELSE spam_hits-%d END",
                abs (control.spam_hits - s->control_sh),
                abs (control.spam_hits - s->control_sh) );
    }
    buffer_cat (prepare, scratch);
  }

  /* Do not update innocent hits if no change is needed */
  if (abs(control.innocent_hits - s->control_ih) != 0)
  {
    if (control.innocent_hits > s->control_ih)
    {
      snprintf (scratch, sizeof (scratch),
                ",innocent_hits=innocent_hits+%d",
                abs (control.innocent_hits - s->control_ih) );
    } else {
      snprintf (scratch, sizeof (scratch),
                ",innocent_hits="
                             "CASE WHEN innocent_hits-%d<=0 THEN 0"
                             " ELSE innocent_hits-%d END",
                abs (control.innocent_hits - s->control_ih),
                abs (control.innocent_hits - s->control_ih) );
    }
    buffer_cat (prepare, scratch);
  }

  snprintf (scratch, sizeof (scratch),
            " WHERE uid=%d AND token=$1;",
             (int) p->pw_uid);
  buffer_cat (prepare, scratch);

  snprintf (scratch, sizeof (scratch),
            "PREPARE dspam_insert_plan (%s,int,int) AS INSERT INTO dspam_token_data"
            " (uid,token,spam_hits,innocent_hits,last_hit) VALUES"
            " (%d,$1,$2,$3,CURRENT_DATE);",
            s->pg_token_type == 0 ? "numeric" : "bigint", (int) p->pw_uid);

  buffer_cat (prepare, scratch);

  /* prepare update and insert statement */
  result = PQexec(s->dbh, prepare->data);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), prepare->data);
    if (result) PQclear(result);
    buffer_destroy(update);
    buffer_destroy(prepare);
    return EFAILURE;
  }

  if (result) PQclear(result);
  result = NULL;
  buffer_destroy(prepare);
  buffer_cat (update, "BEGIN;");

  /*
   *  Add each token in the diction to either an update or an insert queue
   */

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    if (ds_term->key == s->control_token) {
      ds_term = ds_diction_next(ds_c);
      continue;
    }

    /* Don't write lexical tokens if we're in TOE mode classifying */

    if (CTX->training_mode == DST_TOE            &&
        CTX->operating_mode == DSM_CLASSIFY      &&
        ds_term->key != diction->whitelist_token &&
        (!ds_term->name || strncmp(ds_term->name, "bnr.", 4)))
    {
      ds_term = ds_diction_next(ds_c);
      continue;
    }

    ds_diction_getstat(diction, ds_term->key, &stat);

    /* Changed tokens are marked as "dirty" by libdspam */

    if (!(stat.status & TST_DIRTY)) {
      ds_term = ds_diction_next(ds_c);
      continue;
    } else {
      stat.status &= ~TST_DIRTY;
    }

    /* This token wasn't originally loaded from disk, so try an insert */

    if (!(stat.status & TST_DISK))
    {
      char tok_buf[30];
      const char *insertValues[3];

      insertValues[0] = _pgsql_drv_token_write(s->pg_token_type, ds_term->key, tok_buf, sizeof(tok_buf));
      insertValues[1] = stat.spam_hits > 0 ? "1" : "0";
      insertValues[2] = stat.innocent_hits > 0 ? "1" : "0";

      result = PQexecPrepared(s->dbh, "dspam_insert_plan", 3, insertValues, NULL, NULL, 1);
      if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) ) {
        stat.status |= TST_DISK;
      }
      if (result) PQclear(result);
      result = NULL;
    }

    if ((stat.status & TST_DISK)) {
      _pgsql_drv_token_write(s->pg_token_type, ds_term->key, scratch, sizeof(scratch));
      buffer_cat (update, "EXECUTE dspam_update_plan (");
      buffer_cat (update, scratch);
      buffer_cat (update, ");");
      update_one = 1;
    }

    ds_term->s.status |= TST_DISK;

    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);

  buffer_cat (update, "COMMIT;");

  LOGDEBUG("Control: [%ld %ld] [%lu %lu] Delta: [%lu %lu]",
    s->control_sh, s->control_ih,
    control.spam_hits, control.innocent_hits,
    control.spam_hits - s->control_sh, control.innocent_hits - s->control_ih);

  if (update_one)
  {
    result = PQexec(s->dbh, update->data);
    if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
    {
      _pgsql_drv_query_error (PQresultErrorMessage(result), update->data);
      if (result) PQclear(result);
      buffer_destroy(update);
      return EFAILURE;
    }
    if (result) PQclear(result);
    result = NULL;
  }

  buffer_destroy(update);

  /* deallocate prepared update and insert statements */
  snprintf (scratch, sizeof (scratch), "DEALLOCATE dspam_insert_plan;"
                                       "DEALLOCATE dspam_update_plan;");

  result = PQexec(s->dbh, scratch);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), scratch);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if (result) PQclear(result);
  result = NULL;
  return 0;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  char *name;
  PGresult *result;
  char tok_buf[30];

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT spam_hits,innocent_hits FROM dspam_token_data"
            " WHERE uid=%d AND token=%s ", (int) p->pw_uid,
            _pgsql_drv_token_write(s->pg_token_type, token, tok_buf, sizeof(tok_buf)));

  stat->probability = 0.00000;
  stat->spam_hits = 0;
  stat->innocent_hits = 0;
  stat->status &= ~TST_DISK;

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if ( PQntuples(result) < 1 )
  {
    if (result) PQclear(result);
    return 0;
  }

  stat->spam_hits = strtoul (PQgetvalue(result, 0, 0), NULL, 0);
  if (stat->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->spam_hits", PQgetvalue(result,0,0));
    if (result) PQclear(result);
    return EFAILURE;
  }
  stat->innocent_hits = strtoul (PQgetvalue(result, 0, 1), NULL, 0);
  if (stat->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->innocent_hits", PQgetvalue(result,0,1));
    if (result) PQclear(result);
    return EFAILURE;
  }
  stat->status |= TST_DISK;

  if (result) PQclear(result);
  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  char *name;
  PGresult *result;
  char tok_buf[30];

  result = NULL;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  /* It's either not on disk or the caller isn't using stat.status */
  if (!(stat->status & TST_DISK))
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_token_data (uid,token,spam_hits,innocent_hits,last_hit)"
              " VALUES (%d,%s,%lu,%lu,CURRENT_DATE)",
              (int) p->pw_uid,
              _pgsql_drv_token_write(s->pg_token_type, token, tok_buf, sizeof(tok_buf)),
              stat->spam_hits, stat->innocent_hits);
    result = PQexec(s->dbh, query);
  }

  if ((stat->status & TST_DISK) || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    /* insert failed; try updating instead */
    snprintf (query, sizeof (query), "UPDATE dspam_token_data"
              " SET spam_hits=%lu,"
              "innocent_hits=%lu,"
              "last_hit=CURRENT_DATE"
              " WHERE uid=%d"
              " AND token=%s", stat->spam_hits,
              stat->innocent_hits, (int) p->pw_uid,
              _pgsql_drv_token_write(s->pg_token_type, token, tok_buf, sizeof(tok_buf)));

    if (result) PQclear(result);

    result = PQexec(s->dbh, query);

    if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
    {
      _pgsql_drv_query_error (PQresultErrorMessage(result), query);
      if (result) PQclear(result);
      return EFAILURE;
    }
  }

  if (result) PQclear(result);

  return 0;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _pgsql_drv_storage *s;

  if (CTX == NULL) {
    return EINVAL;
  }

  /* don't init if we're already initted */
  if (CTX->storage != NULL)
  {
    LOGDEBUG ("_ds_init_storage: storage already initialized");
    return EINVAL;
  }

  s = calloc (1, sizeof (struct _pgsql_drv_storage));
  if (s == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  if (dbh) {
    s->dbh = dbh;
  } else {
    s->dbh = _pgsql_drv_connect(CTX);
  }

  s->dbh_attached = (dbh) ? 1 : 0;
  s->u_getnextuser[0] = 0;
  memset(&s->p_getpwnam, 0, sizeof(struct passwd));
  memset(&s->p_getpwuid, 0, sizeof(struct passwd));

  if (s->dbh == NULL || PQstatus(s->dbh) == CONNECTION_BAD)
  {
    LOG (LOG_WARNING, "%s", PQerrorMessage(s->dbh) );
    free(s);
    return EFAILURE;
  }

  CTX->storage = s;

  /* Start a transaction block
     Due to long time locks we'll not use
     single transaction to avoid deadlocks

  result = PQexec(s->dbh, "BEGIN");
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), "BEGIN");
    if (result) PQclear(result);
    return EFAILURE;
  }
  if (result) PQclear(result);
  */

  s->control_token = 0;
  s->control_ih = 0;
  s->control_sh = 0;

  /* init db version and token type */
  s->pg_token_type = _pgsql_drv_token_type(s,NULL,0);

  /* get spam totals on successful init */
  if (CTX->username != NULL)
  {
    if (_pgsql_drv_get_spamtotals (CTX))
    {
      LOGDEBUG ("_ds_init_storage: unable to load totals. Using zero values.");
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
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  // PGresult *result;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: called with NULL storage handle");
    return EINVAL;
  }

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: invalid database handle (NULL)");
    return EINVAL;
  }

  /* Store spam totals on shutdown */
  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
      _pgsql_drv_set_spamtotals (CTX);
  }

  if (s->iter_user != NULL) {
    PQclear (s->iter_user);
    s->iter_user = NULL;
  }

  if (s->iter_token != NULL) {
    PQclear (s->iter_token);
    s->iter_token = NULL;
  }

  if (s->iter_sig != NULL) {
    PQclear (s->iter_sig);
    s->iter_sig = NULL;
  }

  /* End a transaction block
  result = PQexec(s->dbh, "COMMIT");
  if (result) PQclear(result);
  */

  if (!s->dbh_attached)
    PQfinish(s->dbh);

  s->dbh = NULL;

  if (s->p_getpwnam.pw_name)
    free(s->p_getpwnam.pw_name);
  if (s->p_getpwuid.pw_name)
    free(s->p_getpwuid.pw_name);
  free(s);
  CTX->storage = NULL;

  return 0;
}

int
_ds_create_signature_id (DSPAM_CTX * CTX, char *buf, size_t len)
{
  char session[64];
  char digit[6];
  int pid, j;
  struct passwd *p;
  char *name;

  pid = getpid ();
  if (_ds_match_attribute(CTX->config->attributes, "PgSQLUIDInSignature", "on"))
  {
    if (!CTX->group || CTX->flags & DSF_MERGED) {
      p = _pgsql_drv_getpwnam (CTX, CTX->username);
      name = CTX->username;
    } else {
      p = _pgsql_drv_getpwnam (CTX, CTX->group);
      name = CTX->group;
    }
    if (!p) {
      LOG(LOG_ERR, "Unable to determine UID for %s", name);
      return EINVAL;
    }
    snprintf (session, sizeof (session), "%d,%8lx%d", (int) p->pw_uid,
              (long) time(NULL), pid);
  }
  else
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
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  size_t length;
  unsigned char *mem, *mem2;
  char query[256];
  PGresult *result;
  int uid = -1;
  char *sig_esc = NULL;
  int pgerror;
  size_t pgsize;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_signature: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  if (_ds_match_attribute(CTX->config->attributes, "PgSQLUIDInSignature", "on"))
  {
    char *u, *sig, *username;
    void *dbh = s->dbh;
    int dbh_attached = s->dbh_attached;
    sig = strdup(signature);
    u = strchr(sig, ',');
    if (!u) {
      LOGDEBUG("_ds_get_signature: unable to locate uid in signature");
      free(sig);
      sig = NULL;
      return EFAILURE;
    }
    u[0] = 0;
    u = sig;
    uid = atoi(u);
    free(sig);
    sig = NULL;

    /* Change the context's username and reinitialize storage */

    p = _pgsql_drv_getpwuid (CTX, uid);
    if (p == NULL) {
      LOG(LOG_CRIT, "_ds_get_signature: _pgsql_drv_getpwuid(%d) failed: aborting", uid);
      return EFAILURE;
    }

    username = strdup(p->pw_name);
    _ds_shutdown_storage(CTX);
    free(CTX->username);
    CTX->username = username;
    _ds_init_storage(CTX, (dbh_attached) ? dbh : NULL);
    s = (struct _pgsql_drv_storage *) CTX->storage;
  }

  if (uid == -1) {
    uid = (int) p->pw_uid;
  }

  /* escape the signature */
  sig_esc = malloc(strlen(signature)*2+1);
  if (sig_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EFAILURE;
  }
  pgsize = PQescapeStringConn (s->dbh, sig_esc, signature, strlen(signature), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_get_signature: unable to escape signature '%s'", signature);
    free(sig_esc);
    return EFAILURE;
  }

  snprintf (query, sizeof (query),
            "SELECT data,length FROM dspam_signature_data WHERE uid=%d AND signature='%s'",
            (int) uid, sig_esc);

  free(sig_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if ( PQntuples(result) < 1 )
  {
    LOGDEBUG("_ds_get_signature: failed PQntuples()");
    if (result) PQclear(result);
    return EFAILURE;
  }

  if ( PQgetlength(result, 0, 0) == 0)
  {
    LOGDEBUG("_ds_get_signature: PQgetlength() failed");
    if (result) PQclear(result);
    return EFAILURE;
  }

  mem = PQunescapeBytea((unsigned char *) PQgetvalue(result,0,0), &length);
  SIG->length = strtoul (PQgetvalue(result,0,1), NULL, 0);
  if (SIG->length == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to signature data length", PQgetvalue(result,0,1));
    SIG->length = 0;
    PQFREEMEM(mem);
    if (result) PQclear(result);
    return EFAILURE;
  }
  mem2 = calloc(1, length+1);
  if (!mem2) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    SIG->length = 0;
    PQFREEMEM(mem);
    if (result) PQclear(result);
    return EUNKNOWN;
  }

  memcpy(mem2, mem, length);
  PQFREEMEM(mem);
  if (SIG->data)
    free(SIG->data);
  SIG->data = (void *) mem2;

  if (result) PQclear(result);
  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  size_t length;
  unsigned char *mem;
  char scratch[1024];
  buffer *query;
  PGresult *result;
  struct passwd *p;
  char *name;
  char *sig_esc = NULL;
  int pgerror;
  size_t pgsize;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_signature: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  mem = PQescapeByteaConn(s->dbh, SIG->data, SIG->length, &length);

  /* escape the signature */
  sig_esc = malloc(strlen(signature)*2+1);
  if (sig_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EFAILURE;
  }
  pgsize = PQescapeStringConn (s->dbh, sig_esc, signature, strlen(signature), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_set_signature: unable to escape signature '%s'", signature);
    free(sig_esc);
    return EFAILURE;
  }

  snprintf (scratch, sizeof (scratch),
            "INSERT INTO dspam_signature_data (uid,signature,length,created_on,data) VALUES (%d,'%s',%lu,CURRENT_DATE,'",
            (int) p->pw_uid, sig_esc, (unsigned long) SIG->length);
  free(sig_esc);
  buffer_cat (query, scratch);
  buffer_cat (query, (const char *) mem);
  buffer_cat (query, "')");
  PQFREEMEM(mem);
  mem = NULL;

  result = PQexec(s->dbh, query->data);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query->data);
    if (result) PQclear(result);
    buffer_destroy(query);
    return EFAILURE;
  }

  buffer_destroy(query);
  if (result) PQclear(result);
  return 0;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  PGresult *result;
  char *sig_esc = NULL;
  int pgerror;
  size_t pgsize;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  /* escape the signature */
  sig_esc = malloc(strlen(signature)*2+1);
  if (sig_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EFAILURE;
  }
  pgsize = PQescapeStringConn (s->dbh, sig_esc, signature, strlen(signature), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_delete_signature: unable to escape signature '%s'", signature);
    free(sig_esc);
    return EFAILURE;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM dspam_signature_data WHERE uid=%d AND signature='%s'",
            (int) p->pw_uid, sig_esc);

  free(sig_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if (result) PQclear(result);
  return 0;
}

int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  PGresult *result;
  char *sig_esc = NULL;
  int pgerror;
  size_t pgsize;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  /* escape the signature */
  sig_esc = malloc(strlen(signature)*2+1);
  if (sig_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EFAILURE;
  }
  pgsize = PQescapeStringConn (s->dbh, sig_esc, signature, strlen(signature), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_verify_signature: unable to escape signature '%s'", signature);
    free(sig_esc);
    return EFAILURE;
  }

  snprintf (query, sizeof (query),
            "SELECT signature FROM dspam_signature_data WHERE uid=%d AND signature='%s'",
            (int) p->pw_uid, sig_esc);

  free(sig_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }

  if ( PQntuples(result) < 1 )
  {
    if (result) PQclear(result);
    return -1;
  }

  if (result) PQclear(result);
  return 0;
}

/*
 * _ds_get_nextuser()
 *
 * DESCRIPTION
 *   The _ds_get_nextuser() function is called to get the next user from the
 *   classification context. Calling this function repeatedly will return all
 *   users one by one.
 *
 * RETURN VALUES
 *   returns username on success, NULL on failure or when all usernames have
 *   already been returned for the classification context. When there are no
 *   more users to return then iter_user of the storage driver is set to NULL.
 */

char *
_ds_get_nextuser (DSPAM_CTX * CTX)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
#ifndef VIRTUAL_USERS
  struct passwd *p;
#else
  char *virtual_table, *virtual_username;
#endif
  uid_t uid;
  char query[512];
  PGresult *result;

  if (s->dbh == NULL) {
    LOGDEBUG ("_ds_get_nextuser: invalid database handle (NULL)");
    return NULL;
  }

#ifdef VIRTUAL_USERS
  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "PgSQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }
#endif

  /* Set the notice processor to prevent notices from being written to stderr */
  PQsetNoticeReceiver(s->dbh, (PQnoticeReceiver) _pgsql_drv_notice_receiver, NULL);
  PQsetNoticeProcessor(s->dbh, (PQnoticeProcessor) _pgsql_drv_notice_processor, NULL);

  if (s->iter_user == NULL) {
    /* libpq do not have fetch_row() so we have to use cursor */

    /* Start transaction block */
    result = PQexec(s->dbh, "BEGIN");
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), "_ds_get_nextuser: BEGIN command failed");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      return NULL;
    }
    if (result)
      PQclear(result);
    result = NULL;

    /* Declare Cursor */
#ifdef VIRTUAL_USERS
    snprintf(query, sizeof(query), "DECLARE dsnucursor CURSOR FOR SELECT DISTINCT %s FROM %s",
        virtual_username,
        virtual_table);
#else
    strlcpy (query, "DECLARE dsnucursor CURSOR FOR SELECT DISTINCT uid FROM dspam_stats", sizeof(query));
#endif

    result = PQexec(s->dbh, query);
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), query);
      if (result) PQclear(result);
      result = PQexec(s->dbh, "CLOSE dsnucursor");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      return NULL;
    }
    if (result) PQclear(result);

  } else
    PQclear(s->iter_user);

  s->iter_user = PQexec(s->dbh, "FETCH NEXT FROM dsnucursor");
  if ( PQresultStatus(s->iter_user) != PGRES_TUPLES_OK && PQresultStatus(s->iter_user) != PGRES_NONFATAL_ERROR ) {
    _pgsql_drv_query_error (PQresultErrorMessage(s->iter_user), "FETCH NEXT command failed");
    result = PQexec(s->dbh, "CLOSE dsnucursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_user) PQclear(s->iter_user);
    s->iter_user = NULL;
    return NULL;
  }

  if ( PQntuples(s->iter_user) < 1 ) {
    result = PQexec(s->dbh, "CLOSE dsnucursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_user) PQclear(s->iter_user);
    s->iter_user = NULL;
    return NULL;
  }

  uid = (uid_t) atoi (PQgetvalue(s->iter_user,0,0));
  if (uid == INT_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nextuser: failed converting %s to uid", PQgetvalue(s->iter_user,0,0));
    return NULL;
  }

#ifdef VIRTUAL_USERS
  strlcpy (s->u_getnextuser, PQgetvalue(s->iter_user,0,0), sizeof (s->u_getnextuser));
#else
  p = _pgsql_drv_getpwuid (CTX, uid);
  if (p == NULL)
    return NULL;

  strlcpy (s->u_getnextuser, p->pw_name, sizeof (s->u_getnextuser));
#endif

  return s->u_getnextuser;
}

/*
 * _ds_get_nexttoken()
 *
 * DESCRIPTION
 *   The _ds_get_nexttoken() function is called to get the next token from the
 *   classification context. Calling this function repeatedly will return all
 *   tokens for a user or group one by one.
 *
 * RETURN VALUES
 *   returns token on success, NULL on failure or when all tokens have already
 *   been returned for the user or group. When there are no more tokens to return
 *   then iter_token of the storage driver is set to NULL.
 */

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct _ds_storage_record *st;
  char query[256];
  struct passwd *p;
  char *name;
  PGresult *result;

  if (s->dbh == NULL) {
    LOGDEBUG ("_ds_get_nexttoken: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL) {
    LOGDEBUG ("_ds_get_nexttoken: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_record));
  if (st == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  /* Set the notice processor to prevent notices from being written to stderr */
  PQsetNoticeReceiver(s->dbh, (PQnoticeReceiver) _pgsql_drv_notice_receiver, NULL);
  PQsetNoticeProcessor(s->dbh, (PQnoticeProcessor) _pgsql_drv_notice_processor, NULL);

  if (s->iter_token == NULL) {
    /* libpq do not have fetch_row() so we have to use cursor */

    /* Start transaction block */
    result = PQexec(s->dbh, "BEGIN");
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), "_ds_get_nextsignature: BEGIN command failed");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      goto FAIL;
    }
    if (result) PQclear(result);

    /* Declare Cursor */
    snprintf (query, sizeof (query),
              "DECLARE dsntcursor CURSOR FOR SELECT"
              " token,spam_hits,innocent_hits,date_part('epoch',last_hit)"
              " FROM dspam_token_data WHERE uid=%d",
              (int) p->pw_uid);

    result = PQexec(s->dbh, query);
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), query);
      if (result) PQclear(result);
      result = PQexec(s->dbh, "CLOSE dsntcursor");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      goto FAIL;
    }
    if (result) PQclear(result);

  } else
    PQclear(s->iter_token);

  s->iter_token = PQexec(s->dbh, "FETCH NEXT FROM dsntcursor");
  if ( PQresultStatus(s->iter_token) != PGRES_TUPLES_OK && PQresultStatus(s->iter_token) != PGRES_NONFATAL_ERROR ) {
    _pgsql_drv_query_error (PQresultErrorMessage(s->iter_token), "FETCH NEXT command failed");
    result = PQexec(s->dbh, "CLOSE dsntcursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_token) PQclear(s->iter_token);
    s->iter_token = NULL;
    goto FAIL;
  }

  if ( PQntuples(s->iter_token) < 1 ) {
    result = PQexec(s->dbh, "CLOSE dsntcursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_token) PQclear(s->iter_token);
    s->iter_token = NULL;
    goto FAIL;
  }

  st->token = _pgsql_drv_token_read (s->pg_token_type, PQgetvalue( s->iter_token, 0, 0));
  st->spam_hits = strtoul (PQgetvalue( s->iter_token, 0, 1), NULL, 0);
  if (st->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->spam_hits", PQgetvalue(s->iter_token,0,1));
    goto FAIL;
  }
  st->innocent_hits = strtoul (PQgetvalue( s->iter_token, 0, 2), NULL, 0);
  if (st->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->innocent_hits", PQgetvalue(s->iter_token,0,2));
    goto FAIL;
  }
  st->last_hit = (time_t) strtol ( PQgetvalue( s->iter_token, 0, 3), NULL, 0);
  if (st->last_hit == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->last_hit", PQgetvalue(s->iter_token,0,3));
    goto FAIL;
  }

  return st;

FAIL:
  free(st);
  return NULL;
}

/*
 * _ds_get_nextsignature()
 *
 * DESCRIPTION
 *   The _ds_get_nextsignature() function is called to get the next signature
 *   from the classification context. Calling this function repeatedly will return
 *   all signatures for a user or group one by one.
 *
 * RETURN VALUES
 *   returns signature on success, NULL on failure or when all signatures have
 *   already been returned for the user or group. When there are no more signatures
 *   to return then iter_sig of the storage driver is set to NULL.
 */

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct _ds_storage_signature *st;
  size_t length;
  char query[256];
  PGresult *result;
  struct passwd *p;
  char *name;
  unsigned char *mem;

  if (s->dbh == NULL) {
    LOGDEBUG ("_ds_get_nextsignature: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL) {
    LOGDEBUG ("_ds_get_nextsignature: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_signature));
  if (st == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  /* Set the notice processor to prevent notices from being written to stderr */
  PQsetNoticeReceiver(s->dbh, (PQnoticeReceiver) _pgsql_drv_notice_receiver, NULL);
  PQsetNoticeProcessor(s->dbh, (PQnoticeProcessor) _pgsql_drv_notice_processor, NULL);

  if (s->iter_sig == NULL) {
    /* libpq do not have fetch_row() so we have to use cursor */

    /* Start transaction block */
    result = PQexec(s->dbh, "BEGIN");
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), "_ds_get_nextsignature: BEGIN command failed");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      goto FAIL;
    }
    if (result) PQclear(result);

    /* Declare Cursor */
    snprintf (query, sizeof (query),
              "DECLARE dsnscursor CURSOR FOR SELECT"
              " data,signature,length,date_part('epoch',created_on)"
              " FROM dspam_signature_data WHERE uid=%d",
              (int) p->pw_uid);

    result = PQexec(s->dbh, query);
    if ( PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR ) {
      _pgsql_drv_query_error (PQresultErrorMessage(result), query);
      if (result) PQclear(result);
      result = PQexec(s->dbh, "CLOSE dsnscursor");
      if (result) PQclear(result);
      result = PQexec(s->dbh, "END");
      if (result) PQclear(result);
      goto FAIL;
    }
    if (result) PQclear(result);

  } else
    PQclear(s->iter_sig);

  s->iter_sig = PQexec(s->dbh, "FETCH NEXT FROM dsnscursor");
  if ( PQresultStatus(s->iter_sig) != PGRES_TUPLES_OK && PQresultStatus(s->iter_sig) != PGRES_NONFATAL_ERROR ) {
    _pgsql_drv_query_error (PQresultErrorMessage(s->iter_sig), "FETCH NEXT command failed");
    result = PQexec(s->dbh, "CLOSE dsnscursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_sig) PQclear(s->iter_sig);
    s->iter_sig = NULL;
    goto FAIL;
  }

  if ( PQntuples(s->iter_sig) < 1 ) {
    result = PQexec(s->dbh, "CLOSE dsnscursor");
    if (result) PQclear(result);
    result = PQexec(s->dbh, "END");
    if (result) PQclear(result);
    if (s->iter_sig) PQclear(s->iter_sig);
    s->iter_sig = NULL;
    goto FAIL;
  }

  if ( PQgetlength(s->iter_sig, 0, 0) == 0 ) {
    if (s->iter_sig) PQclear(s->iter_sig);
    s->iter_sig = NULL;
    goto FAIL;
  }

  mem = PQunescapeBytea( (unsigned char *) PQgetvalue( s->iter_sig, 0, 0), &length );
  // memcpy (mem, row[0], lengths[0]);
  st->data = malloc(length);
  if (st->data == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    PQFREEMEM(mem);
    goto FAIL;
  }

  memcpy(st->data, mem, length);
  PQFREEMEM(mem);
  strlcpy (st->signature, PQgetvalue(s->iter_sig, 0, 1), sizeof (st->signature));
  st->length = strtoul (PQgetvalue(s->iter_sig, 0, 2), NULL, 0);
  if (st->length == LONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nextsignature: failed converting %s to st->length", PQgetvalue(s->iter_sig,0,2));
    free(st->data);
    goto FAIL;
  }
  st->created_on = (time_t) strtol (PQgetvalue(s->iter_sig, 0, 3), NULL, 0);
  if (st->created_on == LONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nextsignature: failed converting %s to st->created_on", PQgetvalue(s->iter_sig,0,3));
    free(st->data);
    goto FAIL;
  }

  return st;

FAIL:
  free(st);
  return NULL;
}

struct passwd *
_pgsql_drv_getpwnam (DSPAM_CTX * CTX, const char *name)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;

#ifndef VIRTUAL_USERS
  struct passwd *q;
#if defined(_REENTRANT) && defined(HAVE_GETPWNAM_R)
  struct passwd pwbuf;
  char buf[1024];
#endif

  if (name == NULL)
    return NULL;

  if (s->p_getpwnam.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (s->p_getpwnam.pw_name, name))
      return &s->p_getpwnam;

    free (s->p_getpwnam.pw_name);
    s->p_getpwnam.pw_name = NULL;
    s->p_getpwnam.pw_uid = 0;
  }

#if defined(_REENTRANT) && defined(HAVE_GETPWNAM_R)
  if (getpwnam_r(name, &pwbuf, buf, sizeof(buf), &q))
    q = NULL;
#else
  q = getpwnam (name);
#endif

  if (q == NULL)
    return NULL;
  s->p_getpwnam.pw_uid = q->pw_uid;
  s->p_getpwnam.pw_name = strdup (q->pw_name);

  return &s->p_getpwnam;
#else
  char query[512];
  PGresult *result;
  char *virtual_table, *virtual_uid, *virtual_username;
  char *name_esc = NULL;
  int pgerror;
  size_t pgsize;

  if (s->p_getpwnam.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (s->p_getpwnam.pw_name, name)) {
      LOGDEBUG("_pgsql_drv_getpwnam: returning cached name '%s'", name);
      return &s->p_getpwnam;
    }

    free (s->p_getpwnam.pw_name);
    s->p_getpwnam.pw_name = NULL;
  }

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "PgSQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

  /* escape the user name */
  name_esc = malloc(strlen(name)*2+1);
  if (name_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }
  pgsize = PQescapeStringConn (s->dbh, name_esc, name, strlen(name), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_pgsql_drv_getpwnam: unable to escape user name '%s'", name);
    free(name_esc);
    return NULL;
  }

  snprintf (query, sizeof (query),
            "SELECT %s FROM %s WHERE %s='%s'",
            virtual_uid, virtual_table, virtual_username, name_esc);

  free(name_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    LOGDEBUG ("_pgsql_drv_getpwnam: unable to run query: %s", query);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS) {
      LOGDEBUG("_pgsql_drv_getpwnam: returning NULL for query on name: %s that returned a null result", name);
      if (result) PQclear(result);
      return NULL;
    }
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return NULL;
  }

  if ( PQntuples(result) < 1 )
  {
    if (result) PQclear(result);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS) {
      LOGDEBUG("_pgsql_drv_getpwnam: returning NULL for query on name: %s that returned a null result", name);
      return NULL;
    }
    return _pgsql_drv_setpwnam (CTX, name);
  }

  if ( PQgetvalue(result, 0, 0) == NULL )
  {
    if (result) PQclear(result);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS) {
      LOGDEBUG("_pgsql_drv_getpwnam: returning NULL for query on name: %s", name);
      return NULL;
    }
    LOGDEBUG("_pgsql_drv_getpwnam: setting, then returning passed name: %s", name);
    return _pgsql_drv_setpwnam (CTX, name);
  }

  s->p_getpwnam.pw_uid = atoi(PQgetvalue(result, 0, 0));
  if (s->p_getpwnam.pw_uid == INT_MAX && errno == ERANGE) {
    LOGDEBUG("_pgsql_drv_getpwnam: failed converting %s to s->p_getpwnam.pw_uid", PQgetvalue(result,0,0));
    if (result) PQclear(result);
    return NULL;
  }
  if (name == NULL)
    s->p_getpwnam.pw_name = strdup("");
  else
    s->p_getpwnam.pw_name = strdup (name);

  if (result) PQclear(result);
  LOGDEBUG("_pgsql_drv_getpwnam: successful returning struct for name: %s", s->p_getpwnam.pw_name);
  return &s->p_getpwnam;

#endif
}

struct passwd *
_pgsql_drv_getpwuid (DSPAM_CTX * CTX, uid_t uid)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
#ifndef VIRTUAL_USERS
  struct passwd *q;
#if defined(_REENTRANT) && defined(HAVE_GETPWUID_R)
  struct passwd pwbuf;
  char buf[1024];
#endif

  if (s->p_getpwuid.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (s->p_getpwuid.pw_uid == uid)
    {
      return &s->p_getpwuid;
    }
    free(s->p_getpwuid.pw_name);
    s->p_getpwuid.pw_name = NULL;
  }

#if defined(_REENTRANT) && defined(HAVE_GETPWUID_R)
  if (getpwuid_r(uid, &pwbuf, buf, sizeof(buf), &q))
    q = NULL;
#else
  q = getpwuid (uid);
#endif

  if (q == NULL)
   return NULL;

  if (s->p_getpwuid.pw_name) {
    free(s->p_getpwuid.pw_name);
    s->p_getpwuid.pw_name = NULL;
  }

  memcpy (&s->p_getpwuid, q, sizeof (struct passwd));
  s->p_getpwuid.pw_name = strdup(q->pw_name);

  return &s->p_getpwuid;
#else
  char query[512];
  PGresult *result;
  char *virtual_table, *virtual_uid, *virtual_username;

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "PgSQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

  if (s->p_getpwuid.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (s->p_getpwuid.pw_uid == uid)
      return &s->p_getpwuid;
    free (s->p_getpwuid.pw_name);
    s->p_getpwuid.pw_name = NULL;
  }

  snprintf (query, sizeof (query),
            "SELECT %s FROM %s WHERE %s=%d",
            virtual_username, virtual_table, virtual_uid, (int) uid);

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return NULL;
  }

  if ( PQntuples(result) < 1 )
  {
    if (result) PQclear(result);
    return NULL;
  }

  if ( PQgetvalue(result, 0, 0) == NULL )
  {
    if (result) PQclear(result);
    return NULL;
  }

  s->p_getpwuid.pw_name = strdup ( PQgetvalue(result, 0, 0) );
  s->p_getpwuid.pw_uid = (int) uid;

  if (result) PQclear(result);
  return &s->p_getpwuid;
#endif
}

void
_pgsql_drv_query_error (const char *error, const char *query)
{
  FILE *file;
  char fn[MAX_FILENAME_LENGTH];
  char buf[26];

  LOG (LOG_WARNING, "query error: %s: see sql.errors for more details",
       error);

  snprintf (fn, sizeof (fn), "%s/sql.errors", LOGDIR);

  file = fopen (fn, "a");

  if (file == NULL)
  {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, fn, strerror (errno));
    return;
  }

  fprintf (file, "[%s] %d: %s: %s\n", format_date_r(buf), (int) getpid (), error, query);
  fclose (file);
  return;
}

#ifdef VIRTUAL_USERS
struct passwd *
_pgsql_drv_setpwnam (DSPAM_CTX * CTX, const char *name)
{
  if (name == NULL)
    return NULL;

  char query[512];
  char *virtual_table, *virtual_uid, *virtual_username;
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  PGresult *result;
  char *name_esc = NULL;
  int pgerror;
  size_t pgsize;

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "PgSQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "PgSQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

#ifdef EXT_LOOKUP
  LOGDEBUG("_pgsql_drv_setpwnam: verified_user is %d", verified_user);
  if (verified_user == 0) {
    LOGDEBUG("_pgsql_drv_setpwnam: External lookup verification of %s failed: not adding user", name);
    return NULL;
  }
#endif

  /* escape the user name */
  name_esc = malloc(strlen(name)*2+1);
  if (name_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }
  pgsize = PQescapeStringConn (s->dbh, name_esc, name, strlen(name), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_pgsql_drv_setpwnam: unable to escape user name '%s'", name);
    free(name_esc);
    return NULL;
  }

  snprintf (query, sizeof (query),
            "INSERT INTO %s (%s, %s) VALUES (default, '%s')",
            virtual_table, virtual_uid, virtual_username, name_esc);

  free(name_esc);

  /* we need to fail, to prevent a potential loop - even if it was inserted
   * by another process */

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return NULL;
  }

  if (result) PQclear(result);
  return _pgsql_drv_getpwnam (CTX, name);
}
#endif

int
_ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  PGresult *result;
  char tok_buf[30];

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_del_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_del_spamrecord: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM dspam_token_data WHERE uid=%d AND token=%s",
            (int) p->pw_uid,
            _pgsql_drv_token_write (s->pg_token_type, token, tok_buf, sizeof(tok_buf)) );

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    return EFAILURE;
  }
  if (result) PQclear(result);

  return 0;
}

int _ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _pgsql_drv_storage *s = (struct _pgsql_drv_storage *) CTX->storage;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  buffer *query;
  char scratch[1024];
  char queryhead[1024];
  struct passwd *p;
  char *name;
  int writes = 0;
  PGresult *result;

  if (diction->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _pgsql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _pgsql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: unable to _pgsql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (queryhead, sizeof(queryhead),
            "DELETE FROM dspam_token_data"
            " WHERE uid=%d AND token IN (",
            (int) p->pw_uid);

  buffer_cat (query, queryhead);

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    _pgsql_drv_token_write (s->pg_token_type, ds_term->key, scratch, sizeof(scratch));
    buffer_cat (query, scratch);
    ds_term = ds_diction_next(ds_c);

    if (writes > 2500 || ds_term == NULL) {
      buffer_cat (query, ")");

      result = PQexec(s->dbh, query->data);
      if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
      {
        _pgsql_drv_query_error (PQresultErrorMessage(result), query->data);
        if (result) PQclear(result);
        buffer_destroy(query);
        ds_diction_close(ds_c);
        return EFAILURE;
      }
      if (result) PQclear(result);
      result = NULL;

      buffer_copy(query, queryhead);
      writes = 0;

    } else {
      writes++;
      if (ds_term != NULL)
        buffer_cat (query, ",");
    }
  }
  ds_diction_close(ds_c);

  if (writes) {
    buffer_cat (query, ")");

    result = PQexec(s->dbh, query->data);
    if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
    {
      _pgsql_drv_query_error (PQresultErrorMessage(result), query->data);
      if (result) PQclear(result);
      buffer_destroy(query);
      return EFAILURE;
    }
    if (result) PQclear(result);
  }

  if (result) PQclear(result);
  buffer_destroy (query);
  return 0;
}

#ifdef PREFERENCES_EXTENSION
DSPAM_CTX *_pgsql_drv_init_tools(
 const char *home,
 config_t config,
 void *dbh,
 int mode)
{
  DSPAM_CTX *CTX;
  struct _pgsql_drv_storage *s;
  int dbh_attached = (dbh) ? 1 : 0;

  CTX = dspam_create (NULL, NULL, home, mode, 0);

  if (CTX == NULL)
    return NULL;

  _pgsql_drv_set_attributes(CTX, config);

  if (!dbh)
    dbh = _pgsql_drv_connect(CTX);

  if (!dbh)
    goto BAIL;

  if (dspam_attach(CTX, dbh))
    goto BAIL;

  s = (struct _pgsql_drv_storage *) CTX->storage;
  s->dbh_attached = dbh_attached;

  return CTX;

BAIL:
  LOGDEBUG ("_pgsql_drv_init_tools: Bailing and returning NULL!");
  dspam_destroy(CTX);
  return NULL;
}

agent_pref_t _ds_pref_load(
  config_t config,
  const char *username,
  const char *home,
  void *dbh)
{
  struct _pgsql_drv_storage *s;
  struct passwd *p;
  char query[512];
  PGresult *result;
  DSPAM_CTX *CTX;
  agent_pref_t PTX;
  agent_attrib_t pref;
  int uid, ntuples, i = 0;

  CTX = _pgsql_drv_init_tools(home, config, dbh, DSM_TOOLS);
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "_ds_pref_load: unable to initialize tools context");
    return NULL;
  }

  s = (struct _pgsql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _pgsql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_load: unable to _pgsql_drv_getpwnam(%s)",
              username);
      dspam_destroy(CTX);
      return NULL;
    } else {
      uid = (int) p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  LOGDEBUG("Loading preferences for uid %d", uid);

  snprintf(query, sizeof(query), "SELECT preference,value"
                              " FROM dspam_preferences WHERE uid=%d", (int) uid);

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_TUPLES_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    dspam_destroy(CTX);
    return NULL;
  }

  if ( PQntuples(result) < 1 )
  {
    if (result) PQclear(result);
    dspam_destroy(CTX);
    return NULL;
  }

  PTX = malloc(sizeof(agent_attrib_t )*(PQntuples(result)+1));
  if (PTX == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    dspam_destroy(CTX);
    if (result) PQclear(result);
    return NULL;
  }

  PTX[0] = NULL;

  if ( PQgetlength(result, 0, 0) == 0)
  {
    if (result) PQclear(result);
    dspam_destroy(CTX);
    _ds_pref_free(PTX);
    free(PTX);
    return NULL;
  }

  ntuples = PQntuples(result);

  for (i=0; i<ntuples; i++)
  {
    char *p = PQgetvalue(result,i,0);
    char *q = PQgetvalue(result,i,1);

    pref = malloc(sizeof(struct _ds_agent_attribute));
    if (pref == NULL) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      if (result) PQclear(result);
      dspam_destroy(CTX);
      return PTX;
    }

    pref->attribute = strdup(p);
    pref->value = strdup(q);
    PTX[i] = pref;
    PTX[i+1] = NULL;
  }

  if (result) PQclear(result);
  dspam_destroy(CTX);
  return PTX;
}

int _ds_pref_set (
 config_t config,
 const char *username,
 const char *home,
 const char *preference,
 const char *value,
 void *dbh)
{
  struct _pgsql_drv_storage *s;
  struct passwd *p;
  char query[512];
  DSPAM_CTX *CTX;
  int uid;
  PGresult *result;
  char *pref_esc = NULL;
  char *val_esc = NULL;
  int pgerror;
  size_t pgsize;

  CTX = _pgsql_drv_init_tools(home, config, dbh, DSM_PROCESS);
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "_ds_pref_set: unable to initialize tools context");
    goto FAIL;
  }

  s = (struct _pgsql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _pgsql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_set: unable to _pgsql_drv_getpwnam(%s)",
              CTX->username);
      goto FAIL;
    } else {
      uid = (int) p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  /* escape the preference name */
  pref_esc = malloc(strlen(preference)*2+1);
  if (pref_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    goto FAIL;
  }
  pgsize = PQescapeStringConn (s->dbh, pref_esc, preference, strlen(preference), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_pref_set: unable to escape preference '%s'", preference);
    goto FAIL;
  }

  /* escape the preference value */
  val_esc = malloc(strlen(value)*2+1);
  if (val_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    goto FAIL;
  }
  pgsize = PQescapeStringConn (s->dbh, val_esc, value, strlen(value), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_pref_set: unable to escape preference value '%s'", value);
    goto FAIL;
  }

  snprintf(query, sizeof(query), "DELETE FROM dspam_preferences"
    " WHERE uid=%d AND preference='%s'", (int) uid, pref_esc);

  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    result = NULL;
    goto FAIL;
  }
  if (result) PQclear(result);

  snprintf(query, sizeof(query), "INSERT INTO dspam_preferences"
    " (uid,preference,value) VALUES (%d,'%s','%s')", (int) uid, pref_esc, val_esc);

  free(pref_esc);
  free(val_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    result = NULL;
    goto FAIL;
  }

  if (result) PQclear(result);
  dspam_destroy(CTX);
  return 0;

FAIL:
  LOGDEBUG("_ds_pref_set: failed");
  if (pref_esc) free(pref_esc);
  if (val_esc) free(val_esc);
  if (CTX) dspam_destroy(CTX);
  return EFAILURE;
}

int _ds_pref_del (
 config_t config,
 const char *username,
 const char *home,
 const char *preference,
 void *dbh)
{
  struct _pgsql_drv_storage *s;
  struct passwd *p;
  char query[512];
  DSPAM_CTX *CTX;
  int uid;
  PGresult *result;
  char *pref_esc = NULL;
  int pgerror;
  size_t pgsize;

  CTX = _pgsql_drv_init_tools(home, config, dbh, DSM_TOOLS);
  if (CTX == NULL) {
    LOG (LOG_WARNING, "_ds_pref_del: unable to initialize tools context");
    goto FAIL;
  }

  s = (struct _pgsql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _pgsql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_del: unable to _pgsql_drv_getpwnam(%s)",
              username);
      goto FAIL;
    } else {
      uid = (int) p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  /* escape the preference name */
  pref_esc = malloc(strlen(preference)*2+1);
  if (pref_esc == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    goto FAIL;
  }
  pgsize = PQescapeStringConn (s->dbh, pref_esc, preference, strlen(preference), &pgerror);
  if (pgsize == 0 || pgerror != 0) {
    LOGDEBUG ("_ds_pref_del: unable to escape preference '%s'", preference);
    goto FAIL;
  }

  snprintf(query, sizeof(query), "DELETE FROM dspam_preferences"
    " WHERE uid=%d AND preference='%s'", (int) uid, pref_esc);

  free(pref_esc);
  result = PQexec(s->dbh, query);
  if ( !result || (PQresultStatus(result) != PGRES_COMMAND_OK && PQresultStatus(result) != PGRES_NONFATAL_ERROR) )
  {
    _pgsql_drv_query_error (PQresultErrorMessage(result), query);
    if (result) PQclear(result);
    goto FAIL;
  }

  if (result) PQclear(result);
  dspam_destroy(CTX);
  return 0;

FAIL:
  LOGDEBUG("_ds_pref_del: failed");
  if (pref_esc) free(pref_esc);
  if (CTX) dspam_destroy(CTX);
  return EFAILURE;
}

int _pgsql_drv_set_attributes(DSPAM_CTX *CTX, config_t config) {
  int i, ret = 0;
  attribute_t t;
  char *profile;

  profile = _ds_read_attribute(config, "DefaultProfile");

  for(i=0;config[i];i++) {
    t = config[i];

    while(t) {

      if (!strncasecmp(t->key, "PgSQL", 5))
      {
        if (profile == NULL || profile[0] == 0)
        {
          ret += dspam_addattribute(CTX, t->key, t->value);
        }
        else if (strchr(t->key, '.'))
        {
          if (!strcasecmp((strchr(t->key, '.')+1), profile)) {
            char *x = strdup(t->key);
            char *y = strchr(x, '.');
            y[0] = 0;

            ret += dspam_addattribute(CTX, x, t->value);
            free(x);
          }
        }
      }
      t = t->next;
    }
  }

  return 0;
}

#else
/* Preference Stubs for Flat-File */

agent_pref_t _ds_pref_load(config_t config, const char *user,
  const char *home, void *dbh)
{
  return _ds_ff_pref_load(config, user, home, dbh);
}

int _ds_pref_set(config_t config, const char *user, const char *home,
  const char *attrib, const char *value, void *dbh)
{
  return _ds_ff_pref_set(config, user, home, attrib, value, dbh);
}

int _ds_pref_del(config_t config, const char *user, const char *home,
  const char *attrib, void *dbh)
{
  return _ds_ff_pref_del(config, user, home, attrib, dbh);
}

#endif

void *_ds_connect (DSPAM_CTX *CTX)
{
  return (void *) _pgsql_drv_connect(CTX);
}

PGconn *_pgsql_drv_connect(DSPAM_CTX *CTX)
{
  PGconn *dbh;
  FILE *file;
  char filename[MAX_FILENAME_LENGTH];
  char buffer[256];
  char hostname[128] = "";
  char user[64] = "";
  char password[32] = "";
  char db[64] = "";
  int port = 5432, i = 0;

  /* Read Storage Attributes */

  if (_ds_read_attribute(CTX->config->attributes, "PgSQLServer")) {
    char *p;

    strlcpy(hostname,
           _ds_read_attribute(CTX->config->attributes, "PgSQLServer"),
            sizeof(hostname));

    if (_ds_read_attribute(CTX->config->attributes, "PgSQLPort")) {
      port = atoi(_ds_read_attribute(CTX->config->attributes, "PgSQLPort"));
      if (port == INT_MAX && errno == ERANGE) {
        LOGDEBUG("_pgsql_drv_connect: failed converting %s to port", _ds_read_attribute(CTX->config->attributes, "PgSQLPort"));
        goto FAILURE;
      }
    }
    else
      port = 0;

    if ((p = _ds_read_attribute(CTX->config->attributes, "PgSQLUser")))
      strlcpy(user, p, sizeof(user));
    if ((p = _ds_read_attribute(CTX->config->attributes, "PgSQLPass")))
      strlcpy(password, p, sizeof(password));
    if ((p = _ds_read_attribute(CTX->config->attributes, "PgSQLDb")))
      strlcpy(db, p, sizeof(db));

  } else {
    if (!CTX->home) {
      LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
      goto FAILURE;
    }
    snprintf (filename, MAX_FILENAME_LENGTH, "%s/pgsql.data", CTX->home);
    file = fopen (filename, "r");
    if (file == NULL)
    {
      LOG (LOG_WARNING, "unable to open %s for reading: %s",
           filename, strerror (errno));
      goto FAILURE;
    }

    db[0] = 0;

    while (fgets (buffer, sizeof (buffer), file) != NULL)
    {
      chomp (buffer);
      if (!i)
        strlcpy (hostname, buffer, sizeof (hostname));
      else if (i == 1) {
        port = atoi (buffer);
        if (port == INT_MAX && errno == ERANGE) {
          fclose (file);
          LOGDEBUG("_pgsql_drv_connect: failed converting %s to port", buffer);
          goto FAILURE;
        }
      }
      else if (i == 2)
        strlcpy (user, buffer, sizeof (user));
      else if (i == 3)
        strlcpy (password, buffer, sizeof (password));
      else if (i == 4)
        strlcpy (db, buffer, sizeof (db));
      i++;
    }
    fclose (file);
  }

  if (db[0] == 0)
  {
    LOG (LOG_WARNING, "file %s: incomplete pgsql connect data", filename);
    return NULL;
  }

  if (port == 0) port = 5432;

  /* create string and connect to db */
  snprintf (buffer, sizeof (buffer),
            "host='%s' user='%s' dbname='%s' password='%s' port='%d'",
            hostname, user, db, password, port);

  dbh = PQconnectdb(buffer);

  if ( PQstatus(dbh) == CONNECTION_BAD)
  {
    LOG (LOG_WARNING, "%s", PQerrorMessage(dbh) );
    return NULL;
  }

  return dbh;

FAILURE:
  LOGDEBUG("_pgsql_drv_connect: failed");
  return NULL;
}


/*
** The following routines either return or get passed a type value
** which represents the type of the dspam_token_data.token column.
** The possible values & their meanings are:
**    -1 -> Nothing known yet
**     0 -> Uses NUMERIC(20)
**     1 -> Uses BIGINT
*/

int
_pgsql_drv_token_type(struct _pgsql_drv_storage *s, PGresult *result, int column)
{
  int found_type = -1;
  char *type_str;
  char query[1024];
  PGresult *select_res;

  if (result == NULL)
  {
    memset((void *)query, 0, sizeof(query));

    snprintf(query, sizeof(query),
      "SELECT typname FROM pg_type WHERE typelem IN"
        " (SELECT atttypid FROM pg_attribute WHERE attname='token' AND attrelid IN"
          " (SELECT oid FROM pg_class WHERE relname='dspam_token_data'));");

    select_res = PQexec(s->dbh,query);
    if ( !select_res || (PQresultStatus(select_res) != PGRES_TUPLES_OK && PQresultStatus(select_res) != PGRES_NONFATAL_ERROR) )
    {
      _pgsql_drv_query_error (PQresultErrorMessage(select_res), query);
      if (select_res) PQclear(select_res);
      return -1;
    }

    if ( PQntuples(select_res) != 1 )
    {
      if (select_res) PQclear(select_res);
      return -1;
    }

    type_str = PQgetvalue(select_res, 0, 0);
    if (strncasecmp(type_str, "_numeric", 8) == 0) {
      found_type = 0;
    } else if (strncasecmp(type_str, "_int8", 5) == 0) {
      found_type = 1;
    } else {
      LOGDEBUG ("_pgsql_drv_token_type: Failed to get type of dspam_token_data.token from system tables");
      if (select_res) PQclear(select_res);
      return -1;
    }
    if (select_res) PQclear(select_res);
  }
  else
  {
    int col_type = PQftype(result, column);

    if (col_type == NUMERICOID) {
      found_type = 0;
    } else if (col_type == BIGINTOID) {
      found_type = 1;
    } else {
      LOGDEBUG ("_pgsql_drv_token_type: Failed to get type of dspam_token_data.token from result set");
      return -1;
    }
  }
  return found_type;
}

unsigned long long
_pgsql_drv_token_read(int type, char *str)
{
  if (type == 1) {
    return (unsigned long long) strtoll(str, NULL, 0);
  }
  return strtoull(str, NULL, 0);
}

char *
_pgsql_drv_token_write(int type, unsigned long long token, char *buffer, size_t bufsz)
{
  memset((void *)buffer,0,bufsz);
  if (type == 1) {
    snprintf(buffer, bufsz, "%lld", token);
  } else {
    snprintf(buffer, bufsz, "%llu", token);
  }
  return buffer;
}
