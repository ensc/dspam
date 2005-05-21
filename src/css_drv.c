/* $Id: css_drv.c,v 1.10 2005/05/21 11:46:59 jonz Exp $ */

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

/*
 * css_drv.c - CRM114 sparse spectra storage driver
 *             mmap'd flat-file storage for fast markovian-based filtering
 *
 * DESCRIPTION
 *   This driver uses a flat, fixed-size file for storage. It is exceptionally
 *   fast and does not require any third-party dependencies. The default fixed
 *   size stores up to a million records.
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#include <unistd.h>
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
#include "css_drv.h"
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
_css_drv_lock_get (
  DSPAM_CTX *CTX,
  struct _css_drv_storage *s,
  const char *username)
{
  char filename[MAX_FILENAME_LENGTH];
  int r;

  _ds_userdir_path(filename, CTX->home, username, "totals");
  _ds_prepare_path_for(filename);

  s->lock = fopen(filename, "a");
  if (s->lock == NULL) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFAILURE;
  }
  r = _ds_get_fcntl_lock(fileno(s->lock));
  if (r) {
    fclose(s->lock);
    LOG(LOG_ERR, ERR_IO_LOCK, filename, r, strerror(errno));
  }
  return r;
}

int
_css_drv_lock_free (struct _css_drv_storage *s, const char *username)
{
  int r;

  r = _ds_free_fcntl_lock(fileno(s->lock));
  if (!r) {
    fclose(s->lock);
  } else {
    LOG(LOG_ERR, ERR_IO_LOCK_FREE, username, r, strerror(errno));
  }

  return r;
}

int _css_drv_open(DSPAM_CTX *CTX, const char *filename, css_drv_map_t map) {
//  int open_flags = (CTX->operating_mode == DSM_CLASSIFY || CTX->operating_mode == DSM_TOOLS) ? O_RDONLY : O_RDWR;
//  int mmap_flags = (CTX->operating_mode == DSM_CLASSIFY || CTX->operating_mode == DSM_TOOLS) ? PROT_READ : PROT_READ | PROT_WRITE;

  int open_flags = O_RDWR;
  int mmap_flags = PROT_READ + PROT_WRITE;

  map->fd = open(filename, open_flags);
  if (map->fd < 0) {
    struct _css_drv_spam_record rec;
    long recno;

    memset(&rec, 0, sizeof(struct _css_drv_spam_record));

    FILE *f = fopen(filename, "w");
    if (!f) {
      LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
      return EFILE;
    }

    for(recno=0;recno<CSS_REC_MAX;recno++)
      fwrite(&rec, sizeof(struct _css_drv_spam_record), 1, f);
    fclose(f);
    map->fd = open(filename, open_flags);
  }

  if (map->fd < 0) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFILE;
  }

  map->addr = mmap(NULL, CSS_REC_MAX*sizeof(struct _css_drv_spam_record), mmap_flags, MAP_SHARED, map->fd, 0);
  if (map->addr == MAP_FAILED) {
    close(map->fd);
    map->addr = 0;
    return EFAILURE;
  }
  return 0;
}

int
_css_drv_close(css_drv_map_t map) {
  struct _css_drv_spam_record rec;
  int r;

  if (!map->addr)
    return EINVAL;

  r = munmap(map->addr, CSS_REC_MAX*sizeof(struct _css_drv_spam_record));
  if (r) {
    LOG(LOG_WARNING, "munmap failed on error %d: %s", r, strerror(errno));
  }
 
  /* Touch the file to ensure caching is refreshed */

  lseek (map->fd, 0, SEEK_SET);
  read (map->fd, &rec, sizeof(struct _css_drv_spam_record));
  lseek (map->fd, 0, SEEK_SET);
  write (map->fd, &rec, sizeof(struct _css_drv_spam_record));
  close(map->fd);

  map->addr = 0;

  return r;
}

int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _css_drv_storage *s = NULL;
  int r1, r2;

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

  /* don't init if we're already initialized */
  if (CTX->storage != NULL)
    return EINVAL;

  s = calloc (1, sizeof (struct _css_drv_storage));
  if (s == NULL)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  if (CTX->username != NULL)
  {
    char nonspam[MAX_FILENAME_LENGTH];
    char spam[MAX_FILENAME_LENGTH];
    int lock_result;

    if (CTX->group == NULL)
    {
      _ds_userdir_path(nonspam, CTX->home, CTX->username, "nonspam.css");
      _ds_userdir_path(spam, CTX->home, CTX->username, "spam.css");
    }
    else
    {
      _ds_userdir_path(nonspam, CTX->home, CTX->group, "nonspam.css");
      _ds_userdir_path(spam, CTX->home, CTX->group, "sig.css");
    }

    lock_result = _css_drv_lock_get (CTX, s, 
      (CTX->group) ? CTX->group : CTX->username);
    if (lock_result < 0) 
      goto BAIL;

    _ds_prepare_path_for(nonspam);
    r1 = _css_drv_open(CTX, nonspam, &s->nonspam);
    r2 = _css_drv_open(CTX, spam, &s->spam);

    if (r1 || r2) {
      _css_drv_close(&s->nonspam);
      _css_drv_close(&s->spam);
      free(s);
      return EFAILURE;
    }
  }

  CTX->storage = s;

  if (CTX->username != NULL)
  {
    if (_css_drv_get_spamtotals (CTX))
    {
      LOGDEBUG ("unable to load totals.  using zero values.");
      memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
    }
  }
  else
  {
    memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
  }

  return 0;

BAIL:
  free(s);
  return EFAILURE;
}

int
_ds_shutdown_storage (DSPAM_CTX * CTX)
{
  struct _css_drv_storage *s;
  int lock_result;

  if (!CTX || !CTX->storage)
    return EINVAL;

  s  = (struct _css_drv_storage *) CTX->storage;

  if (CTX->username != NULL && CTX->operating_mode != DSM_CLASSIFY)
  {
    _css_drv_set_spamtotals (CTX);
  }

  _css_drv_close(&s->nonspam);
  _css_drv_close(&s->spam);

  lock_result =
    _css_drv_lock_free (s, (CTX->group) ? CTX->group : CTX->username);
  if (lock_result < 0)
    return EUNKNOWN;

  free (CTX->storage);
  CTX->storage = NULL;

  return 0;
}

int
_css_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _css_drv_storage *s = (struct _css_drv_storage *) CTX->storage;
  char filename[MAX_USERNAME_LENGTH];
  FILE *file;

  if (s->nonspam.addr == 0)
    return EINVAL;

  memset (&CTX->totals, 0, sizeof (struct _ds_spam_totals));
  _ds_userdir_path(filename, CTX->home, 
    (CTX->group) ? CTX->group : CTX->username, "totals");

  file = fopen(filename, "r");
  if (!file) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFAILURE;
  }

  fscanf(file, "%lu %lu %lu %lu %lu %lu %lu %lu", 
    &CTX->totals.spam_learned,
    &CTX->totals.innocent_learned,
    &CTX->totals.spam_misclassified,
    &CTX->totals.innocent_misclassified,
    &CTX->totals.spam_corpusfed,
    &CTX->totals.innocent_corpusfed,
    &CTX->totals.spam_classified,
    &CTX->totals.innocent_classified);
  fclose(file);
  return 0;
}

int
_css_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _css_drv_storage *s = (struct _css_drv_storage *) CTX->storage;
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;

  if (s->nonspam.addr == NULL)
    return EINVAL;

  _ds_userdir_path(filename, CTX->home,
    (CTX->group) ? CTX->group : CTX->username, "totals");

  file = fopen(filename, "w");
  if (!file) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFAILURE;
  }

  fprintf(file, "%lu %lu %lu %lu %lu %lu %lu %lu\n",
    CTX->totals.spam_learned,
    CTX->totals.innocent_learned,
    CTX->totals.spam_misclassified,
    CTX->totals.innocent_misclassified,
    CTX->totals.spam_corpusfed,
    CTX->totals.innocent_corpusfed,
    CTX->totals.spam_classified,
    CTX->totals.innocent_classified);
  fclose(file);

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
  css_drv_spam_record_t rec;
  struct _css_drv_storage *s = (struct _css_drv_storage *) CTX->storage;
  unsigned long thumb, filepos;
  int wrap;

  stat->probability = 0.00000;

  if (s->nonspam.addr == NULL)
    return EINVAL;

  filepos = (token % CSS_REC_MAX) * sizeof(struct _css_drv_spam_record);
  thumb = filepos;
 
  /* Nonspam Counter */

  wrap = 0;
  rec = s->nonspam.addr+filepos;
  while(rec->hashcode != token && rec->hashcode !=0 && 
        ((wrap && filepos <thumb) || (!wrap)))
  {
    filepos += sizeof(struct _css_drv_spam_record);

    if (!wrap && filepos >= (CSS_REC_MAX * sizeof(struct _css_drv_spam_record)))
    {
      filepos = 0;
      wrap = 1;
    }
    rec = s->nonspam.addr+filepos;
  }
  if (rec->hashcode == 0) 
    return EFAILURE;
  stat->innocent_hits = rec->counter;

  /* Spam Counter */

  wrap = 0;
  filepos = thumb;
  rec = s->spam.addr+filepos;
  while(rec->hashcode != token && rec->hashcode != 0 && 
        ((wrap && filepos<thumb) || (!wrap)))
  {
    filepos += sizeof(struct _css_drv_spam_record);
    if (!wrap && filepos >= (CSS_REC_MAX * sizeof(struct _css_drv_spam_record)))    {
      filepos = 0;
      wrap = 1;
    }
    rec = s->spam.addr+filepos;
  }
  if (rec->hashcode == 0) 
    return EFAILURE;
  stat->spam_hits = rec->counter;

  return 0;
}

int
_ds_set_spamrecord (DSPAM_CTX * CTX, unsigned long long token,
                    struct _ds_spam_stat *stat)
{
  css_drv_spam_record_t rec;
  struct _css_drv_storage *s = (struct _css_drv_storage *) CTX->storage;
  unsigned long filepos, thumb;
  int wrap, iterations;

  if (s->nonspam.addr == NULL)
    return EINVAL;

  filepos = (token % CSS_REC_MAX) * sizeof(struct _css_drv_spam_record);
  thumb = filepos;

  /* Nonspam Counter */

  wrap = 0;
  iterations = 0;
  rec = s->nonspam.addr+filepos;
  while(rec->hashcode != token && rec->hashcode != 0 && 
        ((wrap && filepos<thumb) || (!wrap)))
  {
    iterations++;
    filepos += sizeof(struct _css_drv_spam_record);

    if (!wrap && filepos >= (CSS_REC_MAX * sizeof(struct _css_drv_spam_record)))
    {
      filepos = 0;
      wrap = 1;
    }
    rec = s->nonspam.addr+filepos;
  }
  if (rec->hashcode != token && rec->hashcode != 0) {
    LOG(LOG_WARNING, "nonspam.css table full. could not insert %llu. tried %lu times.", token, iterations);
    return EFAILURE;
  }
  rec->hashcode = token;
  rec->counter = stat->innocent_hits;
  if (rec->counter < 0) 
    rec->counter = 0;

  /* Spam Counter */

  wrap = 0;
  iterations = 0;
  filepos = thumb;
  rec = s->spam.addr+filepos;
  while(rec->hashcode != token && rec->hashcode != 0 && 
        ((wrap && filepos<thumb) || (!wrap)))
  {
    iterations++;
    filepos += sizeof(struct _css_drv_spam_record);

    if (!wrap && filepos >= (CSS_REC_MAX * sizeof(struct _css_drv_spam_record)))    {
      filepos = 0;
      wrap = 1;
    }
    rec = s->spam.addr+filepos;
  }

  if (rec->hashcode != token && rec->hashcode != 0) {
    LOG(LOG_WARNING, "spam.css table full. could not insert %llu. tried %lu times.", token, iterations);
    return EFAILURE;
  }
  rec->hashcode = token;
  rec->counter = stat->spam_hits;
  if (rec->counter < 0)
    rec->counter = 0;

  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  char filename[MAX_FILENAME_LENGTH];
  char scratch[128];
  FILE *file;

  _ds_userdir_path(filename, CTX->home, (CTX->group) ? CTX->group : CTX->username, "sig");
  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));
  _ds_prepare_path_for(filename);

  file = fopen(filename, "w");
  if (!file) {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror(errno));
    return EFAILURE;
  }
  fwrite(SIG->data, SIG->length, 1, file);
  fclose(file);

  return 0;
}

int
_ds_get_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  char filename[MAX_FILENAME_LENGTH];
  char scratch[128];
  FILE *file;
  struct stat statbuf;

  _ds_userdir_path(filename, CTX->home, (CTX->group) ? CTX->group : CTX->username, "sig");
  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));

  if (stat (filename, &statbuf)) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFAILURE;
  };

  SIG->data = malloc(statbuf.st_size);
  if (!SIG->data) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  file = fopen(filename, "r");
  if (!file) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFAILURE;
  }
  fread(SIG->data, statbuf.st_size, 1, file);
  SIG->length = statbuf.st_size;
  fclose(file);
  return 0;
}

void *_ds_connect (DSPAM_CTX *CTX)
{
  return NULL;
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
_ds_verify_signature (DSPAM_CTX * CTX, const char *signature)
{
  char filename[MAX_FILENAME_LENGTH];
  char scratch[128];
  struct stat statbuf;

  _ds_userdir_path(filename, CTX->home, (CTX->group) ? CTX->group : CTX->username, "sig");
  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));

  if (stat (filename, &statbuf)) 
    return 1;

  return 0;
}

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _css_drv_storage *s = (struct _css_drv_storage *) CTX->storage;
  struct _css_drv_spam_record rec;
  struct _ds_storage_record *sr;
  struct _ds_spam_stat stat;

  rec.hashcode = 0;

  sr = calloc(0, sizeof(struct _ds_storage_record));
  if (!sr) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  while(rec.hashcode == 0) {
    s->offset_nexttoken += sizeof(struct _css_drv_spam_record);
    if (s->offset_nexttoken > CSS_REC_MAX * sizeof(struct _css_drv_spam_record))
    { 
      free(sr);
      return NULL;
    }

    memcpy(&rec,s->nonspam.addr+s->offset_nexttoken, sizeof(struct _css_drv_spam_record));
    _ds_get_spamrecord (CTX, rec.hashcode, &stat);
  }

  sr->token = rec.hashcode;
  sr->spam_hits = stat.spam_hits;
  sr->innocent_hits = stat.innocent_hits;
  sr->last_hit = time(NULL);
  return sr;
}

int
_ds_delete_signature (DSPAM_CTX * CTX, const char *signature)
{
  char filename[MAX_FILENAME_LENGTH];
  char scratch[128];

  _ds_userdir_path(filename, CTX->home, (CTX->group) ? CTX->group : CTX->username, "sig");
  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));  
  return unlink(filename);
}

/* TODO */

char * 
_ds_get_nextuser (DSPAM_CTX * CTX) 
{
  return NULL;
}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  return NULL;
}

int
_ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  return 0;
}

