/* $Id: sqlite3_drv.c,v 1.187 2011/06/28 00:13:48 sbajic Exp $ */

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
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#   include <pwd.h>
#   include <dirent.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

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
#include "sqlite3_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"
#include "config_shared.h"

#ifdef _WIN32
#   include <process.h>
#   include "dir_win32.h"
#endif

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
_sqlite_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[1024];
  char *err=NULL, **row;
  int nrow, ncolumn;
  int rc;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_sqlite_drv_get_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  memset(&s->control_totals, 0, sizeof(struct _ds_spam_totals));
  memset(&CTX->totals, 0, sizeof(struct _ds_spam_totals));

  snprintf (query, sizeof (query),
            "SELECT spam_learned,innocent_learned,"
            "spam_misclassified,innocent_misclassified,"
            "spam_corpusfed,innocent_corpusfed,"
            "spam_classified,innocent_classified"
            " FROM dspam_stats");

  if ((sqlite3_get_table(s->dbh, query, &row, &nrow, &ncolumn, &err))!=SQLITE_OK)
  {
    _sqlite_drv_query_error (err, query);
    return EFAILURE;
  }

  if (nrow>0 && row != NULL) {
    CTX->totals.spam_learned		= strtoul (row[ncolumn], NULL, 0);
    if (CTX->totals.spam_learned == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.spam_learned", row[ncolumn]);
      rc = EFAILURE;
      goto FAIL;
    }
    CTX->totals.innocent_learned	= strtoul (row[ncolumn+1], NULL, 0);
    if (CTX->totals.innocent_learned == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.innocent_learned", row[ncolumn+1]);
      rc = EFAILURE;
      goto FAIL;
    }
    CTX->totals.spam_misclassified	= strtoul (row[ncolumn+2], NULL, 0);
    if (CTX->totals.spam_misclassified == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.spam_misclassified", row[ncolumn+2]);
      rc = EFAILURE;
      goto FAIL;
    }
    CTX->totals.innocent_misclassified 	= strtoul (row[ncolumn+3], NULL, 0);
    if (CTX->totals.innocent_misclassified == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.innocent_misclassified", row[ncolumn+3]);
      rc = EFAILURE;
      goto FAIL;
    }
    CTX->totals.spam_corpusfed		= strtoul (row[ncolumn+4], NULL, 0);
    if (CTX->totals.spam_corpusfed == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.spam_corpusfed", row[ncolumn+4]);
      rc = EFAILURE;
      goto FAIL;
    }
    CTX->totals.innocent_corpusfed	= strtoul (row[ncolumn+5], NULL, 0);
    if (CTX->totals.innocent_corpusfed == ULONG_MAX && errno == ERANGE) {
      LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.innocent_corpusfed", row[ncolumn+5]);
      rc = EFAILURE;
      goto FAIL;
    }
    if (row[ncolumn+6] != NULL && row[ncolumn+7] != NULL) {
      CTX->totals.spam_classified	= strtoul (row[ncolumn+6], NULL, 0);
      if (CTX->totals.spam_classified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.spam_classified", row[ncolumn+6]);
        rc = EFAILURE;
        goto FAIL;
      }
      CTX->totals.innocent_classified	= strtoul (row[ncolumn+7], NULL, 0);
      if (CTX->totals.innocent_classified == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_sqlite_drv_get_spamtotals: failed converting %s to CTX->totals.innocent_classified", row[ncolumn+7]);
        rc = EFAILURE;
        goto FAIL;
      }
    } else {
      CTX->totals.spam_classified	= 0;
      CTX->totals.innocent_classified	= 0;
    }
    rc = 0;
  } else {
    rc = EFAILURE;
  }

FAIL:
  sqlite3_free_table(row);
  if ( !rc )
    memcpy(&s->control_totals, &CTX->totals, sizeof(struct _ds_spam_totals));

  return rc;
}

int
_sqlite_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[1024];
  char *err=NULL;
  int result = SQLITE_OK;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_sqlite_drv_set_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _sqlite_drv_get_spamtotals (CTX);    /* undo changes to in memory totals */
    return 0;
  }

  /* dspam_stat_id insures only one stats record */

  if (s->control_totals.innocent_learned == 0)
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_stats (dspam_stat_id,spam_learned,"
              "innocent_learned,spam_misclassified,innocent_misclassified,"
              "spam_corpusfed,innocent_corpusfed,"
              "spam_classified,innocent_classified)"
              " VALUES (%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu)",
              0,
              CTX->totals.spam_learned,
              CTX->totals.innocent_learned, CTX->totals.spam_misclassified,
              CTX->totals.innocent_misclassified, CTX->totals.spam_corpusfed,
              CTX->totals.innocent_corpusfed, CTX->totals.spam_classified,
              CTX->totals.innocent_classified);
    result = sqlite3_exec(s->dbh, query, NULL, NULL, NULL);
  }

  if (s->control_totals.innocent_learned != 0 || result != SQLITE_OK)
  {
    snprintf (query, sizeof (query),
              "UPDATE dspam_stats SET spam_learned=spam_learned%s%d,"
              "innocent_learned=innocent_learned%s%d,"
              "spam_misclassified=spam_misclassified%s%d,"
              "innocent_misclassified=innocent_misclassified%s%d,"
              "spam_corpusfed=spam_corpusfed%s%d,"
              "innocent_corpusfed=innocent_corpusfed%s%d,"
              "spam_classified=spam_classified%s%d,"
              "innocent_classified=innocent_classified%s%d",
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
                  s->control_totals.innocent_classified));

    if ((sqlite3_exec(s->dbh, query, NULL, NULL, &err))!=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query);
      return EFAILURE;
    }
  }

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  buffer *query;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  char scratch[1024];
  char queryhead[1024];
  struct _ds_spam_stat stat;
  unsigned long long token = 0;
  char *err=NULL, **row=NULL;
  int nrow, ncolumn, i;

  if (diction->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  stat.spam_hits = 0;
  stat.innocent_hits = 0;
  stat.probability   = 0.00000;

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (queryhead, sizeof (queryhead),
            "SELECT token,spam_hits,innocent_hits"
            " FROM dspam_token_data WHERE token IN (");

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while (ds_term) {
    scratch[0] = 0;
    buffer_copy(query, queryhead);
    while (ds_term) {
      snprintf (scratch, sizeof (scratch), "'%" LLU_FMT_SPEC "'", ds_term->key);
      buffer_cat (query, scratch);
      ds_term->s.innocent_hits = 0;
      ds_term->s.spam_hits = 0;
      ds_term->s.probability = 0.00000;
      ds_term->s.status = 0;
      if((query->used + 1024) > 1000000) {
        LOGDEBUG("_ds_getall_spamrecords: Splitting query at %ld characters", query->used);
        break;
      }
      ds_term = ds_diction_next(ds_c);
      if (ds_term)
        buffer_cat (query, ",");
    }
    buffer_cat (query, ")");

#ifdef VERBOSE
    LOGDEBUG ("SQLite query length: %ld\n", query->used);
    _sqlite_drv_query_error (strdup("VERBOSE DEBUG (INFO ONLY - NOT AN ERROR)"), query->data);
#endif

    if ((sqlite3_get_table(s->dbh, query->data, &row, &nrow, &ncolumn, &err))
        !=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query->data);
      LOGDEBUG ("_ds_getall_spamrecords: unable to run query: %s", query->data);
      buffer_destroy(query);
      ds_diction_close(ds_c);
      return EFAILURE;
    }

    if (nrow < 1) {
      sqlite3_free_table(row);
      buffer_destroy(query);
      ds_diction_close(ds_c);
      return 0;
    }

    if (row == NULL) {
      buffer_destroy(query);
      ds_diction_close(ds_c);
      return 0;
    }

    for(i=1;i<=nrow;i++) {
      token = strtoull (row[(i*ncolumn)], NULL, 0);
      stat.spam_hits = strtoul (row[1+(i*ncolumn)], NULL, 0);
      if (stat.spam_hits == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.spam_hits", row[1+(i*ncolumn)]);
        sqlite3_free_table(row);
        return EFAILURE;
      }
      stat.innocent_hits = strtoul (row[2+(i*ncolumn)], NULL, 0);
      if (stat.innocent_hits == ULONG_MAX && errno == ERANGE) {
        LOGDEBUG("_ds_getall_spamrecords: failed converting %s to stat.innocent_hits", row[2+(i*ncolumn)]);
        sqlite3_free_table(row);
        return EFAILURE;
      }
      stat.status = 0;
      stat.status |= TST_DISK;
      if (stat.spam_hits < 0)
        stat.spam_hits = 0;
      if (stat.innocent_hits < 0)
        stat.innocent_hits = 0;
      ds_diction_addstat(diction, token, &stat);
    }
    if (row != NULL)
      sqlite3_free_table(row);
    row = NULL;
    ds_term = ds_diction_next(ds_c);
  }
  ds_diction_close(ds_c);
  buffer_destroy (query);
  if (row != NULL)
    sqlite3_free_table(row);
  row = NULL;

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
}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  struct _ds_spam_stat control, stat;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  char queryhead[1024];
  buffer *query;
  char scratch[1024];
  char *err=NULL;
  int update_any = 0;

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

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  ds_diction_getstat(diction, s->control_token, &control);

  snprintf (queryhead, sizeof (queryhead),
            "UPDATE dspam_token_data SET last_hit=date('now'),"
            "spam_hits=max(0,spam_hits%s%d),"
            "innocent_hits=max(0,innocent_hits%s%d)"
            " WHERE token IN (",
            (control.spam_hits > s->control_sh) ? "+" : "-",
            abs (control.spam_hits - s->control_sh),
            (control.innocent_hits > s->control_ih) ? "+" : "-",
            abs (control.innocent_hits - s->control_ih));

  buffer_copy (query, queryhead);


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
      snprintf(ins, sizeof (ins),
               "INSERT INTO dspam_token_data (token,spam_hits,"
               "innocent_hits,last_hit) VALUES ('%" LLU_FMT_SPEC "',%d,%d,"
               "date('now'))",
               ds_term->key,
               stat.spam_hits > 0 ? 1 : 0,
               stat.innocent_hits > 0 ? 1 : 0);
      if ((sqlite3_exec(s->dbh, ins, NULL, NULL, NULL)) != SQLITE_OK)
        stat.status |= TST_DISK;
    }

    if (stat.status & TST_DISK) {
      snprintf (scratch, sizeof (scratch), "'%" LLU_FMT_SPEC "'", ds_term->key);
      buffer_cat (query, scratch);
      update_any = 1;
      use_comma = 1;
    }

    ds_term->s.status |= TST_DISK;

    ds_term = ds_diction_next(ds_c);
    if((query->used + 1024) > 1000000) {
      LOGDEBUG("_ds_setall_spamrecords: Splitting update query at %ld characters", query->used);
      buffer_cat (query, ")");
      if (update_any) {
        if ((sqlite3_exec(s->dbh, query->data, NULL, NULL, &err)) != SQLITE_OK) {
          _sqlite_drv_query_error (err, query->data);
          LOGDEBUG ("_ds_setall_spamrecords: unable to run query: %s", query->data);
          ds_diction_close(ds_c);
          buffer_destroy(query);
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

  if (update_any)
  {
    if ((sqlite3_exec(s->dbh, query->data, NULL, NULL, &err))!=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query->data);
      LOGDEBUG ("_ds_setall_spamrecords: unable to run query: %s", query->data);
      buffer_destroy(query);
      return EFAILURE;
    }
  }

  buffer_destroy (query);
  return 0;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[1024];
  char *err=NULL, **row;
  int nrow, ncolumn;


  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT spam_hits,innocent_hits FROM dspam_token_data"
            " WHERE token='%" LLU_FMT_SPEC "'", token);

  stat->probability = 0.00000;
  stat->spam_hits = 0;
  stat->innocent_hits = 0;
  stat->status &= ~TST_DISK;

  if ((sqlite3_get_table(s->dbh, query, &row, &nrow, &ncolumn, &err))!=SQLITE_OK)
  {
    _sqlite_drv_query_error (err, query);
    LOGDEBUG ("_ds_get_spamrecord: unable to run query: %s", query);
    return EFAILURE;
  }

  if (nrow < 1)
    sqlite3_free_table(row);

  if (nrow < 1 || row == NULL)
    return 0;

  stat->spam_hits = strtoul (row[0], NULL, 0);
  if (stat->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->spam_hits", row[0]);
    sqlite3_free_table(row);
    return EFAILURE;
  }
  stat->innocent_hits = strtoul (row[1], NULL, 0);
  if (stat->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_spamrecord: failed converting %s to stat->innocent_hits", row[1]);
    sqlite3_free_table(row);
    return EFAILURE;
  }
  stat->status |= TST_DISK;
  sqlite3_free_table(row);
  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[1024];
  char *err=NULL;
  int result = 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
    return 0;

  /* It's either not on disk or the caller isn't using stat.disk */
  if (!(stat->status & TST_DISK))
  {
    snprintf (query, sizeof (query),
              "INSERT INTO dspam_token_data (token,spam_hits,innocent_hits,last_hit)"
              " VALUES ('%" LLU_FMT_SPEC "',%lu,%lu,date('now'))",
              token,
              stat->spam_hits > 0 ? stat->spam_hits : 0,
              stat->innocent_hits > 0 ? stat->innocent_hits : 0);
    result = sqlite3_exec(s->dbh, query, NULL, NULL, NULL);
  }

  if ((stat->status & TST_DISK) || result)
  {
    /* insert failed; try updating instead */
    snprintf (query, sizeof (query), "UPDATE dspam_token_data"
              " SET spam_hits=%lu,"
              "innocent_hits=%lu"
              " WHERE token='%" LLU_FMT_SPEC "'",
              stat->spam_hits > 0 ? stat->spam_hits : 0,
              stat->innocent_hits > 0 ? stat->innocent_hits : 0,
              token);

    if ((sqlite3_exec(s->dbh, query, NULL, NULL, &err))!=SQLITE_OK) {
      _sqlite_drv_query_error (err, query);
      LOGDEBUG ("_ds_set_spamrecord: unable to run query: %s", query);
      return EFAILURE;
    }
  }

  return 0;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _sqlite_drv_storage *s;
  FILE *file;
  char buff[1024];
  char filename[MAX_FILENAME_LENGTH];
  char *err=NULL;
  struct stat st;
  int noexist;

  buff[0] = 0;

  if (CTX == NULL)
    return EINVAL;

  if (CTX->flags & DSF_MERGED) {
    LOG(LOG_ERR, ERR_DRV_NO_MERGED);
    return EINVAL;
  }

  /* don't init if we're already initted */
  if (CTX->storage != NULL)
  {
    LOGDEBUG ("_ds_init_storage: storage already initialized");
    return EINVAL;
  }

  s = calloc (1, sizeof (struct _sqlite_drv_storage));
  if (s == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s->dbh = NULL;
  s->control_token = 0;
  s->iter_token = NULL;
  s->iter_sig = NULL;
  s->control_token = 0;
  s->control_sh = 0;
  s->control_ih = 0;
  s->dbh_attached = (dbh) ? 1 : 0;

  if (CTX->group == NULL || CTX->group[0] == 0)
    _ds_userdir_path (filename, CTX->home, CTX->username, "sdb");
  else
    _ds_userdir_path (filename, CTX->home, CTX->group, "sdb");

  _ds_prepare_path_for (filename);

  noexist = stat(filename, &st);

  if (dbh)
    s->dbh = dbh;
  else if ((sqlite3_open(filename, &s->dbh))!=SQLITE_OK)
    s->dbh = NULL;

  if (s->dbh == NULL)
  {
    free(s);
    LOGDEBUG
      ("_ds_init_storage: unable to initialize database: %s", filename);
    return EFAILURE;
  }

  /* Commit timeout of 20 minutes */
  sqlite3_busy_timeout(s->dbh, 1000 * 60 * 20);

  /* Create database objects */

  if (noexist) {

    LOGDEBUG ("_ds_init_storage: Creating object structure in database: %s", filename);

    buff[0] = 0;
    snprintf (buff, sizeof (buff),
                "CREATE TABLE dspam_token_data (token CHAR(20) PRIMARY KEY,"
                "spam_hits INT,innocent_hits INT,last_hit DATE)");
    if ((sqlite3_exec(s->dbh, buff, NULL, NULL, &err))!=SQLITE_OK) {
      _sqlite_drv_query_error (err, buff);
      free(s);
      return EFAILURE;
    }

    buff[0] = 0;
    snprintf (buff, sizeof (buff),
                "CREATE INDEX id_token_data_02 ON dspam_token_data"
                "(innocent_hits)");
    if ((sqlite3_exec(s->dbh, buff, NULL, NULL, &err))!=SQLITE_OK) {
      _sqlite_drv_query_error (err, buff);
      free(s);
      return EFAILURE;
    }

    buff[0] = 0;
    snprintf (buff, sizeof (buff),
                "CREATE TABLE dspam_signature_data ("
                "signature CHAR(128) PRIMARY KEY,data BLOB,created_on DATE)");
    if ((sqlite3_exec(s->dbh, buff, NULL, NULL, &err))!=SQLITE_OK) {
      _sqlite_drv_query_error (err, buff);
      free(s);
      return EFAILURE;
    }

    buff[0] = 0;
    snprintf (buff, sizeof (buff),
                "CREATE TABLE dspam_stats (dspam_stat_id INT PRIMARY KEY,"
                "spam_learned INT,innocent_learned INT,"
                "spam_misclassified INT,innocent_misclassified INT,"
                "spam_corpusfed INT,innocent_corpusfed INT,"
                "spam_classified INT,innocent_classified INT)");
    if ((sqlite3_exec(s->dbh, buff, NULL, NULL, &err))!=SQLITE_OK) {
      _sqlite_drv_query_error (err, buff);
      free(s);
      return EFAILURE;
    }

    buff[0] = 0;
  }

  if (_ds_read_attribute(CTX->config->attributes, "SQLitePragma")) {
    char pragma[1024];
    attribute_t t = _ds_find_attribute(CTX->config->attributes, "SQLitePragma");
    while(t != NULL) {
      snprintf(pragma, sizeof(pragma), "PRAGMA %s", t->value);
      if ((sqlite3_exec(s->dbh, pragma, NULL, NULL, &err))!=SQLITE_OK)
      {
        LOG(LOG_WARNING, "sqlite.pragma function error: %s: %s", err, pragma);
        _sqlite_drv_query_error (err, pragma);
      }
      t = t->next;
    }
  } else {
    snprintf(filename, MAX_FILENAME_LENGTH, "%s/sqlite.pragma", CTX->home);
    file = fopen(filename, "r");
    if (file != NULL) {
      while((fgets(buff, sizeof(buff), file))!=NULL) {
        chomp(buff);
        if ((sqlite3_exec(s->dbh, buff, NULL, NULL, &err))!=SQLITE_OK)
        {
          LOG(LOG_WARNING, "sqlite.pragma function error: %s: %s", err, buff);
          _sqlite_drv_query_error (err, buff);
        }
      }
      fclose(file);
    }
  }

  CTX->storage = s;

  s->dir_handles = nt_create (NT_INDEX);
  s->control_token = 0;
  s->control_sh = 0;
  s->control_ih = 0;

  /* get spam totals on successful init */
  if (CTX->username != NULL)
  {
      if (_sqlite_drv_get_spamtotals (CTX))
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
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  struct nt_node *node_nt;
  struct nt_c c_nt;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: invalid database handle (NULL)");
    return EINVAL;
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


  /* Store spam totals on shutdown */
  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
      _sqlite_drv_set_spamtotals (CTX);
  }

  if (!s->dbh_attached)
    sqlite3_close(s->dbh);

  s->dbh = NULL;

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
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[128];
  char *err=NULL;
  const char *query_tail;
  sqlite3_stmt *stmt;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "SELECT data FROM dspam_signature_data WHERE signature=\"%s\"",
            signature);

  if ((sqlite3_prepare(s->dbh, query, -1, &stmt, &query_tail))
        !=SQLITE_OK)
  {
    _sqlite_drv_query_error (err, query);
    return EFAILURE;
  }

  if ((sqlite3_step(stmt))!=SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return EFAILURE;
  }

  SIG->length = sqlite3_column_bytes(stmt, 0);
  SIG->data = malloc(SIG->length);
  if (SIG->data == NULL) {
    sqlite3_finalize(stmt);
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  memcpy(SIG->data, sqlite3_column_blob(stmt, 0), SIG->length);

  if ((sqlite3_finalize(stmt)!=SQLITE_OK))
    LOGDEBUG("_ds_get_signature: sqlite3_finalize() failed: %s", strerror(errno));

  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char scratch[1024];
  char *err=NULL;
  const char *query_tail=NULL;
  sqlite3_stmt *stmt;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (scratch, sizeof (scratch),
            "INSERT INTO dspam_signature_data (signature,created_on,data)"
            " VALUES (\"%s\",date('now'),?)", signature);

  if ((sqlite3_prepare(s->dbh, scratch, -1, &stmt, &query_tail))!=SQLITE_OK)
  {
    _sqlite_drv_query_error ("_ds_set_signature: sqlite3_prepare() failed", scratch);
    return EFAILURE;
  }

  sqlite3_bind_blob(stmt, 1, SIG->data, SIG->length, SQLITE_STATIC);

  if ((sqlite3_step(stmt))!=SQLITE_DONE) {
    _sqlite_drv_query_error (err, scratch);
    return EFAILURE;
  }

  sqlite3_finalize(stmt);

  return 0;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[128];
  char *err=NULL;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM dspam_signature_data WHERE signature=\"%s\"",
             signature);

  if ((sqlite3_exec(s->dbh, query, NULL, NULL, &err))!=SQLITE_OK)
  {
    _sqlite_drv_query_error (err, query);
    return EFAILURE;
  }

  return 0;
}

int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[128];
  char *err=NULL, **row;
  int nrow, ncolumn;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (query, sizeof (query),
        "SELECT signature FROM dspam_signature_data WHERE signature=\"%s\"",
        signature);

  if ((sqlite3_get_table(s->dbh, query, &row, &nrow, &ncolumn, &err))!=SQLITE_OK)  {
    _sqlite_drv_query_error (err, query);
    return EFAILURE;
  }

  sqlite3_free_table(row);

  if (nrow<1) {
    return -1;
  }

  return 0;
}

char *
_ds_get_nextuser (DSPAM_CTX * CTX)
{
  static char user[MAX_FILENAME_LENGTH];
  static char path[MAX_FILENAME_LENGTH];
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
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
           "_ds_get_nextuser: unable to open directory '%s' for reading: %s",
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

  if (dir != NULL) {
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
               (entry->d_name + strlen (entry->d_name) - 4, ".sdb", 4))
      {
        strlcpy (user, entry->d_name, sizeof (user));
        user[strlen (user) - 4] = 0;
        return user;
      }
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
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  struct _ds_storage_record *st;
  char query[128];
  char *err=NULL;
  const char *query_tail=NULL;
  int x;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: invalid database handle (NULL)");
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_record));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->iter_token == NULL)
  {
    snprintf (query, sizeof (query),
              "SELECT token,spam_hits,innocent_hits,strftime('%%s',"
              "last_hit) FROM dspam_token_data");

    if ((sqlite3_prepare(s->dbh, query, -1, &s->iter_token, &query_tail))
        !=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query);
      free(st);
      return NULL;
    }
  }

  if ((x = sqlite3_step(s->iter_token)) !=SQLITE_ROW) {
    if (x != SQLITE_DONE) {
      _sqlite_drv_query_error (err, query);
      s->iter_token = NULL;
      free(st);
      return NULL;
    }
    sqlite3_finalize((struct sqlite3_stmt *) s->iter_token);
    s->iter_token = NULL;
    free(st);
    return NULL;
  }

  st->token = strtoull ((const char *) sqlite3_column_text(s->iter_token, 0), NULL, 0);
  st->spam_hits = strtoul ((const char *) sqlite3_column_text(s->iter_token, 1), NULL, 0);
  if (st->spam_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->spam_hits", (const char *) sqlite3_column_text(s->iter_token, 1));
    s->iter_token = NULL;
    free(st);
    return NULL;
  }
  st->innocent_hits = strtoul ((const char *) sqlite3_column_text(s->iter_token, 2), NULL, 0);
  if (st->innocent_hits == ULONG_MAX && errno == ERANGE) {
    LOGDEBUG("_ds_get_nexttoken: failed converting %s to st->innocent_hits", (const char *) sqlite3_column_text(s->iter_token, 2));
    s->iter_token = NULL;
    free(st);
    return NULL;
  }
  st->last_hit = (time_t) strtol ((const char *) sqlite3_column_text(s->iter_token, 3), NULL, 0);

  return st;
}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  struct _ds_storage_signature *st;
  unsigned long length;
  char query[128];
  char *mem;
  char *err=NULL;
  const char *query_tail=NULL;
  int x;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_nextsignature: invalid database handle (NULL)");
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
              "SELECT data,signature,strftime('%%s',created_on)"
              " FROM dspam_signature_data");

   if ((sqlite3_prepare(s->dbh, query, -1, &s->iter_sig, &query_tail))
        !=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query);
      free(st);
      return NULL;
    }
  }

  if ((x = sqlite3_step(s->iter_sig)) !=SQLITE_ROW) {
    if (x != SQLITE_DONE) {
      _sqlite_drv_query_error (err, query);
      s->iter_sig = NULL;
      free(st);
      return NULL;
    }
    sqlite3_finalize((struct sqlite3_stmt *) s->iter_sig);
    s->iter_sig = NULL;
    free(st);
    return NULL;
  }

  length = sqlite3_column_bytes(s->iter_sig, 0);
  mem = malloc (length);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    sqlite3_finalize(s->iter_sig);
    s->iter_sig = NULL;
    free(st);
    return NULL;
  }

  memcpy(mem, sqlite3_column_blob(s->iter_sig, 0), length);

  st->data = mem;
  strlcpy(st->signature, (const char *) sqlite3_column_text(s->iter_sig, 1), sizeof(st->signature));
  st->length = length;
  st->created_on = (time_t) strtol( (const char *) sqlite3_column_text(s->iter_sig, 2), NULL, 0);

  return st;
}

void
_sqlite_drv_query_error (const char *error, const char *query)
{
  FILE *file;
  time_t tm = time (NULL);
  char ct[128];
  char fn[MAX_FILENAME_LENGTH];

  LOG (LOG_WARNING, "query error: %s: see sql.errors for more details",
       error);

  snprintf (fn, sizeof (fn), "%s/sql.errors", LOGDIR);

  snprintf (ct, sizeof (ct), "%s", ctime (&tm));
  chomp (ct);

  file = fopen (fn, "a");

  if (file == NULL)
  {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, fn, strerror (errno));
  }
  else
  {
    fprintf (file, "[%s] %d: %s: %s\n", ct, (int) getpid (), error, query);
    fclose (file);
  }

  free((char *)error);
  return;
}

int
_ds_del_spamrecord (DSPAM_CTX * CTX, unsigned long long token)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  char query[128];
  char *err=NULL;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_del_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "DELETE FROM dspam_token_data WHERE token='%" LLU_FMT_SPEC "'",
            token);

  if ((sqlite3_exec(s->dbh, query, NULL, NULL, &err))!=SQLITE_OK)
  {
    _sqlite_drv_query_error (err, query);
    return EFAILURE;
  }

  return 0;
}

int _ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  struct _sqlite_drv_storage *s = (struct _sqlite_drv_storage *) CTX->storage;
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  buffer *query;
  char *err=NULL;
  char scratch[1024];
  char queryhead[1024];
  int writes = 0;

  if (diction->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: invalid database handle (NULL)");
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
            " WHERE token IN (");

  buffer_cat (query, queryhead);

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while (ds_term)
  {
    snprintf (scratch, sizeof (scratch), "'%" LLU_FMT_SPEC "'", ds_term->key);
    buffer_cat (query, scratch);
    ds_term = ds_diction_next(ds_c);

    if (writes > 2500 || ds_term == NULL) {
      buffer_cat (query, ")");

      if ((sqlite3_exec(s->dbh, query->data, NULL, NULL, &err))!=SQLITE_OK)
      {
        _sqlite_drv_query_error (err, query->data);
        buffer_destroy(query);
        return EFAILURE;
      }

      buffer_copy(query, queryhead);
      writes = 0;

    } else {
      writes++;
      if (ds_term)
        buffer_cat (query, ",");
    }
  }
  ds_diction_close(ds_c);

  if (writes) {
    buffer_cat (query, ")");

    if ((sqlite3_exec(s->dbh, query->data, NULL, NULL, &err))!=SQLITE_OK)
    {
      _sqlite_drv_query_error (err, query->data);
      buffer_destroy(query);
      return EFAILURE;
    }
  }

  buffer_destroy (query);
  return 0;
}

void *_ds_connect (DSPAM_CTX *CTX)
{
  return NULL;
}


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

