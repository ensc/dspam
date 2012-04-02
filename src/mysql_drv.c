/* $Id: mysql_drv.c,v 1.889 2011/10/01 10:22:17 sbajic Exp $ */

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
#include <mysql.h>
#include <limits.h>

/* Work around broken limits.h on debian etch (and possibly others?) */
#if defined __GNUC__ && defined _GCC_LIMITS_H_ && ! defined ULLONG_MAX
#  define ULLONG_MAX ULONG_LONG_MAX
#endif

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
#include "mysql_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"
#include "pref.h"
#include "config_shared.h"

#define MYSQL_RUN_QUERY(A, B) mysql_query(A, B)
#define MYSQL_RUN_REAL_QUERY(A, B, C) mysql_real_query(A, B, C)

/*
 * _mysql_drv_get_UIDInSignature()
 *
 * DESCRIPTION
 *   The _mysql_drv_get_UIDInSignature() function is called to check if the
 *   configuration option MySQLUIDInSignature is turned on or off.
 *
 * RETURN VALUES
 *   Returns 1 if MySQLUIDInSignature is turned "on" and 0 if turned "off".
 */
/*
static int _mysql_drv_get_UIDInSignature (DSPAM_CTX *CTX) {
  static int uid_in_signature = -1;
  if (uid_in_signature > -1) {
    return uid_in_signature;
  } else {
    if (_ds_match_attribute(CTX->config->attributes, "MySQLUIDInSignature", "on"))
      uid_in_signature = 1;
    else
      uid_in_signature = 0;
  }
  return uid_in_signature;
}
*/

/*
 * _mysql_drv_get_virtual_table()
 *
 * DESCRIPTION
 *   The _mysql_drv_get_virtual_table() function is called to get the
 *   virtual table name.
 *
 * RETURN VALUES
 *   Returns the name of the virtual table if defined, otherwise returns
 *   "dspam_virtual_uids".
 */
/*
static char *_mysql_drv_get_virtual_table (DSPAM_CTX *CTX) {
  static char *virtual_table = "*";
  if (virtual_table[0] != '*') {
    return virtual_table;
  } else {
    if ((virtual_table = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualTable")) == NULL) {
      virtual_table = "dspam_virtual_uids";
    }
  }
  return virtual_table;
}
*/

/*
 * _mysql_drv_get_virtual_uid_field()
 *
 * DESCRIPTION
 *   The _mysql_drv_get_virtual_uid_field() function is called to get the
 *   virtual uid field name.
 *
 * RETURN VALUES
 *   Returns the name of the virtual uid field if defined, otherwise returns
 *   "uid".
 */
/*
static char *_mysql_drv_get_virtual_uid_field (DSPAM_CTX *CTX) {
  static char *virtual_uid = "*";
  if (virtual_uid[0] != '*') {
    return virtual_uid;
  } else {
    if ((virtual_uid = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualUIDField")) == NULL) {
      virtual_uid = "uid";
    }
  }
  return virtual_uid;
}
*/

/*
 * _mysql_drv_get_virtual_username_field()
 *
 * DESCRIPTION
 *   The _mysql_drv_get_virtual_username_field() function is called to get the
 *   virtual username field name.
 *
 * RETURN VALUES
 *   Returns the name of the virtual username field if defined, otherwise returns
 *   "username".
 */
/*
static char *_mysql_drv_get_virtual_username_field (DSPAM_CTX *CTX) {
  static char *virtual_username = "*";
  if (virtual_username[0] != '*') {
    return virtual_username;
  } else {
    if ((virtual_username = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualUsernameField")) == NULL) {
      virtual_username = "username";
    }
  }
  return virtual_username;
}
*/

/*
 * _mysql_driver_get_max_packet()
 *
 * DESCRIPTION
 *   The _mysql_driver_get_max_packet() function is called to get the maximum
 *   size of db communication buffer.
 *
 * RETURN VALUES
 *   Returns the maximum size for the db communication buffer. If the maximum
 *   size can not be determined then 1000000 is returned.
 */
static unsigned long _mysql_driver_get_max_packet (MYSQL *dbh) {
  static unsigned long drv_max_packet = 0;
  if (drv_max_packet > 0) {
    return drv_max_packet;
  } else {
    drv_max_packet = 1000000;
  }
  if (dbh) {
    MYSQL_RES *result;
    MYSQL_ROW row;
    char scratch[128];
    snprintf (scratch, sizeof (scratch), "SHOW VARIABLES WHERE variable_name='max_allowed_packet'");
    if (MYSQL_RUN_QUERY (dbh, scratch) == 0) {
      result = mysql_use_result (dbh);
      if (result != NULL) {
        row = mysql_fetch_row (result);
        if (row != NULL) {
          drv_max_packet = strtoul (row[1], NULL, 0);
          if (drv_max_packet == ULONG_MAX && errno == ERANGE) {
            LOGDEBUG("_ds_init_storage: failed converting %s to max_allowed_packet", row[1]);
            drv_max_packet = 1000000;
          }
        }
      }
      mysql_free_result (result);
    }
  }
  return drv_max_packet;
}

int
dspam_init_driver (DRIVER_CTX *DTX)
{
#if defined(MYSQL4_INITIALIZATION) && defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40001
  const char *server_default_groups[]=
  { "server", "embedded", "mysql_SERVER", 0 };

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 50003
  if (mysql_library_init(0, NULL, (char**) server_default_groups)) {
    LOGDEBUG("dspam_init_driver: failed initializing MySQL driver");
    return EFAILURE;
  }
#else
  if (mysql_server_init(0, NULL, (char**) server_default_groups)) {
    LOGDEBUG("dspam_init_driver: failed initializing MySQL driver");
    return EFAILURE;
  }
#endif
#endif

  if (DTX == NULL)
    return 0;

  /* Establish a series of stateful connections */

  if (DTX->flags & DRF_STATEFUL) {
    int i, connection_cache = 3;

    if (_ds_read_attribute(DTX->CTX->config->attributes, "MySQLConnectionCache"))
      connection_cache = atoi(_ds_read_attribute(DTX->CTX->config->attributes, "MySQLConnectionCache"));

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
            _mysql_drv_dbh_t dbt = (_mysql_drv_dbh_t) DTX->connections[i]->dbh;
            mysql_close(dbt->dbh_read);
            if (dbt->dbh_write != dbt->dbh_read)
              mysql_close(dbt->dbh_write);
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

#if defined(MYSQL4_INITIALIZATION) && defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40001
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 50003
  mysql_library_end();
#else
  mysql_server_end();
#endif
#endif
  return 0;
}

int
_mysql_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[512];
  struct passwd *p;
  char *name;
  MYSQL_RES *result;
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;
  struct _ds_spam_totals user, group;
  int uid = -1, gid = -1;
  result = NULL;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: invalid database handle (NULL)");
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
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
              name);
    if (!(CTX->flags & DSF_MERGED))
      return EINVAL;
  } else {
    uid = (int) p->pw_uid;
  }

  if (CTX->group != NULL && CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
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
	      " FROM dspam_stats WHERE uid='%d'",
	      (int) uid);
  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to run query: %s", query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbt->dbh_read);
  if (result == NULL) {
    LOGDEBUG("_mysql_drv_get_spamtotals: failed mysql_use_result()");
    return EFAILURE;
  }

  while ((row = mysql_fetch_row (result)) != NULL) {
    int rid = atoi(row[0]);
    if (rid == INT_MAX && errno == ERANGE) {
      LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to rid", row[0]);
      goto FAIL;
    }
    if (rid == uid) {
      user.spam_learned			= strtoul (row[1], NULL, 0);
      if ((unsigned long int)user.spam_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.spam_learned", row[1]);
        goto FAIL;
      }
      user.innocent_learned		= strtoul (row[2], NULL, 0);
      if ((unsigned long int)user.innocent_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.innocent_learned", row[2]);
        goto FAIL;
      }
      user.spam_misclassified		= strtoul (row[3], NULL, 0);
      if ((unsigned long int)user.spam_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.spam_misclassified", row[3]);
        goto FAIL;
      }
      user.innocent_misclassified	= strtoul (row[4], NULL, 0);
      if ((unsigned long int)user.innocent_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.innocent_misclassified", row[4]);
        goto FAIL;
      }
      user.spam_corpusfed		= strtoul (row[5], NULL, 0);
      if ((unsigned long int)user.spam_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.spam_corpusfed", row[5]);
        goto FAIL;
      }
      user.innocent_corpusfed		= strtoul (row[6], NULL, 0);
      if ((unsigned long int)user.innocent_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.innocent_corpusfed", row[6]);
        goto FAIL;
      }
      if (row[7] != NULL && row[8] != NULL) {
        user.spam_classified		= strtoul (row[7], NULL, 0);
        if ((unsigned long int)user.spam_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.spam_classified", row[7]);
          goto FAIL;
        }
        user.innocent_classified	= strtoul (row[8], NULL, 0);
        if ((unsigned long int)user.innocent_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to user.innocent_classified", row[8]);
          goto FAIL;
        }
      } else {
        user.spam_classified		= 0;
        user.innocent_classified	= 0;
      }
    } else {
      group.spam_learned		= strtoul (row[1], NULL, 0);
      if ((unsigned long int)group.spam_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.spam_learned", row[1]);
        goto FAIL;
      }
      group.innocent_learned		= strtoul (row[2], NULL, 0);
      if ((unsigned long int)group.innocent_learned == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.innocent_learned", row[2]);
        goto FAIL;
      }
      group.spam_misclassified		= strtoul (row[3], NULL, 0);
      if ((unsigned long int)group.spam_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.spam_misclassified", row[3]);
        goto FAIL;
      }
      group.innocent_misclassified	= strtoul (row[4], NULL, 0);
      if ((unsigned long int)group.innocent_misclassified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.innocent_misclassified", row[4]);
        goto FAIL;
      }
      group.spam_corpusfed		= strtoul (row[5], NULL, 0);
      if ((unsigned long int)group.spam_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.spam_corpusfed", row[5]);
        goto FAIL;
      }
      group.innocent_corpusfed		= strtoul (row[6], NULL, 0);
      if ((unsigned long int)group.innocent_corpusfed == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.innocent_corpusfed", row[6]);
        goto FAIL;
      }
      if (row[7] != NULL && row[8] != NULL) {
        group.spam_classified		= strtoul (row[7], NULL, 0);
        if ((unsigned long int)group.spam_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.spam_classified", row[7]);
          goto FAIL;
        }
        group.innocent_classified	= strtoul (row[8], NULL, 0);
        if ((unsigned long int)group.innocent_classified == ULONG_MAX && errno == ERANGE) {
          LOGDEBUG("_mysql_drv_get_spamtotals: failed converting %s to group.innocent_classified", row[8]);
          goto FAIL;
        }
      } else {
        group.spam_classified		= 0;
        group.innocent_classified	= 0;
      }
    }
  }

  mysql_free_result (result);
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
  mysql_free_result (result);
  result = NULL;
  return EFAILURE;
}

int
_mysql_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[1024];
  int query_rc = 0;
  int query_errno = 0;
  int update_any = 0;
  struct _ds_spam_totals user;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_mysql_drv_set_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _mysql_drv_get_spamtotals (CTX);    /* undo changes to in memory totals */
    return 0;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_mysql_drv_set_spamtotals: unable to _mysql_drv_getpwnam(%s)",
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
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
      }
    }
  } else {
    query_rc = -1;
  }

  if (query_rc) {
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
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, buf->data);
      if (query_rc) {
        query_errno = mysql_errno (s->dbt->dbh_write);
        if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
          /* Locking issue. Wait 1 second and then retry the transaction again */
          sleep(1);
          query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, buf->data);
        }
      }

      if (query_rc) {
        _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), buf->data);
        LOGDEBUG ("_mysql_drv_set_spamtotals: unable to run query: %s", buf->data);
        if (CTX->flags & DSF_MERGED)
          memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));
        buffer_destroy (buf);
        return EFAILURE;
      }
      buffer_destroy (buf);
    }
  }

  if (CTX->flags & DSF_MERGED)
    memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  buffer *query;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  char scratch[1024];
  char queryhead[1024];
  MYSQL_RES *result;
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;
  struct _ds_spam_stat stat;
  unsigned long long token = 0;
  int uid = -1, gid = -1;
  result = NULL;

  if (diction->items < 1)
    return 0;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  uid = (int) p->pw_uid;

  if (CTX->group != NULL && CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_ds_getall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
                CTX->group);
      return EINVAL;
    }
    gid = (int) p->pw_uid;
  }

  if (gid < 0) gid = uid;

  stat.spam_hits     = 0;
  stat.innocent_hits = 0;
  stat.probability   = 0.00000;

  /* Get the all spam records but split the query when the query size + 1024
   * reaches max_allowed_packet
   */
  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  if (uid != gid) {
    snprintf (queryhead, sizeof(queryhead),
            "SELECT uid,token,spam_hits,innocent_hits"
            " FROM dspam_token_data WHERE uid IN (%d,%d) AND token IN (",
            (int) uid, (int) gid);
  } else {
    snprintf (queryhead, sizeof(queryhead),
            "SELECT uid,token,spam_hits,innocent_hits"
            " FROM dspam_token_data WHERE uid=%d AND token IN (",
            (int) uid);
  }

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while (ds_term) {
    scratch[0] = 0;
    buffer_copy(query, queryhead);
    while (ds_term) {
      snprintf (scratch, sizeof (scratch), "'%llu'", ds_term->key);
      buffer_cat (query, scratch);
      ds_term->s.innocent_hits = 0;
      ds_term->s.spam_hits = 0;
      ds_term->s.probability = 0.00000;
      ds_term->s.status = 0;
      if((unsigned long)(query->used + 1024) > _mysql_driver_get_max_packet(s->dbt->dbh_read)) {
        LOGDEBUG("_ds_getall_spamrecords: Splitting query at %ld characters", query->used);
        break;
      }
      ds_term = ds_diction_next(ds_c);
      if (ds_term)
        buffer_cat (query, ",");
    }
    buffer_cat (query, ")");

#ifdef VERBOSE
  LOGDEBUG ("mysql query length: %ld\n", query->used);
  _mysql_drv_query_error ("VERBOSE DEBUG (INFO ONLY - NOT AN ERROR)", query->data);
#endif

    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query->data);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_read);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query->data);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query->data);
      LOGDEBUG ("_ds_getall_spamrecords: unable to run query: %s", query->data);
      buffer_destroy(query);
      ds_diction_close(ds_c);
      return EFAILURE;
    }
    result = mysql_use_result (s->dbt->dbh_read);
    if (result == NULL) {
      LOGDEBUG("_ds_getall_spamrecords: failed mysql_use_result()");
      buffer_destroy(query);
      ds_diction_close(ds_c);
      return EFAILURE;
    }
    while ((row = mysql_fetch_row (result)) != NULL) {
      int rid = atoi(row[0]);
      if (rid == INT_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to rid", row[0]);
        ds_diction_close(ds_c);
        goto FAIL;
      }
      token = strtoull (row[1], NULL, 0);
      if (token == ULLONG_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to token", row[1]);
        ds_diction_close(ds_c);
        goto FAIL;
      }
      stat.spam_hits = strtoul (row[2], NULL, 0);
      if ((unsigned long int)stat.spam_hits == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.spam_hits", row[2]);
        ds_diction_close(ds_c);
        goto FAIL;
      }
      stat.innocent_hits = strtoul (row[3], NULL, 0);
      if ((unsigned long int)stat.innocent_hits == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.innocent_hits", row[3]);
        ds_diction_close(ds_c);
        goto FAIL;
      }
      stat.status = 0;
      if (rid == uid)
        stat.status |= TST_DISK;
      ds_diction_addstat(diction, token, &stat);
    }
    mysql_free_result (result);
    result = NULL;
    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);
  buffer_destroy (query);
  mysql_free_result (result);
  result = NULL;

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
  mysql_free_result (result);
  result = NULL;
  return EFAILURE;
}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_spam_stat control, stat;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  char queryhead[1024];
  buffer *query;
  char scratch[1024];
  buffer *buf;
  int query_rc = 0;
  int query_errno = 0;
  struct passwd *p;
  char *name;
  int update_any = 0;
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40100
  char inserthead[1024];
  buffer *insert;
  int insert_any = 0;
#endif

  if (diction->items < 1)
    return 0;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY &&
     (CTX->training_mode != DST_TOE ||
     (diction->whitelist_token == 0 && (!(CTX->flags & DSF_NOISE)))))
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }
  buf = buffer_create (NULL);
  if (buf == NULL)
  {
    buffer_destroy(query);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40100
  insert = buffer_create(NULL);
  if (insert == NULL)
  {
    buffer_destroy(query);
    buffer_destroy(buf);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }
#endif

  ds_diction_getstat(diction, s->control_token, &control);
  snprintf (scratch, sizeof (scratch),
            "UPDATE dspam_token_data SET last_hit=CURRENT_DATE()");
  buffer_copy (buf, scratch);
  /* Do not update spam hits if no change is needed */
  int sh_adiff = abs(control.spam_hits - s->control_sh);
  if (sh_adiff != 0) {
    if (control.spam_hits > s->control_sh) {
      snprintf (scratch, sizeof (scratch),
                ",spam_hits=spam_hits+%d",
                sh_adiff);
    } else {
      snprintf (scratch, sizeof (scratch),
                ",spam_hits=IF(spam_hits<%d,0,spam_hits-%d)",
                sh_adiff + 1,
                sh_adiff);
    }
    buffer_cat (buf, scratch);
  }
  /* Do not update innocent hits if no change is needed */
  int ih_adiff = abs(control.innocent_hits - s->control_ih);
  if (ih_adiff != 0) {
    if (control.innocent_hits > s->control_ih) {
      snprintf (scratch, sizeof (scratch),
                ",innocent_hits=innocent_hits+%d",
                ih_adiff);
    } else {
      snprintf (scratch, sizeof (scratch),
                ",innocent_hits=IF(innocent_hits<%d,0,innocent_hits-%d)",
                ih_adiff + 1,
                ih_adiff);
    }
    buffer_cat (buf, scratch);
  }
  snprintf (scratch, sizeof (scratch),
            " WHERE uid=%d AND token IN (",
            (int) p->pw_uid);
  buffer_cat (buf, scratch);

  /* Query head used for update */
  snprintf (queryhead, sizeof (queryhead),
            "%s",
            buf->data);
  buffer_destroy (buf);

  buffer_copy (query, queryhead);

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40100
  snprintf (inserthead, sizeof(inserthead),
            "INSERT INTO dspam_token_data(uid,token,spam_hits,"
            "innocent_hits,last_hit) VALUES");
  buffer_copy (insert, inserthead);
#endif

  /*
   *  Add each token in the diction to either an update or an insert queue
   */

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    int use_comma = 0;
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
      char ins[1024];
#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40100
      snprintf (ins, sizeof (ins),
                "%s(%d,'%llu',%d,%d,CURRENT_DATE())",
                 (insert_any) ? "," : "",
                 (int) p->pw_uid,
                 ds_term->key,
                 stat.spam_hits > 0 ? 1 : 0,
                 stat.innocent_hits > 0 ? 1 : 0);

      insert_any = 1;
      buffer_cat(insert, ins);

      if((unsigned long)(insert->used + 1024) > _mysql_driver_get_max_packet(s->dbt->dbh_write)) {
        LOGDEBUG("_ds_setall_spamrecords: Splitting insert query at %ld characters", insert->used);
        if (insert_any) {
          snprintf (scratch, sizeof (scratch),
                    " ON DUPLICATE KEY UPDATE last_hit=CURRENT_DATE()");
          buffer_cat(insert, scratch);
          /* Do not update spam hits if no change is needed */
          if (abs(control.spam_hits - s->control_sh) != 0) {
            if (control.spam_hits > s->control_sh) {
              snprintf (scratch, sizeof (scratch), ",spam_hits=spam_hits+1");
            } else {
              snprintf (scratch, sizeof (scratch), ",spam_hits=IF(spam_hits<2,0,spam_hits-1)");
            }
            buffer_cat(insert, scratch);
          }
          /* Do not update innocent hits if no change is needed */
          if (abs(control.innocent_hits - s->control_ih) != 0) {
            if (control.innocent_hits > s->control_ih) {
              snprintf (scratch, sizeof (scratch), ",innocent_hits=innocent_hits+1");
            } else {
              snprintf (scratch, sizeof (scratch), ",innocent_hits=IF(innocent_hits<2,0,innocent_hits-1)");
            }
            buffer_cat(insert, scratch);
          }
          query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, insert->data);
          if (query_rc) {
            query_errno = mysql_errno (s->dbt->dbh_write);
            if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
              /* Locking issue. Wait 1 second and then retry the transaction again */
              sleep(1);
              query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, insert->data);
            }
          }
          if (query_rc) {
            _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), insert->data);
            LOGDEBUG ("_ds_setall_spamrecords: unable to run insert query: %s", insert->data);
            buffer_destroy(insert);
            return EFAILURE;
          }
        }
        buffer_copy (insert, inserthead);
        insert_any = 0;
      }
#else
      snprintf(ins, sizeof (ins),
               "INSERT INTO dspam_token_data(uid,token,spam_hits,"
               "innocent_hits,last_hit) VALUES (%d,'%llu',%d,%d,"
               "CURRENT_DATE())",
               p->pw_uid,
               ds_term->key,
               stat.spam_hits > 0 ? 1 : 0,
               stat.innocent_hits > 0 ? 1 : 0);

      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, ins);
      if (query_rc) {
        query_errno = mysql_errno (s->dbt->dbh_write);
        if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
          /* Locking issue. Wait 1 second and then retry the transaction again */
          sleep(1);
          query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, ins);
        }
      }
      if (query_rc)
        stat.status |= TST_DISK;
#endif
    }

    if (stat.status & TST_DISK) {
      snprintf (scratch, sizeof (scratch), "'%llu'", ds_term->key);
      buffer_cat (query, scratch);
      update_any = 1;
      use_comma = 1;
    }

    ds_term->s.status |= TST_DISK;

    ds_term = ds_diction_next(ds_c);
    if((unsigned long)(query->used + 1024) > _mysql_driver_get_max_packet(s->dbt->dbh_write)) {
      LOGDEBUG("_ds_setall_spamrecords: Splitting update query at %ld characters", query->used);
      buffer_cat (query, ")");
      if (update_any) {
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
        if (query_rc) {
          query_errno = mysql_errno (s->dbt->dbh_write);
          if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
            /* Locking issue. Wait 1 second and then retry the transaction again */
            sleep(1);
            query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
          }
        }
        if (query_rc) {
          _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query->data);
          LOGDEBUG ("_ds_setall_spamrecords: unable to run update query: %s", query->data);
          buffer_destroy(query);
          ds_diction_close(ds_c);
          return EFAILURE;
        }
      }
      buffer_copy (query, queryhead);
    } else if (ds_term && use_comma)
      buffer_cat (query, ",");
  }
  ds_diction_close(ds_c);

  /* Just incase */

  if (query->used && query->data[strlen (query->data) - 1] == ',')
  {
    query->used--;
    query->data[strlen (query->data) - 1] = 0;
  }

  buffer_cat (query, ")");

  LOGDEBUG("Control: [%ld %ld] [%lu %lu] Delta: [%lu %lu]",
    s->control_sh, s->control_ih,
    control.spam_hits, control.innocent_hits,
    control.spam_hits - s->control_sh, control.innocent_hits - s->control_ih);

  if (update_any) {
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query->data);
      LOGDEBUG ("_ds_setall_spamrecords: unable to run update query: %s", query->data);
      buffer_destroy(query);
      return EFAILURE;
    }
  }
  buffer_destroy (query);

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40100
  if (insert_any) {
    snprintf (scratch, sizeof (scratch),
              " ON DUPLICATE KEY UPDATE last_hit=CURRENT_DATE()");
    buffer_cat(insert, scratch);
    /* Do not update spam hits if no change is needed */
    if (abs(control.spam_hits - s->control_sh) != 0) {
      if (control.spam_hits > s->control_sh) {
        snprintf (scratch, sizeof (scratch), ",spam_hits=spam_hits+1");
      } else {
        snprintf (scratch, sizeof (scratch), ",spam_hits=IF(spam_hits<2,0,spam_hits-1)");
      }
      buffer_cat(insert, scratch);
    }
    /* Do not update innocent hits if no change is needed */
    if (abs(control.innocent_hits - s->control_ih) != 0) {
      if (control.innocent_hits > s->control_ih) {
        snprintf (scratch, sizeof (scratch), ",innocent_hits=innocent_hits+1");
      } else {
        snprintf (scratch, sizeof (scratch), ",innocent_hits=IF(innocent_hits<2,0,innocent_hits-1)");
      }
      buffer_cat(insert, scratch);
    }
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, insert->data);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, insert->data);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), insert->data);
      LOGDEBUG ("_ds_setall_spamrecords: unable to run insert query: %s", insert->data);
      buffer_destroy(insert);
      return EFAILURE;
    }
  }

  buffer_destroy(insert);
#endif

  return 0;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  char *name;
  MYSQL_RES *result;
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;
  result = NULL;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
          "SELECT spam_hits,innocent_hits FROM dspam_token_data"
          " WHERE uid=%d AND token IN ('%llu')", (int) p->pw_uid, token);

  stat->probability = 0.00000;
  stat->spam_hits = 0;
  stat->innocent_hits = 0;
  stat->status &= ~TST_DISK;

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_ds_get_spamrecord: unable to run query: %s", query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbt->dbh_read);
  if (result == NULL) {
    LOGDEBUG("_ds_get_spamrecord: failed mysql_use_result()");
    return EFAILURE;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    result = NULL;
    return 0;
  }

  stat->spam_hits = strtoul (row[0], NULL, 0);
  if ((unsigned long int)stat->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->spam_hits", row[0]);
    mysql_free_result (result);
    result = NULL;
    return EFAILURE;
  }
  stat->innocent_hits = strtoul (row[1], NULL, 0);
  if ((unsigned long int)stat->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->innocent_hits", row[1]);
    mysql_free_result (result);
    result = NULL;
    return EFAILURE;
  }
  stat->status |= TST_DISK;
  mysql_free_result (result);
  result = NULL;
  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  char *name;
  int query_rc = 0;
  int query_errno = 0;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 40001
  /* It's either not on disk or the caller isn't using stat.status */
  if (!(stat->status & TST_DISK))
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_token_data (uid,token,spam_hits,innocent_hits,last_hit)"
              " VALUES (%d,'%llu',%lu,%lu,CURRENT_DATE())"
              " ON DUPLICATE KEY UPDATE"
              " spam_hits=%lu,"
              "innocent_hits=%lu,"
              "last_hit=CURRENT_DATE()",
              (int) p->pw_uid,
              token,
              stat->spam_hits, stat->innocent_hits,
              stat->spam_hits, stat->innocent_hits);
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
      }
    }
  }
#else
  /* It's either not on disk or the caller isn't using stat.status */
  if (!(stat->status & TST_DISK))
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_token_data (uid,token,spam_hits,innocent_hits,last_hit)"
              " VALUES (%d,'%llu',%lu,%lu,CURRENT_DATE())",
              (int) p->pw_uid, token, stat->spam_hits, stat->innocent_hits);
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
      }
    }
  } else {
    query_rc = -1;
  }

  if ((stat->status & TST_DISK) || query_rc)
  {
    /* insert failed; try updating instead */
    snprintf (query, sizeof (query), "UPDATE dspam_token_data"
              " SET spam_hits=%lu,"
              "innocent_hits=%lu,"
              "last_hit=CURRENT_DATE(),"
              " WHERE uid=%d"
              " AND token='%llu'", stat->spam_hits,
              stat->innocent_hits, (int) p->pw_uid, token);
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
      }
    }
  }
#endif

  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_set_spamrecord: unable to run query: %s", query);
    return EFAILURE;
  }

  return 0;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _mysql_drv_storage *s;
  _mysql_drv_dbh_t dbt = (_mysql_drv_dbh_t) dbh;

  if (CTX == NULL) {
    return EINVAL;
  }

  /* don't init if we're already initted */
  if (CTX->storage != NULL)
  {
    LOGDEBUG ("_ds_init_storage: storage already initialized");
    return EINVAL;
  }

  if (dbt) {
    if (dbt->dbh_read)
      if (mysql_ping(dbt->dbh_read)) {
        LOGDEBUG ("_ds_init_storage: MySQL server not responding to mysql_ping()");
        return EFAILURE;
      }
  }

  s = calloc (1, sizeof (struct _mysql_drv_storage));
  if (s == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s->dbh_attached = (dbt) ? 1 : 0;
  s->u_getnextuser[0] = 0;
  memset(&s->p_getpwnam, 0, sizeof(struct passwd));
  memset(&s->p_getpwuid, 0, sizeof(struct passwd));

  if (dbt)
    s->dbt = dbt;
  else
    s->dbt = _ds_connect(CTX);

  if (s->dbt == NULL)
  {
    LOG (LOG_WARNING, "Unable to initialize handle to MySQL database");
    free(s);
    return EFAILURE;
  }

  CTX->storage = s;

  s->control_token = 0;
  s->control_ih = 0;
  s->control_sh = 0;

  /* get spam totals on successful init */
  if (CTX->username != NULL)
  {
    if (_mysql_drv_get_spamtotals (CTX))
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;

  if (CTX->storage == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: called with NULL storage handle");
    return EINVAL;
  }

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: invalid database handle (NULL)");
    return EINVAL;
  }

  /* Store spam totals on shutdown */
  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
      _mysql_drv_set_spamtotals (CTX);
  }

  if (s->iter_user != NULL) {
    mysql_free_result (s->iter_user);
    s->iter_user = NULL;
  }

  if (s->iter_token != NULL) {
    mysql_free_result (s->iter_token);
    s->iter_token = NULL;
  }

  if (s->iter_sig != NULL) {
    mysql_free_result (s->iter_sig);
    s->iter_sig = NULL;
  }

  if (! s->dbh_attached) {
    mysql_close (s->dbt->dbh_read);
    if (s->dbt->dbh_write != s->dbt->dbh_read)
      mysql_close (s->dbt->dbh_write);
    if (s->dbt)
      free(s->dbt);
  }
  s->dbt = NULL;

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
  /* TODO
  if (_mysql_drv_get_UIDInSignature(CTX))
  */
  if (_ds_match_attribute(CTX->config->attributes, "MySQLUIDInSignature", "on"))
  {
    if (!CTX->group || CTX->flags & DSF_MERGED) {
      p = _mysql_drv_getpwnam (CTX, CTX->username);
      name = CTX->username;
    } else {
      p = _mysql_drv_getpwnam (CTX, CTX->group);
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  unsigned long *lengths;
  char *mem;
  char query[256];
  MYSQL_RES *result;
  MYSQL_ROW row;
  int uid = -1;
  MYSQL *dbh;
  result = NULL;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_get_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  dbh = _mysql_drv_sig_write_handle(CTX, s);

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }
  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_signature: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  /* TODO
  if (_mysql_drv_get_UIDInSignature(CTX))
  */
  if (_ds_match_attribute(CTX->config->attributes, "MySQLUIDInSignature", "on"))
  {
    char *u, *sig, *username;
    void *dbt = s->dbt;
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

    p = _mysql_drv_getpwuid (CTX, uid);
    if (p == NULL) {
      LOG(LOG_CRIT, "_ds_get_signature: _mysql_drv_getpwuid(%d) failed: aborting", uid);
      return EFAILURE;
    }

    username = strdup(p->pw_name);
    _ds_shutdown_storage(CTX);
    free(CTX->username);
    CTX->username = username;
    _ds_init_storage(CTX, (dbh_attached) ? dbt : NULL);
    s = (struct _mysql_drv_storage *) CTX->storage;

    dbh = _mysql_drv_sig_write_handle(CTX, s);
  }

  if (uid == -1) {
    uid = (int) p->pw_uid;
  }

  snprintf (query, sizeof (query),
            "SELECT data,length FROM dspam_signature_data WHERE uid=%d AND signature=\"%s\"",
            (int) uid, signature);

  if (mysql_real_query (dbh, query, strlen (query)))
  {
    _mysql_drv_query_error (mysql_error (dbh), query);
    LOGDEBUG ("_ds_get_signature: unable to run query: %s", query);
    return EFAILURE;
  }

  result = mysql_use_result (dbh);
  if (result == NULL) {
    LOGDEBUG("_ds_get_signature: failed mysql_use_result()");
    return EFAILURE;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    LOGDEBUG("_ds_get_signature: mysql_fetch_row() failed");
    mysql_free_result (result);
    result = NULL;
    return EFAILURE;
  }

  lengths = mysql_fetch_lengths (result);
  if (lengths == NULL || lengths[0] == 0)
  {
    LOGDEBUG("_ds_get_signature: mysql_fetch_lengths() failed");
    mysql_free_result (result);
    result = NULL;
    return EFAILURE;
  }

  mem = malloc (lengths[0]);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    mysql_free_result (result);
    result = NULL;
    return EUNKNOWN;
  }

  memcpy (mem, row[0], lengths[0]);
  if (SIG->data)
    free(SIG->data);
  SIG->data = mem;
  SIG->length = strtoul (row[1], NULL, 0);
  if (SIG->length == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_signature: failed converting %s to signature data length", row[1]);
    SIG->length = lengths[0];
  }
  mysql_free_result (result);
  result = NULL;

  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  unsigned long length;
  char *mem;
  char scratch[1024];
  buffer *query;
  struct passwd *p;
  char *name;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_set_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_signature: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  mem = calloc(1, (SIG->length*2)+1);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    buffer_destroy(query);
    return EUNKNOWN;
  }

  length = mysql_real_escape_string (s->dbt->dbh_write, mem, SIG->data, SIG->length);

  if(length+1024 > _mysql_driver_get_max_packet(s->dbt->dbh_write)) {
    LOG (LOG_WARNING, "_ds_set_signature: signature data to big to be inserted");
    LOG (LOG_WARNING, "_ds_set_signature: consider increasing max_allowed_packet to at least %llu",
              length+1025);
    return EINVAL;
  }

  snprintf (scratch, sizeof (scratch),
            "INSERT INTO dspam_signature_data (uid,signature,length,created_on,data) VALUES (%d,\"%s\",%lu,CURRENT_DATE(),\"",
            (int) p->pw_uid, signature, (unsigned long) SIG->length);
  buffer_cat (query, scratch);
  buffer_cat (query, mem);
  buffer_cat (query, "\")");
  free(mem);
  mem = NULL;

  if (mysql_real_query (s->dbt->dbh_write, query->data, query->used))
  {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query->data);
    LOGDEBUG ("_ds_set_signature: unable to run query: %s", query->data);
    buffer_destroy(query);
    return EFAILURE;
  }

  buffer_destroy(query);
  return 0;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  int query_rc = 0;
  int query_errno = 0;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM dspam_signature_data WHERE uid=%d AND signature=\"%s\"",
            (int) p->pw_uid, signature);
  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_delete_signature: unable to run query: %s", query);
    return EFAILURE;
  }

  return 0;
}

int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  MYSQL_RES *result;
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;
  result = NULL;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT signature FROM dspam_signature_data WHERE uid=%d AND signature=\"%s\"",
            (int) p->pw_uid, signature);
  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_ds_verify_signature: unable to run query: %s", query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbt->dbh_read);
  if (result == NULL) {
    return -1;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    result = NULL;
    return -1;
  }

  mysql_free_result (result);
  result = NULL;
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
#ifndef VIRTUAL_USERS
  struct passwd *p;
#else
  char *virtual_table, *virtual_username;
#endif
  uid_t uid;
  char query[512];
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;

#ifdef VIRTUAL_USERS
  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "MySQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }
#endif

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_get_nextuser: invalid database handle (NULL)");
    return NULL;
  }

  if (s->iter_user == NULL)
  {
#ifdef VIRTUAL_USERS
    /* TODO
    char *virtual_table, *virtual_username;
    virtual_table = _mysql_drv_get_virtual_table(CTX);
    virtual_username = _mysql_drv_get_virtual_username_field(CTX);
    */
    snprintf(query, sizeof(query), "SELECT DISTINCT %s FROM %s",
      virtual_username,
      virtual_table);
#else
    strncpy (query, "SELECT DISTINCT uid FROM dspam_stats", sizeof(query));
#endif
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_read);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
      LOGDEBUG ("_ds_get_nextuser: unable to run query: %s", query);
      return NULL;
    }

    s->iter_user = mysql_use_result (s->dbt->dbh_read);
    if (s->iter_user == NULL)
      return NULL;
  }

  row = mysql_fetch_row (s->iter_user);
  if (row == NULL)
  {
    mysql_free_result (s->iter_user);
    s->iter_user = NULL;
    return NULL;
  }

  if (row[0]) {
    uid = (uid_t) atoi (row[0]);
    if (uid == INT_MAX && errno == ERANGE) {
      LOGDEBUG("_ds_get_nextuser: failed converting %s to uid", row[0]);
      return NULL;
    }
  } else {
    LOG (LOG_CRIT, "_ds_get_nextuser: detected empty or NULL uid in stats table");
    return NULL;
  }
#ifdef VIRTUAL_USERS
  strlcpy (s->u_getnextuser, row[0], sizeof (s->u_getnextuser));
#else
  p = _mysql_drv_getpwuid (CTX, uid);
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_storage_record *st;
  char query[256];
  MYSQL_ROW row;
  int query_rc = 0;
  int query_errno = 0;
  struct passwd *p;
  char *name;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: unable to _mysql_drv_getpwnam(%s)",
              name);
    return NULL;
  }

  st = calloc (1,sizeof(struct _ds_storage_record));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->iter_token == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT token,spam_hits,innocent_hits,unix_timestamp(last_hit) FROM dspam_token_data WHERE uid=%d",
              (int) p->pw_uid);
    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_read);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
      LOGDEBUG ("_ds_get_nexttoken: unable to run query: %s", query);
      goto FAIL;
    }

    s->iter_token = mysql_use_result (s->dbt->dbh_read);
    if (s->iter_token == NULL)
      goto FAIL;
  }

  row = mysql_fetch_row (s->iter_token);
  if (row == NULL) {
    mysql_free_result (s->iter_token);
    s->iter_token = NULL;
    goto FAIL;
  }

  st->token = strtoull (row[0], NULL, 0);
  if (st->token == ULLONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->token", row[0]);
    goto FAIL;
  }
  st->spam_hits = strtoul (row[1], NULL, 0);
  if ((unsigned long int)st->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->spam_hits", row[1]);
    goto FAIL;
  }
  st->innocent_hits = strtoul (row[2], NULL, 0);
  if ((unsigned long int)st->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->innocent_hits", row[2]);
    goto FAIL;
  }
  st->last_hit = (time_t) strtol (row[3], NULL, 0);
  if (st->last_hit == LONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->last_hit", row[3]);
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_storage_signature *st;
  unsigned long *lengths;
  char query[256];
  MYSQL_ROW row;
  struct passwd *p;
  char *name;
  char *mem;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_get_nextsignature: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nextsignature: unable to _mysql_drv_getpwnam(%s)",
              name);
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_signature));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->iter_sig == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT data,signature,length,unix_timestamp(created_on) FROM dspam_signature_data WHERE uid=%d",
              (int) p->pw_uid);
    if (mysql_real_query (s->dbt->dbh_read, query, strlen (query)))
    {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
      LOGDEBUG ("_ds_get_nextsignature: unable to run query: %s", query);
      goto FAIL;
    }

    s->iter_sig = mysql_use_result (s->dbt->dbh_read);
    if (s->iter_sig == NULL)
      goto FAIL;
  }

  row = mysql_fetch_row (s->iter_sig);
  if (row == NULL) {
    mysql_free_result (s->iter_sig);
    s->iter_sig = NULL;
    goto FAIL;
  }

  lengths = mysql_fetch_lengths (s->iter_sig);
  if (lengths == NULL || lengths[0] == 0)
    goto FAIL;

  mem = malloc (lengths[0]);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    goto FAIL;
  }

  memcpy (mem, row[0], lengths[0]);
  st->data = mem;
  strlcpy (st->signature, row[1], sizeof (st->signature));
  st->length = strtoul (row[2], NULL, 0);
  if ((unsigned long int)st->length == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nextsignature: failed converting %s to st->length", row[2]);
    free(st->data);
    goto FAIL;
  }
  st->created_on = (time_t) strtol (row[3], NULL, 0);
  if (st->created_on == LONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nextsignature: failed converting %s to st->created_on", row[3]);
    free(st->data);
    goto FAIL;
  }

  return st;

FAIL:
  free(st);
  return NULL;
}

struct passwd *
_mysql_drv_getpwnam (DSPAM_CTX * CTX, const char *name)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  int query_rc = 0;
  int query_errno = 0;
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
  MYSQL_RES *result;
  MYSQL_ROW row;
  char *virtual_table, *virtual_uid, *virtual_username;
  char *sql_username;
  int name_size = MAX_USERNAME_LENGTH;
  result = NULL;

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "MySQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

  /* TODO
  virtual_table = _mysql_drv_get_virtual_table(CTX);
  virtual_uid = _mysql_drv_get_virtual_uid_field(CTX);
  virtual_username = _mysql_drv_get_virtual_username_field(CTX);
  */

  if (s->p_getpwnam.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (s->p_getpwnam.pw_name, name)) {
      LOGDEBUG("_mysql_drv_getpwnam returning cached name %s.", name);
      return &s->p_getpwnam;
    }

    free (s->p_getpwnam.pw_name);
    s->p_getpwnam.pw_name = NULL;
  }

  if (name != NULL) {
    name_size = strlen(name);
  }

  sql_username = malloc ((2 * name_size) + 1);
  if (sql_username == NULL)
  {
    LOGDEBUG("_mysql_drv_getpwnam returning NULL for name:  %s.  malloc() failed somehow.", name);
    return NULL;
  }

  mysql_real_escape_string (s->dbt->dbh_read, sql_username, name,
                            strlen(name));

  snprintf (query, sizeof (query),
            "SELECT %s FROM %s WHERE %s='%s'",
            virtual_uid, virtual_table, virtual_username, sql_username);

  free (sql_username);
  sql_username = NULL;

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_mysql_drv_getpwnam: unable to run query: %s", query);
    return NULL;
  }

  result = mysql_use_result (s->dbt->dbh_read);
  if (result == NULL) {
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS) {
      LOGDEBUG("_mysql_drv_getpwnam: returning NULL for query on name: %s that returned a null result", name);
      return NULL;
    }
    LOGDEBUG("_mysql_drv_getpwnam: setting, then returning passed name: %s after null MySQL result", name);
    return _mysql_drv_setpwnam (CTX, name);
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    result = NULL;
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS) {
      LOGDEBUG("_mysql_drv_getpwnam: returning NULL for query on name: %s", name);
      return NULL;
    }
    LOGDEBUG("_mysql_drv_getpwnam: setting, then returning passed name: %s", name);
    return _mysql_drv_setpwnam (CTX, name);
  }

  s->p_getpwnam.pw_uid = atoi(row[0]);
  if (s->p_getpwnam.pw_uid == INT_MAX && errno == ERANGE) {
    LOGDEBUG("_mysql_drv_getpwnam: failed converting %s to s->p_getpwnam.pw_uid", row[0]);
    mysql_free_result (result);
    result = NULL;
    return NULL;
  }
  if (name == NULL)
    s->p_getpwnam.pw_name = strdup("");
  else
    s->p_getpwnam.pw_name = strdup (name);

  mysql_free_result (result);
  result = NULL;
  LOGDEBUG("_mysql_drv_getpwnam: successful returning struct for name: %s", s->p_getpwnam.pw_name);
  return &s->p_getpwnam;

#endif
}

struct passwd *
_mysql_drv_getpwuid (DSPAM_CTX * CTX, uid_t uid)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  int query_rc = 0;
  int query_errno = 0;
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
  MYSQL_RES *result;
  MYSQL_ROW row;
  char *virtual_table, *virtual_uid, *virtual_username;
  result = NULL;

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "MySQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

  /* TODO
  virtual_table = _mysql_drv_get_virtual_table(CTX);
  virtual_uid = _mysql_drv_get_virtual_uid_field(CTX);
  virtual_username = _mysql_drv_get_virtual_username_field(CTX);
  */

  if (s->p_getpwuid.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (s->p_getpwuid.pw_uid == uid)
      return &s->p_getpwuid;
    free (s->p_getpwuid.pw_name);
    s->p_getpwuid.pw_name = NULL;
  }

  snprintf (query, sizeof (query),
            "SELECT %s FROM %s WHERE %s='%d'",
            virtual_username, virtual_table, virtual_uid, (int) uid);

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_mysql_drv_getpwuid: unable to run query: %s", query);
    return NULL;
  }

  result = mysql_use_result (s->dbt->dbh_read);
  if (result == NULL)
    return NULL;

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    result = NULL;
    return NULL;
  }

  if (row[0] == NULL)
  {
    mysql_free_result (result);
    result = NULL;
    return NULL;
  }

  s->p_getpwuid.pw_name = strdup (row[0]);
  s->p_getpwuid.pw_uid = (int) uid;

  mysql_free_result (result);
  result = NULL;
  return &s->p_getpwuid;
#endif
}

void
_mysql_drv_query_error (const char *error, const char *query)
{
  FILE *file;
  char fn[MAX_FILENAME_LENGTH];
  char buf[128];

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
_mysql_drv_setpwnam (DSPAM_CTX * CTX, const char *name)
{
  if (name == NULL)
    return NULL;

  char query[512];
  int query_rc = 0;
  int query_errno = 0;
  char *virtual_table, *virtual_uid, *virtual_username;
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char *sql_username;

  if ((virtual_table
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualTable"))==NULL)
  { virtual_table = "dspam_virtual_uids"; }

  if ((virtual_uid
    = _ds_read_attribute(CTX->config->attributes, "MySQLVirtualUIDField"))==NULL)
  { virtual_uid = "uid"; }

  if ((virtual_username = _ds_read_attribute(CTX->config->attributes,
    "MySQLVirtualUsernameField")) ==NULL)
  { virtual_username = "username"; }

  /* TODO
  virtual_table = _mysql_drv_get_virtual_table(CTX);
  virtual_uid = _mysql_drv_get_virtual_uid_field(CTX);
  virtual_username = _mysql_drv_get_virtual_username_field(CTX);
  */

#ifdef EXT_LOOKUP
  LOGDEBUG("_mysql_drv_setpwnam: verified_user is %d", verified_user);
  if (verified_user == 0) {
    LOGDEBUG("_mysql_drv_setpwnam: External lookup verification of %s failed: not adding user", name);
    return NULL;
  }
#endif

  sql_username = malloc (strlen(name) * 2 + 1);
  if (sql_username == NULL)
  {
    return NULL;
  }

  mysql_real_escape_string (s->dbt->dbh_write, sql_username, name,
                            strlen(name));

  snprintf (query, sizeof (query),
            "INSERT INTO %s (%s,%s) VALUES (NULL,'%s')",
            virtual_table, virtual_uid, virtual_username, sql_username);

  free (sql_username);
  sql_username = NULL;

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  /* we need to fail, to prevent a potential loop - even if it was inserted
   * by another process */
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_mysql_drv_setpwnam: unable to run query: %s", query);
    return NULL;
  }

  return _mysql_drv_getpwnam (CTX, name);

}
#endif

int
_ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char *name;
  char query[256];
  int query_rc = 0;
  int query_errno = 0;

  if (s->dbt == NULL)
  {
    LOGDEBUG ("_ds_del_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_del_spamrecord: unable to _mysql_drv_getpwnam(%s)",
              name);
    return EINVAL;
  }
  snprintf (query, sizeof (query),
          "DELETE FROM dspam_token_data WHERE uid=%d AND token=\"%llu\"",
          (int) p->pw_uid, token);

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_del_spamrecord: unable to run query: %s", query);
    return EFAILURE;
  }

  return 0;
}

int _ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  buffer *query;
  char scratch[1024];
  char queryhead[1024];
  int query_rc = 0;
  int query_errno = 0;
  struct passwd *p;
  char *name;

  if (diction->items < 1)
    return 0;

  if (s->dbt->dbh_write == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->username);
    name = CTX->username;
  } else {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    name = CTX->group;
  }

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
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

  /* Delete the spam records but split the query when the query size + 1024
   * reaches max_allowed_packet
   */
  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while (ds_term) {
    scratch[0] = 0;
    buffer_copy(query, queryhead);
    while (ds_term) {
      snprintf (scratch, sizeof (scratch), "'%llu'", ds_term->key);
      buffer_cat (query, scratch);
      ds_term = ds_diction_next(ds_c);
      if((unsigned long)(query->used + 1024) > _mysql_driver_get_max_packet(s->dbt->dbh_write) || !ds_term) {
        LOGDEBUG("_ds_delall_spamrecords: Splitting query at %lu characters", query->used);
        break;
      }
      buffer_cat (query, ",");
    }
    buffer_cat (query, ")");

    query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
    if (query_rc) {
      query_errno = mysql_errno (s->dbt->dbh_write);
      if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
        /* Locking issue. Wait 1 second and then retry the transaction again */
        sleep(1);
        query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query->data);
      }
    }
    if (query_rc) {
      _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query->data);
      LOGDEBUG ("_ds_delall_spamrecords: unable to run query: %s", query->data);
      buffer_destroy(query);
      query = NULL;
      ds_diction_close(ds_c);
      return EFAILURE;
    }
  }

  ds_diction_close(ds_c);
  buffer_destroy (query);
  query = NULL;
  return 0;
}

#ifdef PREFERENCES_EXTENSION
DSPAM_CTX *_mysql_drv_init_tools(
 const char *home,
 config_t config,
 void *dbt,
 int mode)
{
  DSPAM_CTX *CTX;
  struct _mysql_drv_storage *s;
  int dbh_attached = (dbt) ? 1 : 0;

  CTX = dspam_create (NULL, NULL, home, mode, 0);

  if (CTX == NULL)
    return NULL;

  _mysql_drv_set_attributes(CTX, config);

  if (!dbt)
    dbt = _ds_connect(CTX);

  if (!dbt)
    goto BAIL;

  if (dspam_attach(CTX, dbt))
    goto BAIL;

  s = (struct _mysql_drv_storage *) CTX->storage;
  s->dbh_attached = dbh_attached;

  return CTX;

BAIL:
  LOGDEBUG ("_mysql_drv_init_tools: Bailing and returning NULL!");
  dspam_destroy(CTX);
  return NULL;
}

agent_pref_t _ds_pref_load(
  config_t config,
  const char *username,
  const char *home,
  void *dbt)
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[512];
  int query_rc = 0;
  int query_errno = 0;
  MYSQL_RES *result;
  MYSQL_ROW row;
  DSPAM_CTX *CTX;
  agent_pref_t PTX;
  agent_attrib_t pref;
  int uid, i = 0;
  result = NULL;

  CTX = _mysql_drv_init_tools(home, config, dbt, DSM_TOOLS);
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "_ds_pref_load: unable to initialize tools context");
    return NULL;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_load: unable to _mysql_drv_getpwnam(%s)",
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

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_read);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_read, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_read), query);
    LOGDEBUG ("_ds_pref_load: unable to run query: %s", query);
    dspam_destroy(CTX);
    return NULL;
  }

  result = mysql_store_result (s->dbt->dbh_read);
  if (result == NULL) {
    dspam_destroy(CTX);
    return NULL;
  }

  PTX = malloc(sizeof(agent_attrib_t )*(mysql_num_rows(result)+1));
  if (PTX == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    dspam_destroy(CTX);
    mysql_free_result(result);
    result = NULL;
    return NULL;
  }

  PTX[0] = NULL;

  row = mysql_fetch_row (result);
  if (row == NULL) {
    dspam_destroy(CTX);
    mysql_free_result(result);
    result = NULL;
    _ds_pref_free(PTX);
    free(PTX);
    return NULL;
  }

  while(row != NULL) {
    char *p = row[0];
    char *q = row[1];

    pref = malloc(sizeof(struct _ds_agent_attribute));
    if (pref == NULL) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      mysql_free_result(result);
      result = NULL;
      dspam_destroy(CTX);
      return PTX;
    }

    pref->attribute = strdup(p);
    pref->value = strdup(q);
    PTX[i] = pref;
    PTX[i+1] = NULL;
    i++;

    row = mysql_fetch_row (result);
  }

  mysql_free_result(result);
  result = NULL;
  dspam_destroy(CTX);
  return PTX;
}

int _ds_pref_set (
 config_t config,
 const char *username,
 const char *home,
 const char *preference,
 const char *value,
 void *dbt)
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[512];
  int query_rc = 0;
  int query_errno = 0;
  DSPAM_CTX *CTX;
  int uid;
  char *m1, *m2;

  CTX = _mysql_drv_init_tools(home, config, dbt, DSM_PROCESS);
  if (CTX == NULL) {
    LOG (LOG_WARNING, "_ds_pref_set: unable to initialize tools context");
    return EFAILURE;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_set: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
      dspam_destroy(CTX);
      return EFAILURE;
    } else {
      uid = (int) p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  m1 = calloc(1, strlen(preference)*2+1);
  m2 = calloc(1, strlen(value)*2+1);
  if (m1 == NULL || m2 == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    dspam_destroy(CTX);
    free(m1);
    m1 = NULL;
    free(m2);
    m2 = NULL;
    return EFAILURE;
  }

  mysql_real_escape_string (s->dbt->dbh_write, m1, preference, strlen(preference));
  mysql_real_escape_string (s->dbt->dbh_write, m2, value, strlen(value));

  snprintf(query, sizeof(query), "DELETE FROM dspam_preferences"
    " WHERE uid=%d AND preference='%s'", (int) uid, m1);

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_pref_set: unable to run query: %s", query);
    goto FAIL;
  }

  snprintf(query, sizeof(query), "INSERT INTO dspam_preferences"
    " (uid,preference,value) VALUES (%d,'%s','%s')", (int) uid, m1, m2);

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_pref_set: unable to run query: %s", query);
    goto FAIL;
  }

  dspam_destroy(CTX);
  free(m1);
  m1 = NULL;
  free(m2);
  m2 = NULL;
  return 0;

FAIL:
  LOGDEBUG("_ds_pref_set: failed");
  free(m1);
  m1 = NULL;
  free(m2);
  m2 = NULL;
  dspam_destroy(CTX);
  return EFAILURE;
}

int _ds_pref_del (
 config_t config,
 const char *username,
 const char *home,
 const char *preference,
 void *dbt)
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[512];
  int query_rc = 0;
  int query_errno = 0;
  DSPAM_CTX *CTX;
  int uid;
  char *m1;

  CTX = _mysql_drv_init_tools(home, config, dbt, DSM_TOOLS);
  if (CTX == NULL) {
    LOG (LOG_WARNING, "_ds_pref_del: unable to initialize tools context");
    return EFAILURE;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_del: unable to _mysql_drv_getpwnam(%s)",
              username);
      dspam_destroy(CTX);
      return EFAILURE;
    } else {
      uid = (int) p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  m1 = calloc(1, strlen(preference)*2+1);
  if (m1 == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    dspam_destroy(CTX);
    free(m1);
    m1 = NULL;
    return EFAILURE;
  }

  mysql_real_escape_string (s->dbt->dbh_write, m1, preference, strlen(preference));

  snprintf(query, sizeof(query), "DELETE FROM dspam_preferences"
    " WHERE uid=%d AND preference='%s'", (int) uid, m1);

  query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
  if (query_rc) {
    query_errno = mysql_errno (s->dbt->dbh_write);
    if (query_errno == ER_LOCK_DEADLOCK || query_errno == ER_LOCK_WAIT_TIMEOUT || query_errno == ER_LOCK_OR_ACTIVE_TRANSACTION) {
      /* Locking issue. Wait 1 second and then retry the transaction again */
      sleep(1);
      query_rc = MYSQL_RUN_QUERY (s->dbt->dbh_write, query);
    }
  }
  if (query_rc) {
    _mysql_drv_query_error (mysql_error (s->dbt->dbh_write), query);
    LOGDEBUG ("_ds_pref_del: unable to run query: %s", query);
    goto FAIL;
  }

  dspam_destroy(CTX);
  free(m1);
  m1 = NULL;
  return 0;

FAIL:
  LOGDEBUG("_ds_pref_del: failed");
  free(m1);
  m1 = NULL;
  dspam_destroy(CTX);
  return EFAILURE;
}

int _mysql_drv_set_attributes(DSPAM_CTX *CTX, config_t config) {
  int i, ret = 0;
  attribute_t t;
  char *profile;

  profile = _ds_read_attribute(config, "DefaultProfile");

  for(i=0;config[i];i++) {
    t = config[i];

    while(t) {

      if (!strncasecmp(t->key, "MySQL", 5))
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
            x = NULL;
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
  _mysql_drv_dbh_t dbt = calloc(1, sizeof(struct _mysql_drv_dbh));
  dbt->dbh_read = _mysql_drv_connect(CTX, "MySQL");
  if (!dbt->dbh_read) {
    free(dbt);
    return NULL;
  }
  if (_ds_read_attribute(CTX->config->attributes, "MySQLWriteServer"))
    dbt->dbh_write = _mysql_drv_connect(CTX, "MySQLWrite");
  else
    dbt->dbh_write = dbt->dbh_read;
  return (void *) dbt;
}

MYSQL *_mysql_drv_connect (DSPAM_CTX *CTX, const char *prefix)
{
  MYSQL *dbh;
  FILE *file;
  char filename[MAX_FILENAME_LENGTH];
  char buffer[128];
  char hostname[128] = { 0 };
  char user[64] = { 0 };
  char password[64] = { 0 };
  char db[64] = { 0 };
  int port = 3306, i = 0, real_connect_flag = 0;
  char *p;
  char attrib[128];

  if (!prefix)
    prefix = "MySQL";

  /* Read storage attributes */
  snprintf(attrib, sizeof(attrib), "%sServer", prefix);
  if ((p = _ds_read_attribute(CTX->config->attributes, attrib))) {

    strlcpy(hostname, p, sizeof(hostname));
    if (strlen(p) >= sizeof(hostname))
    {
      LOG(LOG_WARNING, "Truncating MySQLServer to %d characters.",
          sizeof(hostname)-1);
    }

    snprintf(attrib, sizeof(attrib), "%sPort", prefix);
    if (_ds_read_attribute(CTX->config->attributes, attrib)) {
      port = atoi(_ds_read_attribute(CTX->config->attributes, attrib));
      if (port == INT_MAX && errno == ERANGE) {
        LOGDEBUG("_mysql_drv_connect: failed converting %s to port", _ds_read_attribute(CTX->config->attributes, attrib));
        goto FAILURE;
      }
    }
    else
      port = 0;

    snprintf(attrib, sizeof(attrib), "%sUser", prefix);
    if ((p = _ds_read_attribute(CTX->config->attributes, attrib)))
    {
      strlcpy(user, p, sizeof(user));
      if (strlen(p) >= sizeof(user))
      {
        LOG(LOG_WARNING, "Truncating MySQLUser to %d characters.",
            sizeof(user)-1);
      }
    }
    snprintf(attrib, sizeof(attrib), "%sPass", prefix);
    if ((p = _ds_read_attribute(CTX->config->attributes, attrib)))
    {
      strlcpy(password, p, sizeof(password));
      if (strlen(p) >= sizeof(password))
      {
        LOG(LOG_WARNING, "Truncating MySQLPass to %d characters.",
            sizeof(password)-1);
      }
    }
    snprintf(attrib, sizeof(attrib), "%sDb", prefix);
    if ((p = _ds_read_attribute(CTX->config->attributes, attrib)))
    {
      strlcpy(db, p, sizeof(db));
      if (strlen(p) >= sizeof(db))
      {
        LOG(LOG_WARNING, "Truncating MySQLDb to %d characters.",
            sizeof(db)-1);
      }
    }

    snprintf(attrib, sizeof(attrib), "%sCompress", prefix);
    if (_ds_match_attribute(CTX->config->attributes, attrib, "true"))
      real_connect_flag = CLIENT_COMPRESS;

  } else {
    if (!CTX->home) {
      LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
      goto FAILURE;
    }
    snprintf (filename, MAX_FILENAME_LENGTH, "%s/mysql.data", CTX->home);
    file = fopen (filename, "r");
    if (file == NULL)
    {
      LOG (LOG_WARNING, "_mysql_drv_connect: unable to locate mysql configuration");
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
          LOGDEBUG("_mysql_drv_connect: failed converting %s to port", buffer);
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
    LOG (LOG_WARNING, "file %s: incomplete mysql connect data", filename);
    goto FAILURE;
  }

  dbh = mysql_init(NULL);
  if (dbh == NULL)
  {
    LOGDEBUG
      ("_mysql_drv_connect: mysql_init: unable to initialize handle to database");
    goto FAILURE;
  }

#if defined(MYSQL_VERSION_ID) && MYSQL_VERSION_ID >= 50013
  /* enable automatic reconnect for MySQL >= 5.0.13 */
  snprintf(attrib, sizeof(attrib), "%sReconnect", prefix);
  if (_ds_match_attribute(CTX->config->attributes, attrib, "true"))
  {
      my_bool reconnect = 1;
      mysql_options(dbh, MYSQL_OPT_RECONNECT, &reconnect);
  }
#endif

  if (hostname[0] == '/')
  {
    if (!mysql_real_connect (dbh, NULL, user, password, db, 0, hostname,
                        real_connect_flag))
    {
      LOG (LOG_WARNING, "%s", mysql_error (dbh));
      mysql_close(dbh);
      goto FAILURE;
    }
  }
  else
  {
    if (!mysql_real_connect (dbh, hostname, user, password, db, port, NULL,
                        real_connect_flag))
    {
      LOG (LOG_WARNING, "%s", mysql_error (dbh));
      mysql_close(dbh);
      goto FAILURE;
    }
  }

  return dbh;

FAILURE:
  LOGDEBUG("_mysql_drv_connect: failed");
  return NULL;
}

MYSQL * _mysql_drv_sig_write_handle(
    DSPAM_CTX *CTX,
    struct _mysql_drv_storage *s)
{
  if (_ds_match_attribute(CTX->config->attributes, "MySQLReadSignaturesFromWriteDb", "on"))
    return s->dbt->dbh_write;
  else
    return s->dbt->dbh_read;
}
