/* $Id: hash_drv.c,v 1.296 2011/06/28 00:13:48 sbajic Exp $ */

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

/*
 * hash_drv.c - hash-based storage driver 
 *              mmap'd flat-file storage for fast storage
 *              inspired by crm114 sparse spectra algorithm 
 *
 * DESCRIPTION
 *   This driver uses a random access file for storage. It is exceptionally fast
 *   and does not require any third-party dependencies. The auto-extend
 *   functionality allows the file to grow as needed.
 */

#define READ_ATTRIB(A)	   _ds_read_attribute(CTX->config->attributes, A)
#define MATCH_ATTRIB(A, B) _ds_match_attribute(CTX->config->attributes, A, B)

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <string.h>
#include <stdbool.h>
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

#include <sys/mman.h>

#include "storage_driver.h"
#include "config_shared.h"
#include "hash_drv.h"
#include "libdspam.h"
#include "config.h"
#include "error.h"
#include "language.h"
#include "util.h"

int
dspam_init_driver (DRIVER_CTX *DTX)
{
#ifdef DAEMON
  DSPAM_CTX *CTX;
  char *HashConcurrentUser;
   unsigned long connection_cache = 1;
#endif

  if (DTX == NULL) 
    return 0;

#ifdef DAEMON
  CTX = DTX->CTX;
  HashConcurrentUser = READ_ATTRIB("HashConcurrentUser");

  /*
   *  Stateful concurrent hash databases are preloaded into memory and
   *  shared using a reader-writer lock. At the present moment, only a single
   *  user can be loaded into any instance of the daemon, so it is only useful
   *  if you are running with a system-wide filtering user. 
   */

  if (DTX->flags & DRF_STATEFUL) {
    char filename[MAX_FILENAME_LENGTH];
    hash_drv_map_t map;
    unsigned long hash_rec_max = HASH_REC_MAX;
    unsigned long max_seek     = HASH_SEEK_MAX;
    unsigned long max_extents  = 0;
    unsigned long extent_size  = HASH_EXTENT_MAX;
    int pctincrease = 0;
    int flags = HMAP_AUTOEXTEND;
    int ret;
    unsigned long i;

    if (READ_ATTRIB("HashConnectionCache") && !HashConcurrentUser)
      connection_cache = strtol(READ_ATTRIB("HashConnectionCache"), NULL, 0);

    DTX->connection_cache = connection_cache;

    if (READ_ATTRIB("HashRecMax"))
      hash_rec_max = strtol(READ_ATTRIB("HashRecMax"), NULL, 0);

    if (READ_ATTRIB("HashExtentSize"))
      extent_size = strtol(READ_ATTRIB("HashExtentSize"), NULL, 0);

    if (READ_ATTRIB("HashMaxExtents"))
      max_extents = strtol(READ_ATTRIB("HashMaxExtents"), NULL, 0);

    if (!MATCH_ATTRIB("HashAutoExtend", "on"))
      flags &= ~HMAP_AUTOEXTEND;

    if (!MATCH_ATTRIB("HashNoHoles", "on"))
      flags |= HMAP_HOLES;

    if (MATCH_ATTRIB("HashPow2", "on"))
      flags |= HMAP_POW2;

    if (MATCH_ATTRIB("HashFallocate", "on"))
      flags |= HMAP_FALLOCATE;

    if (READ_ATTRIB("HashPctIncrease")) {
      pctincrease = atoi(READ_ATTRIB("HashPctIncrease"));
      if (pctincrease > 100) {
          LOG(LOG_ERR, "HashPctIncrease out of range; ignoring");
          pctincrease = 0;
      }
    }

    if (READ_ATTRIB("HashMaxSeek"))
      max_seek = strtol(READ_ATTRIB("HashMaxSeek"), NULL, 0);

    /* Connection array (just one single connection for hash_drv) */
    DTX->connections = calloc(1, sizeof(struct _ds_drv_connection *) * connection_cache);
    if (DTX->connections == NULL) 
      goto memerr;

    /* Initialize Connections */
    for(i=0;i<connection_cache;i++) {
      DTX->connections[i] = calloc(1, sizeof(struct _ds_drv_connection));
      if (DTX->connections[i] == NULL) 
        goto memerr;

      /* Our connection's storage structure */
      if (HashConcurrentUser) {
        DTX->connections[i]->dbh = calloc(1, sizeof(struct _hash_drv_map));
        if (DTX->connections[i]->dbh == NULL) 
          goto memerr;
        pthread_rwlock_init(&DTX->connections[i]->rwlock, NULL);
      } else {
        DTX->connections[i]->dbh = NULL;
        pthread_mutex_init(&DTX->connections[i]->lock, NULL);
      }
    }

    /* Load concurrent database into resident memory */
    if (HashConcurrentUser) {
      map = (hash_drv_map_t) DTX->connections[0]->dbh;

      /* Tell the server our connection lock will be reader/writer based */
      if (!(DTX->flags & DRF_RWLOCK))
        DTX->flags |= DRF_RWLOCK;

      _ds_userdir_path(filename, DTX->CTX->home, HashConcurrentUser, "css");
      _ds_prepare_path_for(filename);
      LOGDEBUG("preloading %s into memory via mmap()", filename);
      ret = _hash_drv_open(filename, map, hash_rec_max, 
          max_seek, max_extents, extent_size, pctincrease, flags); 

      if (ret) {
        LOG(LOG_CRIT, "_hash_drv_open(%s) failed on error %d: %s", 
                      filename, ret, strerror(errno)); 
        free(DTX->connections[0]->dbh);
        free(DTX->connections[0]);
        free(DTX->connections);
        return EFAILURE;
      }
    }
  }
#endif

  return 0;

#ifdef DAEMON
memerr:
  if (DTX) {
    if (DTX->connections) {
      unsigned long i;
      for(i=0;i<connection_cache;i++) {
        if (DTX->connections[i])
          free(DTX->connections[i]->dbh);
        free(DTX->connections[i]);
      }
    }
    free(DTX->connections);
  }
  LOG(LOG_CRIT, ERR_MEM_ALLOC);
  return EUNKNOWN;
#endif
  
}

int
dspam_shutdown_driver (DRIVER_CTX *DTX)
{
#ifdef DAEMON
  DSPAM_CTX *CTX;

  if (DTX && DTX->CTX) {
    char *HashConcurrentUser;
    CTX = DTX->CTX;
    HashConcurrentUser = READ_ATTRIB("HashConcurrentUser");

    if (DTX->flags & DRF_STATEFUL) {
      int connection_cache = 1;

      if (READ_ATTRIB("HashConnectionCache") && !HashConcurrentUser)
        connection_cache = strtol(READ_ATTRIB("HashConnectionCache"), NULL, 0);

      LOGDEBUG("unloading hash database from memory");
      if (DTX->connections) {
        int i;
        for(i=0;i<connection_cache;i++) {
          LOGDEBUG("unloading connection object %d", i);
          if (DTX->connections[i]) {
            if (!HashConcurrentUser) {
              pthread_mutex_destroy(&DTX->connections[i]->lock);
            }
            else {
              pthread_rwlock_destroy(&DTX->connections[i]->rwlock);
              hash_drv_map_t map = (hash_drv_map_t) DTX->connections[i]->dbh;
              if (map)
                _hash_drv_close(map);
            }
            free(DTX->connections[i]->dbh);
            free(DTX->connections[i]);
          }
        }
        free(DTX->connections);
      }
    }
  }
#endif

  return 0;
}

int
_hash_drv_lock_get (
  DSPAM_CTX *CTX,
  struct _hash_drv_storage *s,
  const char *username)
{
  char filename[MAX_FILENAME_LENGTH];
  int r;

  _ds_userdir_path(filename, CTX->home, username, "lock");
  _ds_prepare_path_for(filename);

  s->lock = fopen(filename, "a");
  if (s->lock == NULL) {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror(errno));
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
_hash_drv_lock_free (
  struct _hash_drv_storage *s, 
  const char *username)
{
  int r;

  if (username == NULL)
    return 0;

  r = _ds_free_fcntl_lock(fileno(s->lock));
  if (!r) {
    fclose(s->lock);
  } else {
    LOG(LOG_ERR, ERR_IO_LOCK_FREE, username, r, strerror(errno));
  }

  return r;
}

FILE*
_hash_tools_lock_get (const char *cssfilename)
{
  char filename[MAX_FILENAME_LENGTH];
  char *pPeriod;
  int r;
  FILE* lockfile = NULL;

  if (cssfilename == NULL)
    return NULL;
  pPeriod = strrchr(cssfilename, '.');
  if (pPeriod == NULL || strcmp(pPeriod + 1, "css") || (size_t)(pPeriod - cssfilename + 5) >= sizeof(filename))
    return NULL;
  strncpy(filename, cssfilename, pPeriod - cssfilename + 1);
  strcpy(filename + (pPeriod - cssfilename + 1), "lock");
  _ds_prepare_path_for(filename);

  lockfile = fopen(filename, "a");
  if (lockfile == NULL) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return NULL;
  }
  r = _ds_get_fcntl_lock(fileno(lockfile));
  if (r) {
    fclose(lockfile);
	lockfile = NULL;
    LOG(LOG_ERR, ERR_IO_LOCK, filename, r, strerror(errno));
  }
  return lockfile;
}

int
_hash_tools_lock_free (
  const char *cssfilename,
  FILE* lockfile)
{
  int r;

  if (cssfilename == NULL || lockfile == NULL)
    return 0;

  r = _ds_free_fcntl_lock(fileno(lockfile));
  if (!r) {
    fclose(lockfile);
  } else {
    LOG(LOG_ERR, ERR_IO_LOCK_FREE, cssfilename, r, strerror(errno));
  }

  return r;
}

static int write_all(int fd, void const *buf, size_t len)
{
	while (len > 0) {
		ssize_t	l = write(fd, buf, len);

		if (l < 0 && errno == EINTR)
			continue;
		else if (l <= 0)
			break;
		else {
			buf += l;
			len -= l;
		}
	}

	return len == 0;
}

static int _hash_drv_allocate(hash_drv_map_t map,
			      off_t extend_start,
			      struct _hash_drv_header const *header)
{
	struct _hash_drv_spam_record	rec;
	int				rc;
	off_t				total_sz;

	/* TODO: check for overflows? */
	total_sz  = header->hash_rec_max * sizeof rec;
	total_sz += sizeof *header;

	if (map->flags & HMAP_HOLES) {
		rc = ftruncate(map->fd, extend_start + total_sz);
		if (rc < 0) {
			LOG(LOG_ERR,
			    "unable to truncate hash file '%s': %s",
			    map->filename, strerror(errno));
			goto out;
		}
	} else if (map->flags & HMAP_FALLOCATE) {
		rc = posix_fallocate(map->fd, extend_start, total_sz);
		if (rc < 0) {
			LOG(LOG_ERR,
			    "unable to fallocate hash file '%s': %s",
			    map->filename, strerror(errno));
			goto out;
		}
	}

	rc = -1;
	if (!write_all(map->fd, header, sizeof *header)) {
		LOG(LOG_ERR, "failed to write header in hash file '%s': %s",
		    map->filename, strerror(errno));
		goto out;
	}

	if (!(map->flags & (HMAP_HOLES | HMAP_FALLOCATE))) {
		size_t		i;

		memset(&rec, 0, sizeof rec);

		for (i = 0; i < header->hash_rec_max; ++i) {
			if (!write_all(map->fd, &rec, sizeof rec)) {
				LOG(LOG_ERR,
				    "failed to fill hash file '%s': %s",
				    map->filename, strerror(errno));
				goto out;
			}
		}
	}

	rc = 0;

out:
	if (rc < 0) {
		if (ftruncate(map->fd, extend_start) < 0) {
			LOG(LOG_ERR, "unable to restore hash file '%s': %s",
			    map->filename, strerror(errno));
			goto out;
		}
	}

	return rc;
}

static int hash_drv_mmap(hash_drv_map_t map)
{
	int mmap_flags = PROT_READ + PROT_WRITE;

	map->file_len = lseek(map->fd, 0, SEEK_END);
	map->addr = mmap(NULL, map->file_len, mmap_flags, MAP_SHARED, map->fd, 0);
	if (map->addr == MAP_FAILED) {
		close(map->fd);
		map->addr = 0;
		return -1;
	}

	return 0;
}

static bool is_prime(unsigned long v)
{
	/* algorithm is very simple and can take a long time for large v.
	 * Place an upper limit; we do not need perfect primes */
	static unsigned long const	MAX_TEST = 10000;
	unsigned long	f;

	if (v > 2 && v % 2 == 0)
		return false;

	for (f = 3; f*f <= v && f < MAX_TEST; f += 2) {
		if (v % f == 0)
			return false;
	}

	return v >= 2;
}

static unsigned long roundup_hash_op(struct _hash_drv_map const *map,
				     unsigned long v)
{
	if (map->flags & HMAP_POW2) {
		if (v <= 1)
			v = 0;
		else
			v = (sizeof(unsigned long) * 8 - __builtin_clzl(v - 1));

		v = 1ul << v;
	} else {
		while (!is_prime(v) && v < ULONG_MAX)
			++v;
	}

	return v;
}

int _hash_drv_open(
  const char *filename, 
  hash_drv_map_t map, 
  unsigned long recmaxifnew,
  unsigned long max_seek,
  unsigned long max_extents,
  unsigned long extent_size,
  int pctincrease,
  int flags) 
{
  int open_flags = O_RDWR | O_CLOEXEC;

  map->fd = open(filename, open_flags);
  map->flags = flags;

  /*
   *  Create a new hash database if desired. The record count written in the
   *  first segment will be recmaxifnew. Once the file is created, it's then
   *  mmap()'d into memory as usual.
   */

  if (map->fd < 0 && recmaxifnew) {
    struct _hash_drv_header header = {
	    .hash_rec_max	= roundup_hash_op(map, recmaxifnew),
	    .flags		= HASH_FILE_FLAG_HASHFN_DIV,
    };

    map->fd = open(filename, O_CREAT|O_WRONLY|O_CLOEXEC, 0660);
    if (map->fd<0) {
      LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror(errno));
      return EFILE;
    }

    if (_hash_drv_allocate(map, 0, &header) < 0)
      goto WRITE_ERROR;

    close(map->fd);
    map->fd = open(filename, open_flags);
  }

  if (map->fd < 0) {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror(errno));
    return EFILE;
  }

  if (hash_drv_mmap(map))
	  return EFAILURE;

  strlcpy(map->filename, filename, MAX_FILENAME_LENGTH);
  map->max_seek    = max_seek;
  map->max_extents = max_extents;
  map->extent_size = extent_size;
  map->pctincrease = pctincrease;

  map->extents = NULL;
  map->num_extents = 0;

  return 0;

WRITE_ERROR:
  close(map->fd);
  unlink(filename);
  LOG(LOG_ERR, ERR_IO_FILE_WRITING, filename, strerror(errno));
  return EFILE;
}

int
_hash_drv_close(hash_drv_map_t map) {
  int r;

  if (!map->addr)
    return EINVAL;

  r = munmap(map->addr, map->file_len);
  if (r) {
    LOG(LOG_WARNING, "munmap failed on error %d: %s", r, strerror(errno));
  }

  free(map->extents);
  close(map->fd);

  map->addr = 0;

  return r;
}


int
_ds_init_storage (DSPAM_CTX * CTX, void *dbh)
{
  struct _hash_drv_storage *s = NULL;
  hash_drv_map_t map = NULL;

  if (CTX == NULL)
    return EINVAL;

  if (!CTX->home) {
    LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
    return EINVAL;
  }

  if (CTX->flags & DSF_MERGED) {
    LOG(LOG_ERR, ERR_DRV_NO_MERGED);
    return EINVAL;
  }

  if (CTX->storage)
    return EINVAL;

  /* Persistent driver storage */

  s = calloc (1, sizeof (struct _hash_drv_storage));
  if (s == NULL)
  {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  /* If running in HashConcurrentUser mode, use existing hash mapping */

  if (dbh) {
    map = dbh;
    s->dbh_attached = 1;
  } else {
    map = calloc(1, sizeof(struct _hash_drv_map));
    if (!map) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      free(s);
      return EUNKNOWN;
    }
    s->dbh_attached = 0;
  }

  s->map = map;

  /* Mapping defaults */

  s->hash_rec_max = HASH_REC_MAX;
  s->max_seek     = HASH_SEEK_MAX;
  s->max_extents  = 0;
  s->extent_size  = HASH_EXTENT_MAX;
  s->pctincrease  = 0;
  s->flags        = HMAP_AUTOEXTEND;

  if (READ_ATTRIB("HashRecMax"))
    s->hash_rec_max = strtol(READ_ATTRIB("HashRecMax"), NULL, 0);

  if (READ_ATTRIB("HashExtentSize"))
    s->extent_size = strtol(READ_ATTRIB("HashExtentSize"), NULL, 0);

  if (READ_ATTRIB("HashMaxExtents"))
    s->max_extents = strtol(READ_ATTRIB("HashMaxExtents"), NULL, 0);

  if (!MATCH_ATTRIB("HashAutoExtend", "on"))
    s->flags = 0;

  if (READ_ATTRIB("HashPctIncrease")) {
    s->pctincrease = atoi(READ_ATTRIB("HashPctIncrease"));
    if (s->pctincrease > 100) {
        LOG(LOG_ERR, "HashPctIncrease out of range; ignoring");
        s->pctincrease = 0;
    }
  }

  if (READ_ATTRIB("HashMaxSeek"))
    s->max_seek = strtol(READ_ATTRIB("HashMaxSeek"), NULL, 0);

  if (!dbh && CTX->username != NULL)
  {
    char db[MAX_FILENAME_LENGTH];
    int lock_result;
    int ret;

    if (CTX->group == NULL)
      _ds_userdir_path(db, CTX->home, CTX->username, "css");
    else
      _ds_userdir_path(db, CTX->home, CTX->group, "css");

    lock_result = _hash_drv_lock_get (CTX, s, 
      (CTX->group) ? CTX->group : CTX->username);
    if (lock_result < 0) 
      goto BAIL;

    ret = _hash_drv_open(db, s->map, s->hash_rec_max, s->max_seek, 
        s->max_extents, s->extent_size, s->pctincrease, s->flags);

    if (ret) {
      _hash_drv_close(s->map);
      free(s);
      return EFAILURE;
    }
  }

  CTX->storage = s;
  s->dir_handles = nt_create (NT_INDEX);

  if (_hash_drv_get_spamtotals (CTX))
  {
    LOGDEBUG ("unable to load totals.  using zero values.");
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
  struct _hash_drv_storage *s;
  struct nt_node *node_nt;
  struct nt_c c_nt;

  if (!CTX || !CTX->storage)
    return EINVAL;

  s  = (struct _hash_drv_storage *) CTX->storage;

  /* Close open file handles to directories (iteration functions) */

  node_nt = c_nt_first (s->dir_handles, &c_nt);
  while (node_nt != NULL)
  {
    DIR *dir;
    dir = (DIR *) node_nt->ptr;
    closedir (dir);
    node_nt = c_nt_next (s->dir_handles, &c_nt);
  }
  nt_destroy (s->dir_handles);

  if (CTX->operating_mode != DSM_CLASSIFY)
    _hash_drv_set_spamtotals (CTX);

  /* Close connection to hash database only if we're not concurrent */

  if (!s->dbh_attached) {
    _hash_drv_close(s->map);
    free(s->map);
    int lock_result =
      _hash_drv_lock_free (s, (CTX->group) ? CTX->group : CTX->username);
    if (lock_result < 0)
      return EUNKNOWN;
  }

  free (CTX->storage);
  CTX->storage = NULL;

  return 0;
}

int
_hash_drv_get_spamtotals (DSPAM_CTX * CTX)
{
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;
  struct _hash_drv_header const *header;

  if (s->map->addr == 0)
    return EINVAL;

  header = (void const *)((uintptr_t)s->map->addr);

  /* Totals are loaded straight from the hash header */
  memcpy(&CTX->totals, &header->totals, sizeof(struct _ds_spam_totals));
  return 0;
}

int
_hash_drv_set_spamtotals (DSPAM_CTX * CTX)
{
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;
  struct _hash_drv_header *header;

  if (s->map->addr == NULL)
    return EINVAL;

  header = (void *)((uintptr_t)s->map->addr);

  /* Totals are stored into the hash header */
  memcpy(&header->totals, &CTX->totals, sizeof(struct _ds_spam_totals));

  return 0;
}

int
_ds_getall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  ds_term_t ds_term;
  ds_cursor_t ds_c;
  struct _ds_spam_stat stat;
  struct _ds_spam_stat *p_stat = &stat;
  int ret = 0, x = 0;

  if (diction == NULL || CTX == NULL)
    return EINVAL;

  ds_c = ds_diction_cursor(diction);
  ds_term = ds_diction_next(ds_c);
  while(ds_term)
  {
    ds_term->s.spam_hits = 0;
    ds_term->s.innocent_hits = 0;
    ds_term->s.offset = 0;
    x = _ds_get_spamrecord (CTX, ds_term->key, p_stat);
    if (!x)
      ds_diction_setstat(diction, ds_term->key, p_stat);
    else if (x != EFAILURE)
      ret = x;

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
  int ret = 0;
  bool is_noop = true;

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
    int rc;
    if (!(ds_term->s.status & TST_DIRTY)) {  
      ds_term = ds_diction_next(ds_c);
      continue;
    }

    if (CTX->training_mode == DST_TOE           &&
        CTX->classification == DSR_NONE         &&
        CTX->operating_mode == DSM_CLASSIFY     &&
        diction->whitelist_token != ds_term->key  &&
        (!ds_term->name || strncmp(ds_term->name, "bnr.", 4)))
    {
      ds_term = ds_diction_next(ds_c);
      continue;
    }

    if (ds_term->s.spam_hits > CTX->totals.spam_learned)
      ds_term->s.spam_hits = CTX->totals.spam_learned;
    if (ds_term->s.innocent_hits > CTX->totals.innocent_learned)
      ds_term->s.innocent_hits = CTX->totals.innocent_learned;

    rc = _ds_set_spamrecord (CTX, ds_term->key, &ds_term->s);
    if (rc < 0 && ret == 0)
      ret = rc;
    ds_term = ds_diction_next(ds_c);

    is_noop = false;
  }
  ds_diction_close(ds_c);

  if (is_noop)
	  LOG(LOG_DEBUG, "%s: no actions executed", __func__);

  return ret;
}

int
_ds_set_spamrecord (
  DSPAM_CTX * CTX,
  unsigned long long token,
  struct _ds_spam_stat *stat)
{
  struct _hash_drv_spam_record rec;
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;
  int rc;

  rec.hashcode = token;
  rec.nonspam = (stat->innocent_hits > 0) ? stat->innocent_hits : 0;
  rec.spam = (stat->spam_hits > 0) ? stat->spam_hits : 0;

  if(rec.nonspam>0x0fffffff)rec.nonspam=0x0fffffff;
  if(rec.spam>0x0fffffff)rec.spam=0x0fffffff;

  rc = _hash_drv_set_spamrecord(s->map, &rec, stat->offset);
  if (rc < 0) {
	  LOG(LOG_DEBUG, "%s: failed to set spamrecord @%lu: %d\n",
	      __func__, stat->offset, rc);
	  return rc;
  }

  return 0;
}

int
_ds_set_signature (DSPAM_CTX * CTX, struct _ds_spam_signature *SIG,
                   const char *signature)
{
  char filename[MAX_FILENAME_LENGTH];
  char scratch[128];
  FILE *file;

  _ds_userdir_path(filename, 
                   CTX->home, 
                   (CTX->group) ? CTX->group : CTX->username, 
                   "sig");

  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));
  _ds_prepare_path_for(filename);

  file = fopen(filename, "w");
  if (!file) {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror(errno));
    return EFAILURE;
  }
  if(fwrite(SIG->data, SIG->length, 1, file)!=1) {
    fclose(file);
    unlink(filename);
    LOG(LOG_ERR, ERR_IO_FILE_WRITING, filename, strerror(errno));
    return(EFAILURE);
  }
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

  _ds_userdir_path(filename, 
                   CTX->home, 
                   (CTX->group) ? CTX->group : CTX->username,  
                   "sig");

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

  if (fread(SIG->data, statbuf.st_size, 1, file) != 1) {
    LOG(LOG_ERR, ERR_IO_FILE_READ, filename, strerror(errno));
    fclose(file);
    return EFAILURE;
  }

  SIG->length = statbuf.st_size;
  fclose(file);
  return 0;
}

void *_ds_connect (DSPAM_CTX *CTX)
{
  CTX = CTX; /* Keep compiler happy */
  return NULL;
}

int
_ds_create_signature_id (DSPAM_CTX * CTX, char *buf, size_t len)
{
  char session[64];
  char digit[6];
  int pid, j;
 
  CTX = CTX; /* Keep compiler happy */
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

  _ds_userdir_path(filename, 
                   CTX->home, 
                   (CTX->group) ? CTX->group : CTX->username, 
                   "sig");

  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));

  if (stat (filename, &statbuf)) 
    return 1;

  return 0;
}

struct _ds_storage_record *
_ds_get_nexttoken (DSPAM_CTX * CTX)
{
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;
  struct _hash_drv_spam_record rec;
  struct _ds_storage_record *sr;
  struct _ds_spam_stat stat;

  rec.hashcode = 0;

  sr = calloc(1, sizeof(struct _ds_storage_record));
  if (!sr) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  if (s->offset_nexttoken == 0) {
    s->offset_header = s->map->addr;
    s->offset_nexttoken = sizeof(struct _hash_drv_header);
    memcpy(&rec,
           (void *)((unsigned long) s->map->addr + s->offset_nexttoken),
           sizeof(struct _hash_drv_spam_record));
    if (rec.hashcode)
      _ds_get_spamrecord (CTX, rec.hashcode, &stat);
  }

  while(rec.hashcode == 0 ||
   ((unsigned long) s->map->addr + s->offset_nexttoken ==
    (unsigned long) s->offset_header + sizeof(struct _hash_drv_header) +
    (s->offset_header->hash_rec_max * sizeof(struct _hash_drv_spam_record))))
  {
    s->offset_nexttoken += sizeof(struct _hash_drv_spam_record);

    if ((unsigned long) s->map->addr + s->offset_nexttoken >
        (unsigned long) s->offset_header + sizeof(struct _hash_drv_header) +
      (s->offset_header->hash_rec_max * sizeof(struct _hash_drv_spam_record)))
    { 
      if (s->offset_nexttoken < s->map->file_len) {
        s->offset_header = (void *)((unsigned long) s->map->addr + 
          (s->offset_nexttoken - sizeof(struct _hash_drv_spam_record)));

        s->offset_nexttoken += sizeof(struct _hash_drv_header);
        s->offset_nexttoken -= sizeof(struct _hash_drv_spam_record);
      } else {
        free(sr);
        return NULL;
      }
    }

    memcpy(&rec,
           (void *)((unsigned long) s->map->addr + s->offset_nexttoken), 
           sizeof(struct _hash_drv_spam_record));
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

  _ds_userdir_path(filename, 
                   CTX->home, 
                   (CTX->group) ? CTX->group : CTX->username, 
                   "sig");

  snprintf(scratch, sizeof(scratch), "/%s.sig", signature);
  strlcat(filename, scratch, sizeof(filename));  
  return unlink(filename);
}

char * 
_ds_get_nextuser (DSPAM_CTX * CTX) 
{
  static char user[MAX_FILENAME_LENGTH];
  static char path[MAX_FILENAME_LENGTH];
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;
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
      else if (strlen(entry->d_name)>4 &&
        !strncmp ((entry->d_name + strlen (entry->d_name)) - 4, ".css", 4))
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

  user[0] = 0;
  return NULL;
}

struct _ds_storage_signature *
_ds_get_nextsignature (DSPAM_CTX * CTX)
{
  CTX = CTX; /* Keep compiler happy */
  return NULL;
}

int
_ds_delall_spamrecords (DSPAM_CTX * CTX, ds_diction_t diction)
{
  CTX = CTX; /* Keep compiler happy */
  diction = diction; /* Keep compiler happy */
  return 0;
}

static unsigned long calculate_next_rec_max(struct _hash_drv_map const *map,
					    struct hash_drv_extent const *ext)
{
	unsigned long	res;

	if (!ext)
		res = map->extent_size;
	else
		res = (ext->hash_rec_max
		       + (ext->hash_rec_max * (map->pctincrease/100.0)));

	return roundup_hash_op(map, res);
}

static int _hash_drv_autoextend(
    hash_drv_map_t map, 
    struct hash_drv_extent const *prev_ext)
{
  struct _hash_drv_header header = {
	  .hash_rec_max	= calculate_next_rec_max(map, prev_ext),
	  .flags	= ((map->flags & HMAP_POW2)
			   ? HASH_FILE_FLAG_HASHFN_POW2_0
			   : HASH_FILE_FLAG_HASHFN_DIV),
  };
  off_t lastsize;
  int rc;
  size_t i;

  if (map->addr) {
	  munmap(map->addr, map->file_len);
	  map->addr = NULL;
  }

  /* HACK: _hash_drv_next_extent() can be called with a pointer of the
   * previous extent when _hash_drv_autoextend() has been called. So we can
   * not free the whole 'map->extends' but have to invalidate the pointers.
   *
   * TODO: store offset of 'header' and adjust it? */
  for (i = 0; i < map->num_extents; ++i) {
	  struct hash_drv_extent	*ext = &map->extents[i];

	  ext->header = NULL;
	  ext->records = NULL;
  }

  LOGDEBUG("%s: adding extent #%u, size %ld (%ld + %1.2f%%)",
	   map->filename, prev_ext ? prev_ext->idx + 1 : 0,
	   header.hash_rec_max,
	   prev_ext ? prev_ext->hash_rec_max : map->extent_size,
	   (map->pctincrease/100.0));

  lastsize=lseek (map->fd, 0, SEEK_END);
  rc = _hash_drv_allocate(map, lastsize, &header);

  if (rc)
    return EFAILURE;

  return hash_drv_mmap(map);
}

struct hash_drv_extent *
_hash_drv_next_extent(hash_drv_map_t map, struct hash_drv_extent const *prev)
{
	size_t				offset = prev ? prev->next_offset : 0;
	unsigned int			idx = prev ? (prev->idx + 1) : 0;
	struct _hash_drv_spam_record	*rec;
	struct _hash_drv_header		*header;
	unsigned long			rec_max;
	struct hash_drv_extent		*ext;

	if (map->num_extents <= idx) {
		struct hash_drv_extent	*new;

		new = realloc(map->extents, (idx + 1) * sizeof map->extents[0]);
		if (!new) {
			LOG(LOG_ERR,
			    "failed to allocate cache for %u extents: %s",
			    idx, strerror(errno));
			return NULL;
		}

		memset(&new[map->num_extents], 0,
		       (idx + 1 - map->num_extents) * sizeof map->extents[0]);

		map->extents = new;
		map->num_extents = idx + 1;
	}

	ext = &map->extents[idx];
	if (ext->header)
		return ext;

	/* check whether header is within file */
	if (SIZE_MAX - sizeof *header < offset ||
	    map->file_len < offset + sizeof *header) {
		LOG(LOG_WARNING, "%s: extent #%u@%zu: file too short %zu",
		    map->filename, idx, offset, map->file_len);
		return NULL;
	}

	/* header is within file; we can read it... */
	header = (void *)((uintptr_t)map->addr + offset);
	rec_max = header->hash_rec_max;

	/* check for overflow */
	if ((SIZE_MAX - offset - sizeof *header) / sizeof *rec < rec_max) {
		LOG(LOG_WARNING,
		    "%s: extent #%u@%zu: %lu records (*%zu) will cause overflow",
		    map->filename, idx, offset, rec_max, sizeof *rec);
		return NULL;
	}

	/* check whether file is large enough for whole extent */
	if (map->file_len < offset + sizeof *header + (rec_max * sizeof *rec)) {
		if (!(map->flags & HMAP_ALLOW_BROKEN)) {
			LOG(LOG_WARNING,
			    "%s: extent #%u@%zu: %lu records (*%zu) will exceed file with len %zu",
			    map->filename, idx, offset, rec_max, sizeof *rec, map->file_len);
			return NULL;
		}

		LOGDEBUG("%s: extent #%u@%zu: %lu records (*%zu) will exceed file with len %zu",
			 map->filename, idx, offset, rec_max, sizeof *rec, map->file_len);

		ext->is_broken = true;
		ext->num_records = ((map->file_len - offset - sizeof *header)
				    / sizeof *rec);
	}

	switch (header->flags & HASH_FILE_FLAG_HASHFN_MASK) {
	case HASH_FILE_FLAG_HASHFN_DIV:
		ext->hash_fn = HASH_DRV_HASH_DIV;
		ext->hash_op = header->hash_rec_max;
		break;

	case HASH_FILE_FLAG_HASHFN_POW2_0:
		/* check whether power of two */
		if (header->hash_rec_max == 0 ||
		    (header->hash_rec_max & (header->hash_rec_max - 1)) != 0) {
			LOG(LOG_WARNING,
			    "%s: extent #%u@%zu: invalid pow2-0 has_rec_max %lu",
			    map->filename, idx, offset, header->hash_rec_max);
			return NULL;
		}

		ext->hash_fn = HASH_DRV_HASH_POW2_0;
		ext->hash_op = ffs(header->hash_rec_max) - 1;
		break;

	default:
		LOG(LOG_WARNING, "%s: extent #%u@%zu: unknown hash-fn %x",
		    map->filename, idx, offset, header->flags);
		return NULL;
	}

	ext->idx = idx;
	ext->offset = offset;
	ext->hash_rec_max = header->hash_rec_max;
	ext->header = header;
	ext->records = (void *)(&header[1]);
	ext->next_offset  = sizeof *header + offset;
	ext->next_offset += ext->hash_rec_max * sizeof ext->records[0];

	if (!ext->is_broken)
		ext->num_records = ext->hash_rec_max;

	return ext;
}

static unsigned long calc_hash_pow2_0(unsigned long long code,
				      unsigned int sft)
{
	unsigned int	i;
	unsigned long	res = 0;

	for (i = 0; i < 8 * sizeof code; i += sft) {
		res   ^= code;
		code >>= sft;
	}

	return res & ((1lu << sft) - 1);
}

static unsigned long calc_hash_value(struct hash_drv_extent const *ext,
				     unsigned long long hashcode)
{
	switch (ext->hash_fn) {
	case HASH_DRV_HASH_POW2_0:
		return calc_hash_pow2_0(hashcode, ext->hash_op);

	case HASH_DRV_HASH_DIV:
		return hashcode % ext->hash_op;

	default:
		return ULONG_MAX;	/* will trigger errors */
	}
}

static struct _hash_drv_spam_record *_hash_drv_seek(
  struct _hash_drv_map const *map,
  struct hash_drv_extent const *ext,
  unsigned long long hashcode,
  int flags)
{
  unsigned int iterations;
  struct _hash_drv_spam_record *rec = NULL;
  struct _hash_drv_spam_record *start_rec = ext->records;
  unsigned long const rec_max = ext->hash_rec_max;
  unsigned long fpos = calc_hash_value(ext, hashcode);

  if (fpos >= rec_max) {
	  LOG(LOG_WARNING,
	      "internal error: can not calculate pos for 0x%llx in extent #%u (mod %lu)",
	      hashcode, ext->idx, rec_max);

	  return NULL;
  }

  for (iterations = map->max_seek; iterations > 0; --iterations) { /* Max Iterations  */
	  rec = &start_rec[fpos];

	  if (rec->hashcode == hashcode ||	/* Match token     */
	      rec->hashcode == 0)		/* Insert on empty */
		  break;

	  ++fpos;

	  if (fpos >= rec_max)
		  fpos = 0;
  }

  if (!rec)
    return NULL;

  if (rec->hashcode == hashcode) 
    return rec;

  if (rec->hashcode == 0 && (flags & HSEEK_INSERT)) 
    return rec;

  return NULL;
}

int
_hash_drv_set_spamrecord (
  hash_drv_map_t map,
  hash_drv_spam_record_t wrec,
  unsigned long map_offset)
{
  hash_drv_spam_record_t rec;
  int rc;

  if (map->addr == NULL)
    return EINVAL;

  if (map_offset) {
    rec = (void *)((unsigned long) map->addr + map_offset);
  } else {
    struct hash_drv_extent const *ext = NULL;

    do {
      ext = _hash_drv_next_extent(map, ext);
      if (!ext) {
        rec = NULL;
	break;
      }

      rec = _hash_drv_seek(map, ext, wrec->hashcode, HSEEK_INSERT);
    } while (!rec && ext->next_offset < map->file_len);
  
    if (!rec) {
      if (!ext)
        return EFAILURE;

      if (map->flags & HMAP_AUTOEXTEND) {
        if (ext->idx >= map->max_extents && map->max_extents)
          goto FULL;
  
        if (!_hash_drv_autoextend(map, ext))
          return _hash_drv_set_spamrecord(map, wrec, map_offset);
        else
          return EFAILURE;
      } else {
        goto FULL;
      }
    }
  }

  rc = rec->hashcode ? 1 : 0;

  rec->hashcode = wrec->hashcode;
  rec->nonspam  = wrec->nonspam;
  rec->spam     = wrec->spam;

  return rc;

FULL:
  LOG(LOG_WARNING, "hash table %s full", map->filename);
  return EFAILURE;
}

static unsigned long
_hash_drv_get_spamrecord (
  hash_drv_map_t map,
  hash_drv_spam_record_t wrec)
{
  hash_drv_spam_record_t rec;
  struct hash_drv_extent const *ext = NULL;

  if (map->addr == NULL)
    return 0;

  do {
    ext = _hash_drv_next_extent(map, ext);
    if (!ext) {
	rec = NULL;
	break;
    }

    rec = _hash_drv_seek(map, ext, wrec->hashcode, 0);
  } while (!rec && ext->next_offset < map->file_len);

  if (!rec)
    return 0;

  wrec->nonspam  = rec->nonspam;
  wrec->spam     = rec->spam;
  return (uintptr_t)rec - (uintptr_t)(map->addr);
}

void hash_drv_ext_prefetch(struct hash_drv_extent const *ext)
{
	static uintptr_t	PAGE_SZ = 0;
	uintptr_t		addr = (uintptr_t)ext->records;

	if (PAGE_SZ == 0)
		PAGE_SZ = sysconf(_SC_PAGESIZE);
	if (PAGE_SZ == 0)
		return;

	addr &= ~(PAGE_SZ - 1);
	madvise((void *)addr, ext->num_records * sizeof ext->records[0],
		MADV_SEQUENTIAL);
}

int
_ds_get_spamrecord (
  DSPAM_CTX * CTX,
  unsigned long long token,
  struct _ds_spam_stat *stat)
{
  struct _hash_drv_spam_record rec;
  struct _hash_drv_storage *s = (struct _hash_drv_storage *) CTX->storage;

  rec.spam = rec.nonspam = 0;
  rec.hashcode = token;

  stat->offset = _hash_drv_get_spamrecord(s->map, &rec);
  if (!stat->offset)
    return EFAILURE;

  stat->probability   = 0.00000;
  stat->status        = 0;
  stat->innocent_hits = rec.nonspam & 0x0fffffff;
  stat->spam_hits     = rec.spam & 0x0fffffff;

  return 0;
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

