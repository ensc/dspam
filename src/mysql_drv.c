/* $Id: mysql_drv.c,v 1.9 2004/11/30 23:52:36 jonz Exp $ */

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
#include <mysql.h>

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
#include "lht.h"
#ifdef PREFERENCES_EXTENSION
#include "pref.h"
#include "config_shared.h"
#endif

int test(MYSQL *, char *);

int test(MYSQL *dbh, char *query) {
  int i;
  i = mysql_query(dbh, query);
  return i;
}

#define MYSQL_RUN_QUERY(A, B) mysql_query(A, B)

int
dspam_init_driver (void)
{
#if defined(MYSQL4_INITIALIZATION) && MYSQL_VERSION_ID >= 40001
  const char *server_default_groups[]=
  { "server", "embedded", "mysql_SERVER", 0 };

  if (mysql_server_init(0, NULL, (char**) server_default_groups)) {
    LOGDEBUG("dspam_init_driver() failed");
    return EFAILURE;
  }
#endif

  return 0;
}

int
dspam_shutdown_driver (void)
{
#if defined(MYSQL4_INITIALIZATION) && MYSQL_VERSION_ID >= 40001
  mysql_server_end();
#endif
  return 0;
}

int
_mysql_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  MYSQL_RES *result;
  MYSQL_ROW row;
  struct _ds_spam_totals user, group;
  int uid = -1, gid = -1;

  if (s->dbh == NULL)
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

  if (!CTX->group || CTX->flags & DSF_MERGED) 
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else 
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    if (!(CTX->flags & DSF_MERGED))
      return EINVAL;
  } else {

    uid = p->pw_uid;
  }

  if (CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_mysql_drv_getspamtotals: unable to _mysql_drv_getpwnam(%s)",
                CTX->group);
      return EINVAL;
    }

  }

  gid = p->pw_uid;

  snprintf (query, sizeof (query),
            "select uid, spam_learned, innocent_learned, "
            "spam_misclassified, innocent_misclassified, "
            "spam_corpusfed, innocent_corpusfed, "
            "spam_classified, innocent_classified "
            " from dspam_stats where (uid = %d or uid = %d)",
            uid, gid);
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    LOGDEBUG("mysql_use_result() failed in _ds_get_spamtotals()");
    return EFAILURE;
  }

  while ((row = mysql_fetch_row (result)) != NULL) {
    int rid = atoi(row[0]);
    if (rid == uid) {
      user.spam_learned			= strtol (row[1], NULL, 0);
      user.innocent_learned		= strtol (row[2], NULL, 0);
      user.spam_misclassified		= strtol (row[3], NULL, 0);
      user.innocent_misclassified  	= strtol (row[4], NULL, 0);
      user.spam_corpusfed		= strtol (row[5], NULL, 0);
      user.innocent_corpusfed		= strtol (row[6], NULL, 0);
      if (row[7] != NULL && row[8] != NULL) {
        user.spam_classified		= strtol (row[7], NULL, 0);
        user.innocent_classified	= strtol (row[8], NULL, 0);
      } else {
        user.spam_classified = 0;
        user.innocent_classified = 0;
      }
    } else {
      group.spam_learned           = strtol (row[1], NULL, 0);
      group.innocent_learned       = strtol (row[2], NULL, 0);
      group.spam_misclassified     = strtol (row[3], NULL, 0);
      group.innocent_misclassified = strtol (row[4], NULL, 0);
      group.spam_corpusfed         = strtol (row[5], NULL, 0);
      group.innocent_corpusfed     = strtol (row[6], NULL, 0);
    if (row[7] != NULL && row[8] != NULL) {
        group.spam_classified      = strtol (row[7], NULL, 0);
        group.innocent_classified  = strtol (row[8], NULL, 0);
      } else {
      group.spam_classified = 0;
          group.innocent_classified = 0;
      }
    }
  }

  mysql_free_result (result);

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
}

int
_mysql_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[1024];
  int result = 0;
  struct _ds_spam_totals user;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_mysql_drv_set_spamtotals: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
  {
    _mysql_drv_get_spamtotals (CTX);    /* undo changes to in memory totals */
    return 0;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
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

    if (CTX->totals.innocent_learned < 0)
      CTX->totals.innocent_learned = 0;
    if (CTX->totals.spam_learned < 0)
      CTX->totals.spam_learned = 0;

  }

  if (s->control_totals.innocent_learned == 0)
  {
    snprintf (query, sizeof (query),
              "insert into dspam_stats(uid, spam_learned, innocent_learned, "
              "spam_misclassified, innocent_misclassified, "
              "spam_corpusfed, innocent_corpusfed, "
              "spam_classified, innocent_classified) "
              "values(%d, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld)",
              p->pw_uid, CTX->totals.spam_learned,
              CTX->totals.innocent_learned, CTX->totals.spam_misclassified,
              CTX->totals.innocent_misclassified, CTX->totals.spam_corpusfed,
              CTX->totals.innocent_corpusfed, CTX->totals.spam_classified,
              CTX->totals.innocent_classified);
    result = MYSQL_RUN_QUERY (s->dbh, query);
  }

  if (s->control_totals.innocent_learned != 0 || result)
  {
    snprintf (query, sizeof (query),
              "update dspam_stats set spam_learned = spam_learned %s %d, "
              "innocent_learned = innocent_learned %s %d, "
              "spam_misclassified = spam_misclassified %s %d, "
              "innocent_misclassified = innocent_misclassified %s %d, "
              "spam_corpusfed = spam_corpusfed %s %d, "
              "innocent_corpusfed = innocent_corpusfed %s %d, "
              "spam_classified = spam_classified %s %d, "
              "innocent_classified = innocent_classified %s %d "
              "where uid = %d",
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

    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      if (CTX->flags & DSF_MERGED)
        memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));
      return EFAILURE;
    }
  }

  if (CTX->flags & DSF_MERGED)
    memcpy(&CTX->totals, &user, sizeof(struct _ds_spam_totals));

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  buffer *query;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  char scratch[1024];
  MYSQL_RES *result;
  MYSQL_ROW row;
  struct _ds_spam_stat stat;
  unsigned long long token = 0;
  int get_one = 0;
  int uid, gid;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_getall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  uid = p->pw_uid;
                                                                                
  if (CTX->flags & DSF_MERGED) {
    p = _mysql_drv_getpwnam (CTX, CTX->group);
    if (p == NULL)
    {
      LOGDEBUG ("_ds_getall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
                CTX->group);
      return EINVAL;
    }
                                                                                
  }
                                                                                
  gid = p->pw_uid;

  stat.spam_hits = 0;
  stat.innocent_hits = 0;

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (scratch, sizeof (scratch),
            "select uid, token, spam_hits, innocent_hits "
            "from dspam_token_data where (uid = %d or uid = %d) and token in(",
            uid, gid);
  buffer_cat (query, scratch);
  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    snprintf (scratch, sizeof (scratch), "'%llu'", node_lht->key);
    buffer_cat (query, scratch);
    node_lht->s.innocent_hits = 0;
    node_lht->s.spam_hits = 0;
    node_lht->s.probability = 0;
    node_lht->s.status = 0;
    node_lht = c_lht_next (freq, &c_lht);
    if (node_lht != NULL)
      buffer_cat (query, ",");
    get_one = 1;
  }
  buffer_cat (query, ")");

#ifdef VERBOSE
  LOGDEBUG ("mysql query length: %ld\n", query->used);
  _mysql_drv_query_error ("VERBOSE DEBUG (Not an error)", query->data);
#endif

  if (!get_one) 
    return 0;

  if (MYSQL_RUN_QUERY (s->dbh, query->data))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query->data);
    buffer_destroy(query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    buffer_destroy(query);
    LOGDEBUG("mysql_use_result() failed in _ds_getall_spamrecords()"); 
    return EFAILURE;
  }

  stat.probability = 0;
  while ((row = mysql_fetch_row (result)) != NULL)
  {
    int rid = atoi(row[0]);
    token = strtoull (row[1], NULL, 0);
    stat.spam_hits = strtol (row[2], NULL, 0);
    stat.innocent_hits = strtol (row[3], NULL, 0);
    stat.status = 0;

    if (rid == uid) 
      stat.status |= TST_DISK;

    if (stat.spam_hits < 0)
      stat.spam_hits = 0;
    if (stat.innocent_hits < 0)
      stat.innocent_hits = 0;

    lht_addspamstat (freq, token, &stat);
  }

  node_lht = c_lht_first(freq, &c_lht);
  while(node_lht != NULL && !s->control_token) {
    if (node_lht->s.spam_hits && node_lht->s.innocent_hits) {
      s->control_token = node_lht->key;
      s->control_sh = node_lht->s.spam_hits;
      s->control_ih = node_lht->s.innocent_hits;
    }
    node_lht = c_lht_next(freq, &c_lht);
  }

  if (!s->control_token)
  {
     node_lht = c_lht_first(freq, &c_lht);
     s->control_token = node_lht->key;
     s->control_sh = node_lht->s.spam_hits;
     s->control_ih = node_lht->s.innocent_hits;
  }

  mysql_free_result (result);
  buffer_destroy (query);
  return 0;
}

int
_ds_setall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_spam_stat stat, stat2;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  buffer *query;
  char scratch[1024];
  struct passwd *p;
  int update_one = 0;
#if MYSQL_VERSION_ID >= 40100
  buffer *insert;
  int insert_one = 0;
#endif

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY && 
        (CTX->training_mode != DST_TOE || 
          (freq->whitelist_token == 0 && (!(CTX->flags & DSF_NOISE))))) 
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_setall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

#if MYSQL_VERSION_ID >= 40100
  insert = buffer_create(NULL);
  if (insert == NULL)
  {
    buffer_destroy(query);
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }
#endif

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
  else {
    lht_getspamstat (freq, s->control_token, &stat);
  }

  snprintf (scratch, sizeof (scratch),
            "update dspam_token_data set last_hit = current_date(), "
            "spam_hits = greatest(0, spam_hits %s %d), "
            "innocent_hits = greatest(0, innocent_hits %s %d) "
            "where uid = %d and token in(",
            (stat.spam_hits > s->control_sh) ? "+" : "-",
            abs (stat.spam_hits - s->control_sh),
            (stat.innocent_hits > s->control_ih) ? "+" : "-",
            abs (stat.innocent_hits - s->control_ih), p->pw_uid);

  buffer_cat (query, scratch);

#if MYSQL_VERSION_ID >= 40100
  buffer_copy (insert, "insert into dspam_token_data(uid, token, spam_hits, "
                       "innocent_hits, last_hit) values");
#endif

  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    int wrote_this = 0;
    if (CTX->training_mode == DST_TOE           && 
        CTX->classification == DSR_NONE         &&
        CTX->operating_mode == DSM_CLASSIFY     &&
        freq->whitelist_token != node_lht->key  &&
        (!node_lht->token_name || strncmp(node_lht->token_name, "bnr.", 4)))
    {
      node_lht = c_lht_next(freq, &c_lht);
      continue;
    }
      
    lht_getspamstat (freq, node_lht->key, &stat2);

    if (!(stat2.status & TST_DIRTY)) {
      node_lht = c_lht_next(freq, &c_lht);
      continue;
    } else {
      stat2.status &= ~TST_DIRTY;
    }

    if (!(stat2.status & TST_DISK))
    {
#if MYSQL_VERSION_ID >= 40100
      char ins[1024];

      /* If we're processing a message with a MERGED group, assign it based on
         an empty count and not the current count (since the current count
         also includes the global group's tokens).

         If we're not using MERGED, or if a tool is running, assign it based
         on the actual count (so that tools like dspam_merge don't break) */

      if (CTX->flags & DSF_MERGED) {
        snprintf (ins, sizeof (ins),
                  "%s(%d, '%llu', %d, %d, current_date())",
                   (insert_one) ? ", " : "",
                   p->pw_uid,
                   node_lht->key,
                   stat.spam_hits > s->control_sh ? 1 : 0,
                   stat.innocent_hits > s->control_ih ? 1 : 0);
      } else {
        snprintf (ins, sizeof (ins),
                  "%s(%d, '%llu', %ld, %ld, current_date())",
                   (insert_one) ? ", " : "",
                   p->pw_uid,
                   node_lht->key,
                   stat2.spam_hits > 0 ? (long) 1 : (long) 0,
                   stat2.innocent_hits > 0 ? (long) 1 : (long) 0);
      }

      insert_one = 1;
      buffer_cat(insert, ins);
#else
      char insert[1024];

      /* If we're processing a message with a MERGED group, assign it based on
         an empty count and not the current count (since the current count
         also includes the global group's tokens).

         If we're not using MERGED, or if a tool is running, assign it based
         on the actual count (so that tools like dspam_merge don't break) */

      if (CTX->flags & DSF_MERGED) {
        snprintf (insert, sizeof (insert),
                  "insert into dspam_token_data(uid, token, spam_hits, "
                  "innocent_hits, last_hit) values(%d, '%llu', %d, %d, "
                  "current_date())",
                   p->pw_uid,
                   node_lht->key,
                   stat.spam_hits > s->control_sh ? 1 : 0,
                   stat.innocent_hits > s->control_ih ? 1 : 0);
      } else {
        snprintf(insert, sizeof (insert),
                 "insert into dspam_token_data(uid, token, spam_hits, "
                 "innocent_hits, last_hit) values(%d, '%llu', %ld, %ld, "
                 "current_date())",
                 p->pw_uid,
                 node_lht->key,
                 stat2.spam_hits > 0 ? 1 : 0,
                 stat2.innocent_hits > 0 ? 1 : 0);
      }

      if (MYSQL_RUN_QUERY (s->dbh, insert))
        stat2.status |= TST_DISK;
#endif
    }

    if (stat2.status & TST_DISK) {
      snprintf (scratch, sizeof (scratch), "'%llu'", node_lht->key);
      buffer_cat (query, scratch);
      update_one = 1;
      wrote_this = 1;
    }
 
    node_lht->s.status |= TST_DISK;

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

#ifdef BNR_VERBOSE_DEBUG
  printf("Control: [%ld %ld] [%ld %ld]",  s->control_sh, s->control_ih, stat.spam_hits, stat.innocent_hits); 
#endif

  LOGDEBUG("Control: [%ld %ld] [%ld %ld]", s->control_sh, s->control_ih, stat.spam_hits, stat.innocent_hits);

  if (update_one)
  {

    if (MYSQL_RUN_QUERY (s->dbh, query->data))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query->data);
      buffer_destroy(query);
      return EFAILURE;
    }
  }

#if MYSQL_VERSION_ID >= 40100
  if (insert_one)
  {
     snprintf (scratch, sizeof (scratch),
            " ON DUPLICATE KEY UPDATE last_hit = current_date(), "
            "spam_hits = greatest(0, spam_hits %s %d), "
            "innocent_hits = greatest(0, innocent_hits %s %d) ",
            (stat.spam_hits > s->control_sh) ? "+" : "-",
            abs (stat.spam_hits - s->control_sh) > 0 ? 1 : 0,
            (stat.innocent_hits > s->control_ih) ? "+" : "-",
            abs (stat.innocent_hits - s->control_ih) > 0 ? 1 : 0);

    buffer_cat(insert, scratch);

    if (MYSQL_RUN_QUERY (s->dbh, insert->data))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), insert->data);
      buffer_destroy(insert);
      return EFAILURE;
    }
  }

  buffer_destroy(insert);
#endif

  buffer_destroy (query);
  return 0;
}

int
_ds_get_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_spamrecord: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "select spam_hits, innocent_hits from dspam_token_data "
            "where uid = %d " "and token in('%llu') ", p->pw_uid, token);

  stat->probability = 0.0;
  stat->spam_hits = 0;
  stat->innocent_hits = 0;
  stat->status &= ~TST_DISK;

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    LOGDEBUG("mysql_use_result() failed in _ds_get_spamrecord()"); 
    return EFAILURE;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    return 0;
  }

  stat->spam_hits = strtol (row[0], NULL, 0);
  stat->innocent_hits = strtol (row[1], NULL, 0);
  stat->status |= TST_DISK;
  mysql_free_result (result);
  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  char query[1024];
  struct passwd *p;
  int result = 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: invalid database handle (NULL)");
    return EINVAL;
  }

  if (CTX->operating_mode == DSM_CLASSIFY)
    return 0;

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_spamrecord: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  /* It's either not on disk or the caller isn't using stat.status */
  if (!(stat->status & TST_DISK))
  {
    snprintf (query, sizeof (query),
              "insert into dspam_token_data(uid, token, spam_hits, "
              "innocent_hits, last_hit)"
              " values(%d, '%llu', %ld, %ld, current_date())",
              p->pw_uid, token, stat->spam_hits, stat->innocent_hits);
    result = MYSQL_RUN_QUERY (s->dbh, query);
  }

  if ((stat->status & TST_DISK) || result)
  {
    /* insert failed; try updating instead */
    snprintf (query, sizeof (query), "update dspam_token_data "
              "set spam_hits = %ld, "
              "innocent_hits = %ld "
              "where uid = %d "
              "and token = %lld", stat->spam_hits,
              stat->innocent_hits, p->pw_uid, token);

    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      return EFAILURE;
    }
  }

  return 0;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _mysql_drv_storage *s;
  MYSQL *connect;
  FILE *file;
  char filename[MAX_FILENAME_LENGTH];
  char buffer[128];
  char hostname[128] = "";
  char user[64] = "";
  char password[32] = "";
  char db[64] = "";
  int port = 3306, i = 0, real_connect_flag = 0;

  /* don't init if we're already initted */
  if (CTX->storage != NULL)
  {
    LOGDEBUG ("_ds_init_storage: storage already initialized");
    return EINVAL;
  }

  s = calloc (1, sizeof (struct _mysql_drv_storage));
  if (s == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s->dbh_attached = (dbh) ? 1 : 0;
  s->u_getnextuser[0] = 0;
  memset(&s->p_getpwnam, 0, sizeof(struct passwd));
  memset(&s->p_getpwuid, 0, sizeof(struct passwd));

  /* Read storage attributes */
  if (_ds_read_attribute(CTX->config->attributes, "MySQLServer")) {
    char *p;

    strlcpy(hostname, 
            _ds_read_attribute(CTX->config->attributes, "MySQLServer"), 
            sizeof(hostname));

    if (_ds_read_attribute(CTX->config->attributes, "MySQLPort"))
      port = atoi(_ds_read_attribute(CTX->config->attributes, "MySQLPort"));
    else
      port = 0;

    if ((p = _ds_read_attribute(CTX->config->attributes, "MySQLUser")))
      strlcpy(user, p, sizeof(user));
    if ((p = _ds_read_attribute(CTX->config->attributes, "MySQLPass")))
      strlcpy(password, p, sizeof(password));
    if ((p = _ds_read_attribute(CTX->config->attributes, "MySQLDb")))
      strlcpy(db, p, sizeof(db)); 

    if (_ds_match_attribute(CTX->config->attributes, "MySQLCompress", "true"))
      real_connect_flag = CLIENT_COMPRESS;

  } else {
    snprintf (filename, MAX_FILENAME_LENGTH, "%s/mysql.data", CTX->home);
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
      else if (i == 1)
        port = atoi (buffer);
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
    free(s);
    return EINVAL;
  }

  s->dbh = mysql_init (NULL);
  if (s->dbh == NULL)
  {
    LOGDEBUG
      ("_ds_init_storage: mysql_init: unable to initialize handle to database");
    free(s);
    return EUNKNOWN;
  }

  if (dbh) {
    connect = dbh;
  } else {
    if (hostname[0] == '/')
    {
      connect =
        mysql_real_connect (s->dbh, NULL, user, password, db, 0, hostname, 
                            real_connect_flag);
  
    }
    else
    {
      connect =
        mysql_real_connect (s->dbh, hostname, user, password, db, port, NULL,
                            real_connect_flag);
    }
  }

  if (connect == NULL)
  {
    LOG (LOG_WARNING, "%s", mysql_error (s->dbh));
    mysql_close(s->dbh);
    goto FAILURE;
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
        LOGDEBUG ("unable to load totals.  using zero values.");
      }
  }
  else
  {
    memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
    memset (&s->control_totals, 0, sizeof (struct _ds_spam_totals));
  }

  return 0;

FAILURE:
  free(s);
  LOGDEBUG("_ds_init_storage() failed");
  return EFAILURE;
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

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_shutdown_storage: invalid database handle (NULL)");
    return EINVAL;
  }

  /* Store spam totals on shutdown */
  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
      _mysql_drv_set_spamtotals (CTX);
  }

  if (! s->dbh_attached)
    mysql_close (s->dbh);
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
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  unsigned long *lengths;
  char *mem;
  char query[128];
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_signature: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "select data, length from dspam_signature_data where uid = %d and signature = \"%s\"",
            p->pw_uid, signature);
  if (mysql_real_query (s->dbh, query, strlen (query)))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    LOGDEBUG("mysql_use_result() failed in _ds_get_signature");
    return EFAILURE;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    LOGDEBUG("mysql_fetch_row() failed in _ds_get_signature");
    return EFAILURE;
  }

  lengths = mysql_fetch_lengths (result);
  if (lengths == NULL || lengths[0] == 0)
  {
    mysql_free_result (result);
    LOGDEBUG("mysql_fetch_lengths() failed in _ds_get_signature");
    return EFAILURE;
  }

  mem = malloc (lengths[0]);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    mysql_free_result (result);
    return EUNKNOWN;
  }

  memcpy (mem, row[0], lengths[0]);
  SIG->data = mem;
  SIG->length = strtol (row[1], NULL, 0);

  mysql_free_result (result);

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

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_signature; invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_signature: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  mem = calloc(1, SIG->length*3);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    buffer_destroy(query);
    return EUNKNOWN;
  }

  length = mysql_real_escape_string (s->dbh, mem, SIG->data, SIG->length);

  snprintf (scratch, sizeof (scratch),
            "insert into dspam_signature_data(uid, signature, length, created_on, data) values(%d, \"%s\", %ld, current_date(), \"",
            p->pw_uid, signature, SIG->length);
  buffer_cat (query, scratch);
  buffer_cat (query, mem);
  buffer_cat (query, "\")");

  if (mysql_real_query (s->dbh, query->data, query->used))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query->data);
    buffer_destroy(query);
    free(mem);
    return EFAILURE;
  }

  free (mem);
  buffer_destroy(query);
  return 0;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[128];

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: invalid database handle (NULL)");
    return EINVAL;
  }


  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "delete from dspam_signature_data where uid = %d and signature = \"%s\"",
            p->pw_uid, signature);
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  return 0;
}

int
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[128];
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_verify_signature: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_verisy_signature: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "select signature from dspam_signature_data where uid = %d and signature = \"%s\"",
            p->pw_uid, signature);
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL)
    return -1;

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    return -1;
  }

  mysql_free_result (result);
  return 0;
}

char *
_ds_get_nextuser (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
#ifndef VIRTUAL_USERS
  struct passwd *p;
  uid_t uid;
#endif
  char query[128];
  MYSQL_ROW row;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_nextuser: invalid database handle (NULL)");
    return NULL;
  }

  if (s->iter_user == NULL)
  {
#ifdef VIRTUAL_USERS
    strcpy (query, "select distinct username from dspam_virtual_uids");
#else
    strcpy (query, "select distinct uid from dspam_stats");
#endif
    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      return NULL;
    }

    s->iter_user = mysql_use_result (s->dbh);
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

#ifdef VIRTUAL_USERS
  strlcpy (s->u_getnextuser, row[0], sizeof (s->u_getnextuser));
#else
  uid = (uid_t) atoi (row[0]);
  p = _mysql_drv_getpwuid (CTX, uid);
  if (p == NULL)
  {
    mysql_free_result (s->iter_user);
    s->iter_user = NULL;
    return NULL;
  }

  strlcpy (s->u_getnextuser, p->pw_name, sizeof (s->u_getnextuser));
#endif

  return s->u_getnextuser;
}

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_storage_record *st;
  char query[128];
  MYSQL_ROW row;
  struct passwd *p;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nexttoken: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_record));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  if (s->iter_token == NULL)
  {
    snprintf (query, sizeof (query),
              "select token, spam_hits, innocent_hits, unix_timestamp(last_hit) from dspam_token_data where uid = %d",
              p->pw_uid);
    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      free(st);
      return NULL;
    }

    s->iter_token = mysql_use_result (s->dbh);
    if (s->iter_token == NULL) {
      free(st);
      return NULL;
    }
  }

  row = mysql_fetch_row (s->iter_token);
  if (row == NULL)
  {
    mysql_free_result (s->iter_token);
    s->iter_token = NULL;
    free(st);
    return NULL;
  }

  st->token = strtoull (row[0], NULL, 0);
  st->spam_hits = strtol (row[1], NULL, 0);
  st->innocent_hits = strtol (row[2], NULL, 0);
  st->last_hit = (time_t) strtol (row[3], NULL, 0);

  return st;
}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct _ds_storage_signature *st;
  unsigned long *lengths;
  char query[128];
  MYSQL_ROW row;
  struct passwd *p;
  char *mem;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_nextsignature: invalid database handle (NULL)");
    return NULL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_nextsignature: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return NULL;
  }

  st = calloc (1, sizeof (struct _ds_storage_signature));
  if (st == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  if (s->iter_sig == NULL)
  {
    snprintf (query, sizeof (query),
              "select data, signature, length, unix_timestamp(created_on) from dspam_signature_data where uid = %d",
              p->pw_uid);
    if (mysql_real_query (s->dbh, query, strlen (query)))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      free(st);
      return NULL;
    }

    s->iter_sig = mysql_use_result (s->dbh);
    if (s->iter_sig == NULL) {
      free(st); 
      return NULL;
    }
  }

  row = mysql_fetch_row (s->iter_sig);
  if (row == NULL)
  {
    mysql_free_result (s->iter_sig);
    s->iter_sig = NULL;
    free(st);
    return NULL;
  }

  lengths = mysql_fetch_lengths (s->iter_sig);
  if (lengths == NULL || lengths[0] == 0)
  {
    mysql_free_result (s->iter_sig);
    free(st);
    return NULL;
  }

  mem = malloc (lengths[0]);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    mysql_free_result (s->iter_sig);
    free(st);
    return NULL;
  }

  memcpy (mem, row[0], lengths[0]);
  st->data = mem;
  strlcpy (st->signature, row[1], sizeof (st->signature));
  st->length = strtol (row[2], NULL, 0);
  st->created_on = (time_t) strtol (row[3], NULL, 0);

  return st;
}

struct passwd *
_mysql_drv_getpwnam (DSPAM_CTX * CTX, const char *name)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;

#ifndef VIRTUAL_USERS
  struct passwd *q;
#if defined(_REENTRANT) && defined(HAVE_GETPWNAM_R)
  struct passwd pwbuf;
  char buf[1024];
#endif

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
  char query[256];
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->p_getpwnam.pw_name != NULL)
  {
    /* cache the last name queried */
    if (name != NULL && !strcmp (s->p_getpwnam.pw_name, name))
      return &s->p_getpwnam;

    free (s->p_getpwnam.pw_name);
    s->p_getpwnam.pw_name = NULL;
  }

  snprintf (query, sizeof (query),
            "select uid from dspam_virtual_uids where username = '%s'", name);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return NULL;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS)
      return NULL;
    return _mysql_drv_setpwnam (CTX, name);
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS)
      return NULL;
    return _mysql_drv_setpwnam (CTX, name);
  }

  if (row[0] == NULL)
  {
    mysql_free_result (result);
    if (CTX->source == DSS_ERROR || CTX->operating_mode != DSM_PROCESS)
      return NULL;
    return _mysql_drv_setpwnam (CTX, name);
  }

  s->p_getpwnam.pw_uid = strtol (row[0], NULL, 0);
  s->p_getpwnam.pw_name = strdup (name);

  mysql_free_result (result);
  return &s->p_getpwnam;

#endif
}

struct passwd *
_mysql_drv_getpwuid (DSPAM_CTX * CTX, uid_t uid)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
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

  if (s->p_getpwuid.pw_name)
    free(s->p_getpwuid.pw_name);

  memcpy (&s->p_getpwuid, q, sizeof (struct passwd));
  s->p_getpwuid.pw_name = strdup(q->pw_name);

  return &s->p_getpwuid;
#else
  char query[256];
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->p_getpwuid.pw_name != NULL)
  {
    /* cache the last uid queried */
    if (s->p_getpwuid.pw_uid == uid)
      return &s->p_getpwuid;
    free (s->p_getpwuid.pw_name);
    s->p_getpwuid.pw_name = NULL;
  }

  snprintf (query, sizeof (query),
            "select username from dspam_virtual_uids where uid = '%d'", uid);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return NULL;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL)
    return NULL;

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    return NULL;
  }

  if (row[0] == NULL)
  {
    mysql_free_result (result);
    return NULL;
  }

  s->p_getpwuid.pw_name = strdup (row[0]);
  s->p_getpwuid.pw_uid = uid;

  mysql_free_result (result);
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
    file_error (ERROR_FILE_WRITE, fn, strerror (errno));
    return;
  }

  fprintf (file, "[%s] %d: %s: %s\n", format_date_r(buf), (int) getpid (), 
    error, query); fclose (file);
  return;
}

#ifdef VIRTUAL_USERS
struct passwd *
_mysql_drv_setpwnam (DSPAM_CTX * CTX, const char *name)
{
  char query[256];
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;

  snprintf (query, sizeof (query),
            "insert into dspam_virtual_uids (uid, username) values(NULL, '%s')",
            name);

  /* we need to fail, to prevent a potential loop - even if it was inserted
   * by another process */
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
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
  char query[128];

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delete_signature: invalid database handle (NULL)");
    return EINVAL;
  }
                                                                                
  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delete_token: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "delete from dspam_token_data where uid = %d and token = \"%llu\"",
            p->pw_uid, token);
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }
                                                                                
  return 0;
}

int _ds_delall_spamrecords (DSPAM_CTX * CTX, struct lht *freq)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct lht_node *node_lht;
  struct lht_c c_lht;
  buffer *query;
  char scratch[1024];
  char queryhead[1024];
  struct passwd *p;
  int writes = 0;

  if (freq->items < 1)
    return 0;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_delall_spamrecords: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  snprintf (queryhead, sizeof(queryhead),
            "delete from dspam_token_data "
            "where uid = %d and token in(",
            p->pw_uid);

  buffer_cat (query, queryhead);

  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    snprintf (scratch, sizeof (scratch), "'%llu'", node_lht->key);
    buffer_cat (query, scratch);
    node_lht = c_lht_next (freq, &c_lht);
   
    if (writes > 2500 || node_lht == NULL) {
      buffer_cat (query, ")");

      if (MYSQL_RUN_QUERY (s->dbh, query->data))
      {
        _mysql_drv_query_error (mysql_error (s->dbh), query->data);
        buffer_destroy(query);
        return EFAILURE;
      }
      buffer_copy(query, queryhead);
      writes = 0;
   
    } else { 
      writes++;
      if (node_lht != NULL)
        buffer_cat (query, ",");
    }
  }

  if (writes) {
    buffer_cat (query, ")");

    if (MYSQL_RUN_QUERY (s->dbh, query->data))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query->data);
      buffer_destroy(query);
      return EFAILURE;
    }
  }

  buffer_destroy (query);
  return 0;
}

#ifdef PREFERENCES_EXTENSION
AGENT_PREF _ds_pref_load(
  attribute_t **config,  
  const char *username, 
  const char *home)
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[128];
  MYSQL_RES *result;
  MYSQL_ROW row; 
  DSPAM_CTX *CTX;
  AGENT_PREF PTX;
  AGENT_ATTRIB *pref;
  int uid, i = 0;

  CTX = dspam_create (NULL, NULL, home, DSM_TOOLS, 0);

  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "unable to create dspam context");
    return NULL;
  }

  _mysql_drv_set_attributes(CTX, config);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    return NULL;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_load: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
      dspam_destroy(CTX);
      return NULL;
    } else {
      uid = p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  LOGDEBUG("Loading preferences for uid %d", uid);

  snprintf(query, sizeof(query), "select preference, value "
    "from dspam_preferences where uid = %d", uid);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return NULL;
  }

  result = mysql_store_result (s->dbh);
  if (result == NULL) {
    dspam_destroy(CTX);
    if (username == NULL) 
      return NULL;
    return _ds_pref_load(config, NULL, home);
  }

  PTX = malloc(sizeof(AGENT_ATTRIB *)*(mysql_num_rows(result)+1));
  if (PTX == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC); 
    dspam_destroy(CTX);
    return NULL;
  }

  PTX[0] = NULL;

  row = mysql_fetch_row (result);
  if (row == NULL) {
    dspam_destroy(CTX);
    mysql_free_result(result);
    if (username == NULL)
      return NULL;
    return _ds_pref_load(config, NULL, home);
  }

  while(row != NULL) {
    char *p = row[0];
    char *q = row[1];

    pref = malloc(sizeof(AGENT_ATTRIB));
    if (pref == NULL) {
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
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
  dspam_destroy(CTX);
  return PTX;
}

int _ds_pref_set (
 attribute_t **config,
 const char *username, 
 const char *home,
 const char *preference,
 const char *value) 
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[128];
  DSPAM_CTX *CTX;
  int uid;
  char *m1, *m2;

  CTX = dspam_create (NULL, NULL, home, DSM_TOOLS, 0);
                                                                                
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "unable to create dspam context");
    return EUNKNOWN;
  }
                                                                                
  _mysql_drv_set_attributes(CTX, config);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    return EUNKNOWN;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;
                                                                                
  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);
                                                                                
    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_set: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
      dspam_destroy(CTX);
      return EUNKNOWN; 
    } else {
      uid = p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  m1 = calloc(1, strlen(preference)*2);
  m2 = calloc(1, strlen(value)*2);
  if (m1 == NULL || m2 == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    dspam_destroy(CTX);
    free(m1);
    free(m2);
    return EUNKNOWN;
  }
                                                                                
  mysql_real_escape_string (s->dbh, m1, preference, strlen(preference));   
  mysql_real_escape_string (s->dbh, m2, value, strlen(value)); 

  snprintf(query, sizeof(query), "delete from dspam_preferences "
    "where uid = %d and preference = '%s'", uid, m1);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    goto FAIL;
  }

  snprintf(query, sizeof(query), "insert into dspam_preferences "
    "(uid, preference, value) values(%d, '%s', '%s')", uid, m1, m2);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    goto FAIL;
  }

  dspam_destroy(CTX);
  free(m1);
  free(m2);
  return 0;

FAIL:
  free(m1);
  free(m2);
  dspam_destroy(CTX);
  LOGDEBUG("_ds_pref_set() failed");
  return EFAILURE;
}

int _ds_pref_del (
 attribute_t **config,
 const char *username,
 const char *home,
 const char *preference)
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[128];
  DSPAM_CTX *CTX;
  int uid;
  char *m1;
                                                                                
  CTX = dspam_create (NULL, NULL, home, DSM_TOOLS, 0);
                                                                                
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "unable to create dspam context");
    return EUNKNOWN;
  }
                                                                                
  _mysql_drv_set_attributes(CTX, config);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    return EUNKNOWN;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;
                                                                                
  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);
                                                                                
    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_del: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
      dspam_destroy(CTX);
      return EUNKNOWN;
    } else {
      uid = p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }
                                                                                
  m1 = calloc(1, strlen(preference)*2);
  if (m1 == NULL) 
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    dspam_destroy(CTX);
    free(m1);
    return EUNKNOWN;
  }
                                                                                
  mysql_real_escape_string (s->dbh, m1, preference, strlen(preference));

  snprintf(query, sizeof(query), "delete from dspam_preferences "
    "where uid = %d and preference = '%s'", uid, m1);
                                                                                
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    goto FAIL;
  }

  dspam_destroy(CTX);
  free(m1);
  return 0;
                                                                                
FAIL:
  free(m1);
  dspam_destroy(CTX);
  LOGDEBUG("_ds_pref_del() failed");
  return EFAILURE;

}

int _ds_pref_save(
  attribute_t **config,
  const char *username,  
  const char *home, 
  AGENT_PREF PTX) 
{
  struct _mysql_drv_storage *s;
  struct passwd *p;
  char query[128];
  DSPAM_CTX *CTX;
  AGENT_ATTRIB *pref;
  int uid, i = 0;
  char m1[257], m2[257];

  CTX = dspam_create (NULL, NULL, home, DSM_TOOLS, 0);
                                                                                
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "unable to create dspam context");
    return EUNKNOWN;
  }
                                                                                
  _mysql_drv_set_attributes(CTX, config);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    return EUNKNOWN;
  }

  s = (struct _mysql_drv_storage *) CTX->storage;

  if (username != NULL) {
    p = _mysql_drv_getpwnam (CTX, username);

    if (p == NULL)
    {
      LOGDEBUG ("_ds_pref_save: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
      dspam_destroy(CTX);
      return EFAILURE;
    } else {
      uid = p->pw_uid;
    }
  } else {
    uid = 0; /* Default Preferences */
  }

  snprintf(query, sizeof(query), "delete from dspam_preferences where uid = %d",
     uid);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    dspam_destroy(CTX);
    return EFAILURE;
  }

  for(i=0;PTX[i];i++) {
    pref = PTX[i];

    mysql_real_escape_string (s->dbh, m1, pref->attribute, 
                              strlen(pref->attribute));
    mysql_real_escape_string (s->dbh, m2, pref->value, strlen(pref->value));

    snprintf(query, sizeof(query), "insert into dspam_preferences "
      "(uid, attribute, value) values('%d', '%s', '%s')", uid, m1, m2); 

    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      dspam_destroy(CTX);
      return EFAILURE;
    }
  }

  dspam_destroy(CTX);
  return 0;
}

int _mysql_drv_set_attributes(DSPAM_CTX *CTX, attribute_t **config) {
  int i, ret = 0;
  attribute_t *t;

  for(i=0;config[i];i++) {
    t = config[i];
    if (!strncasecmp(t->key, "MySQL", 5)) 
    {
      ret += dspam_addattribute(CTX, t->key, t->value);
    }
  }

  return 0;
}

#endif

/* Neural network functions */

int _ds_get_node(DSPAM_CTX * CTX, char *user, struct _ds_neural_record *node) {
  char query[256];
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  MYSQL_RES *result;
  MYSQL_ROW row;
  uid_t n_uid;

  if (user == NULL) 
    n_uid = node->uid;
  else {
    p = _mysql_drv_getpwnam (CTX, user);
    if (p == NULL)
    {
      LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
                user);
      return EINVAL;
    }
    n_uid = p->pw_uid;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL) 
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf(query, sizeof(query), "select total_correct, total_incorrect from "
    "dspam_neural_data where uid = %d and node = %d", p->pw_uid, n_uid);

  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EUNKNOWN;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL)
    goto NEWNODE;

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    goto NEWNODE;
  }
  
  node->uid = n_uid;
  node->total_correct = strtol(row[0], NULL, 0);
  node->total_incorrect = strtol(row[1], NULL, 0);
  node->control_correct = node->total_correct;
  node->control_incorrect = node->total_incorrect;
  node->disk = 'Y';

  mysql_free_result (result);

  return 0;

NEWNODE:
  node->uid = n_uid;
  node->total_correct = 0;
  node->total_incorrect = 0;
  node->control_correct = 0;
  node->control_incorrect = 0;
  node->disk = 'N';
  return 0;
}

int _ds_set_node (DSPAM_CTX * CTX, char *user, struct _ds_neural_record *node) {
  char query[256];
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  uid_t n_uid;
                                                                                                                              
  if (user == NULL)
    n_uid = node->uid;
  else {
    p = _mysql_drv_getpwnam (CTX, user);
    if (p == NULL)
    {
      LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
                user);
      return EINVAL;
    }
    n_uid = p->pw_uid;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_mysql_drv_get_spamtotals: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  if (node->disk == 'N') {
    snprintf(query, sizeof(query), "insert into dspam_neural_data "
      "(uid, node, total_correct, total_incorrect) values ("
      "%d, %d, %ld, %ld)", p->pw_uid, node->uid, node->total_correct, 
        node->total_incorrect);

    if (MYSQL_RUN_QUERY (s->dbh, query))
      node->disk = 'Y';
  }

  if (node->disk == 'Y') {
    snprintf(query, sizeof(query), "update dspam_neural_data set "
      "total_correct = total_correct + %ld, "
      "total_incorrect = total_incorrect + %ld "
      "where uid = %d and node = %d", 
        node->total_correct - node->control_correct,
        node->total_incorrect - node->control_incorrect,
      p->pw_uid, node->uid);

    if (MYSQL_RUN_QUERY (s->dbh, query))
    {
      _mysql_drv_query_error (mysql_error (s->dbh), query);
      return EUNKNOWN;
    }
  }

  node->disk = 'Y';
                                                                                
  return 0;

}

int
_ds_get_decision (DSPAM_CTX * CTX, struct _ds_neural_decision *DEC,
                  const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  unsigned long *lengths;
  char *mem;
  char query[128];
  MYSQL_RES *result;
  MYSQL_ROW row;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_get_decision: invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_get_decision: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  snprintf (query, sizeof (query),
            "select data, length from dspam_neural_decisions where uid = %d and signature = \"%s\"",
            p->pw_uid, signature);
  if (mysql_real_query (s->dbh, query, strlen (query)))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }

  result = mysql_use_result (s->dbh);
  if (result == NULL) {
    LOGDEBUG("mysql_use_result() failed in _ds_get_decision"); 
    return EFAILURE;
  }

  row = mysql_fetch_row (result);
  if (row == NULL)
  {
    mysql_free_result (result);
    LOGDEBUG("mysql_fetch_row() failed in _ds_get_decision");
    return EFAILURE;
  }

  lengths = mysql_fetch_lengths (result);
  if (lengths == NULL || lengths[0] == 0)
  {
    mysql_free_result (result);
    LOGDEBUG("mysql_fetch_lengths() failed in _ds_get_decision");
    return EFAILURE;
  }

  mem = malloc (lengths[0]);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    mysql_free_result (result);
    return EUNKNOWN;
  }

  memcpy (mem, row[0], lengths[0]);
  DEC->data = mem;
  DEC->length = strtol (row[1], NULL, 0);

  mysql_free_result (result);

  return 0;
}

int
_ds_set_decision (DSPAM_CTX * CTX, struct _ds_neural_decision *DEC,
                   const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  unsigned long length;
  char *mem;
  char scratch[1024];
  buffer *query;
  struct passwd *p;

  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_set_decision; invalid database handle (NULL)");
    return EINVAL;
  }

  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);

  if (p == NULL)
  {
    LOGDEBUG ("_ds_set_decision: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }

  query = buffer_create (NULL);
  if (query == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  mem = malloc (DEC->length * 2);
  if (mem == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    buffer_destroy(query);
    return EUNKNOWN;
  }
  length = mysql_real_escape_string (s->dbh, mem, DEC->data, DEC->length);

  snprintf (scratch, sizeof (scratch),
            "insert into dspam_neural_decisions(uid, signature, length, created_on, data) values(%d, \"%s\", %ld, current_date(), \"",
            p->pw_uid, signature, DEC->length);
  buffer_cat (query, scratch);
  buffer_cat (query, mem);
  buffer_cat (query, "\")");

  if (mysql_real_query (s->dbh, query->data, query->used))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query->data);
    buffer_destroy(query);
    free(mem);
    return EFAILURE;
  }

  free (mem);
  return 0;
}

int
_ds_delete_decision (DSPAM_CTX * CTX, const char *signature)
{
  struct _mysql_drv_storage *s = (struct _mysql_drv_storage *) CTX->storage;
  struct passwd *p;
  char query[128];
                                                                                                                              
  if (s->dbh == NULL)
  {
    LOGDEBUG ("_ds_delete_decision: invalid database handle (NULL)");
    return EINVAL;
  }
                                                                                                                              
  if (!CTX->group || CTX->flags & DSF_MERGED)
    p = _mysql_drv_getpwnam (CTX, CTX->username);
  else
    p = _mysql_drv_getpwnam (CTX, CTX->group);
                                                                                                                              
  if (p == NULL)
  {
    LOGDEBUG ("_ds_delete_decision: unable to _mysql_drv_getpwnam(%s)",
              CTX->username);
    return EINVAL;
  }
                                                                                                                              
  snprintf (query, sizeof (query),
            "delete from dspam_neural_decisions where uid = %d and signature = \"%s\"",
            p->pw_uid, signature);
  if (MYSQL_RUN_QUERY (s->dbh, query))
  {
    _mysql_drv_query_error (mysql_error (s->dbh), query);
    return EFAILURE;
  }
                                                                                                                              
  return 0;
}

