/* $Id: cssclean.c,v 1.138 2011/06/28 00:13:48 sbajic Exp $ */

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

/* cssclean.c - rebuild a hash database, omitting hapaxes */

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
#include <libgen.h>

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

#define READ_ATTRIB(A)          _ds_read_attribute(agent_config, A)
#define MATCH_ATTRIB(A, B)      _ds_match_attribute(agent_config, A, B)

int DO_DEBUG 
#ifdef DEBUG
= 1
#else
= 0
#endif
;

#include "read_config.h"
#include "hash_drv.h"
#include "error.h"
#include "language.h"
#include "utils.h"
 
#define SYNTAX "syntax: cssclean [filename]" 

int cssclean(const char *filename, int heavy);

int main(int argc, char *argv[]) {
  int r;
  int heavy=0;

  if (argc<2) {
    fprintf(stderr, "%s\n", SYNTAX);
    exit(EXIT_FAILURE);
  }

  if ( (argc>=3) && (!strcmp(argv[2], "heavy") ) )heavy=1;

  agent_config = read_config(NULL);
  if (!agent_config) {
    LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
    exit(EXIT_FAILURE);
  }

  r = cssclean(argv[1], heavy);
  
  if (r) {
    fprintf(stderr, "cssclean failed on error %d\n", r);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

int cssclean(const char *filename, int heavy) {
  unsigned long i;
  FILE* lockfile = NULL;
  struct _hash_drv_map old, new;
  hash_drv_spam_record_t rec;
  struct hash_drv_extent const *ext;
  char *dir = NULL;
  char newfile[512];
  struct stat st;
  unsigned long spam, nonspam, cntr;
  int drop, prb;
  char *filenamecopy;
  unsigned long hash_rec_max = HASH_REC_MAX;
  unsigned long max_seek     = HASH_SEEK_MAX;
  unsigned long max_extents  = 0;
  unsigned long extent_size  = HASH_EXTENT_MAX;
  int pctincrease = 0;
  int flags = 0;
  int rc = EFAILURE;
  ssize_t opt_l;

  if (READ_ATTRIB("HashRecMax"))
    hash_rec_max = strtol(READ_ATTRIB("HashRecMax"), NULL, 0);

  if (READ_ATTRIB("HashExtentSize"))
    extent_size = strtol(READ_ATTRIB("HashExtentSize"), NULL, 0);

  if (READ_ATTRIB("HashMaxExtents"))
    max_extents = strtol(READ_ATTRIB("HashMaxExtents"), NULL, 0);

  if (READ_ATTRIB("HashPctIncrease")) {
    pctincrease = atoi(READ_ATTRIB("HashPctIncrease"));
    if (pctincrease > 100) {
        LOG(LOG_ERR, "HashPctIncrease out of range; ignoring");
        pctincrease = 0;
    }
  }

  if (MATCH_ATTRIB("HashAutoExtend", "on"))
    flags |= HMAP_AUTOEXTEND;

  if (!MATCH_ATTRIB("HashNoHoles", "on"))
    flags |= HMAP_HOLES;

  if (MATCH_ATTRIB("HashPow2", "on"))
    flags |= HMAP_POW2;

  if (MATCH_ATTRIB("HashFallocate", "on"))
    flags |= HMAP_FALLOCATE;

  if (READ_ATTRIB("HashMaxSeek"))
     max_seek = strtol(READ_ATTRIB("HashMaxSeek"), NULL, 0);

  if (stat(filename, &st) < 0)
    return EFAILURE;

  /* create a temporary file name */
  dir = strdup(filename);
  if (dir == NULL)
    goto end;

  filenamecopy = strdup(filename);
  if (filenamecopy == NULL)
    goto end;

  snprintf(newfile, sizeof(newfile), "/%s/.dspam%u.css", dirname((char *) filenamecopy), (unsigned int) getpid());

  lockfile = _hash_tools_lock_get (filename);
  if (lockfile == NULL)
    goto end;

  if (_hash_drv_open(filename, &old, 0, max_seek,
                     max_extents, extent_size, pctincrease,
		     flags | HMAP_ALLOW_BROKEN))
    goto end;

  flags &= ~(HMAP_HOLES);
  flags |= HMAP_FALLOCATE;

  if (_hash_drv_open(newfile, &new, hash_rec_max, max_seek,
                     max_extents, extent_size, pctincrease, flags)) {
    _hash_drv_close(&old);
    goto end;
  }

  /* preserve counters, permissions, and ownership */
  {
	  struct _hash_drv_header const	*old_hdr = old.addr;
	  struct _hash_drv_header	*new_hdr = new.addr;
	  memcpy(&new_hdr->totals, &old_hdr->totals, sizeof new_hdr->totals);
  }

  if (fchown(new.fd, st.st_uid, st.st_gid) < 0) {
    _hash_drv_close(&new);
    _hash_drv_close(&old);
    unlink(newfile);
    goto end;
  }

  if (fchmod(new.fd, st.st_mode) < 0) {
    _hash_drv_close(&new);
    _hash_drv_close(&old);
    unlink(newfile);
    goto end;
  }

  ext = NULL;
  do {
	  ext = _hash_drv_next_extent(&old, ext);
	  if (!ext)
		  break;

	  if (ext->is_broken)
		  LOG(LOG_INFO, "css file was corrupted, fixing it now");

	  hash_drv_ext_prefetch(ext);

	  for (i = 0; i < ext->num_records; ++i) {
		  rec = &ext->records[i];

		  nonspam = rec->nonspam & 0x0fffffff;
		  spam = rec->spam & 0x0fffffff;
		  cntr = ((rec->nonspam>>28) & 0x0f) |
			  ((rec->spam>>24) & 0xf0);

		  if(cntr<255)cntr++;
		  rec->nonspam=nonspam|((cntr&0x0f)<<28);
		  rec->spam=spam|((cntr&0xf0)<<24);

		  if(nonspam+spam>0)
			  prb=(abs(nonspam-spam)*1000)/(nonspam+spam);
		  else
			  prb=1000;

		  drop=0;

		  if(heavy) {
			  if( (nonspam+spam<=1) ||
			      (prb<100)
				  )drop=1;
		  }
		  else {
			  if( ((nonspam*2+spam<5)&&(cntr>60)) ||
			      ((nonspam+spam<=1)&&(cntr>15))  ||
			      ((prb<200)&&(cntr>15)) ||
			      (cntr>120)
				  ) drop=1;
		  }

		  if (rec->hashcode && !drop) {
			  rc = _hash_drv_set_spamrecord(&new, rec, 0);
			  if (rc < 0) {
				  LOG(LOG_WARNING, "aborting on error");
				  _hash_drv_close(&new);
				  _hash_drv_close(&old);
				  unlink(newfile);
				  goto end;
			  }

			  if (rc) {
				  LOG(LOG_DEBUG,
				      "%s: extent #%u@%zu+%lu: hashcode %llx already used",
				      old.filename, ext->idx, ext->offset, i,
				      rec->hashcode);
			  }
		  }
	  }
  } while (!hash_drv_ext_is_eof(&old, ext));

  opt_l = css_optimize(&new, old.flags & ~HMAP_ALLOW_BROKEN);
  if (opt_l < 0) {
	  LOG(LOG_ERR, "%s: failed to optimize CSS", old.filename);
	  rc = EXIT_FAILURE;
	  _hash_drv_close(&new);
	  _hash_drv_close(&old);
	  unlink(newfile);
	  goto end;
  }

  if (opt_l > 0)
	  LOG(LOG_INFO, "%s: freed %zd bytes", old.filename, opt_l);

  _hash_drv_close(&new);
  _hash_drv_close(&old);
  if (rename(newfile, filename) < 0)
    goto end;
  rc = 0;

end:
  free(dir);
  _hash_tools_lock_free(filename, lockfile);
  return rc;
}
