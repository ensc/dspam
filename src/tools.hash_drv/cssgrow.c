/* $Id: cssgrow.c,v 1.1 2005/09/24 01:06:11 jonz Exp $ */

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
 * cssgrow.c - Grow (or shrink) css file to accommodate a new record length
 *
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

int DO_DEBUG 
#ifdef DEBUG
= 1
#else
= 0
#endif
;

#include "hash_drv.h"
#include "error.h"
 
#define SYNTAX "syntax: cssgrow [filename] [reclen]"

int cssgrow(const char *filename, unsigned long reclen);
int set_spamrecord (hash_drv_map_t map, hash_drv_spam_record_t wrec);

int main(int argc, char *argv[]) {
  char *filename;
  unsigned long reclen;
  int r;

  if (argc<3) {
    fprintf(stderr, "%s\n", SYNTAX);
    exit(EXIT_FAILURE);
  }
   
  filename = argv[1];
  reclen = strtol(argv[2], NULL, 0);

//  printf("resizing %s to record length %lu...\n", filename, reclen);
  r = cssgrow(filename, reclen);
  
  if (r) {
    fprintf(stderr, "cssgrow failed on error %d\n", r);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

int cssgrow(const char *filename, unsigned long reclen) {
  struct _hash_drv_map old, new;
  hash_drv_spam_record_t rec;
  unsigned long filepos;
  char newfile[128];

  snprintf(newfile, sizeof(newfile), "/tmp/%u.css", (unsigned int) getpid());
  LOGDEBUG("Writing FILE %s RECLEN %lu\n", newfile, reclen);

  if (_hash_drv_open(filename, &old, 0)) 
    return EFAILURE;

  if (_hash_drv_open(newfile, &new, reclen)) {
    _hash_drv_close(&old);
    return EFAILURE;
  }

  filepos = sizeof(struct _hash_drv_header);
  while(filepos < old.file_len) {
    rec = old.addr+filepos;
    if (rec->hashcode) {
      if (set_spamrecord(&new, rec)) {
        LOG(LOG_WARNING, "aborting on error");
        _hash_drv_close(&new);
        _hash_drv_close(&old);
        unlink(newfile);
        return EFAILURE;
      }
    }
    filepos += sizeof(struct _hash_drv_spam_record);
  }

  _hash_drv_close(&new);
  _hash_drv_close(&old);
  rename(newfile, filename);
  return 0;
}

int set_spamrecord (hash_drv_map_t map, hash_drv_spam_record_t wrec)
{
  hash_drv_spam_record_t rec;
  unsigned long filepos, thumb;
  int wrap, iterations;

  if (!map || !map->addr)
    return EINVAL;

  filepos = sizeof(struct _hash_drv_header) + ((wrec->hashcode % map->header->css_rec_max) * sizeof(struct _hash_drv_spam_record));
  thumb = filepos;

  wrap = 0;
  iterations = 0;
  rec = map->addr+filepos;
  while(rec->hashcode != wrec->hashcode && rec->hashcode != 0 &&
        ((wrap && filepos<thumb) || (!wrap)))
  {
    iterations++;
    filepos += sizeof(struct _hash_drv_spam_record);

    if (!wrap && filepos >= (map->header->css_rec_max * sizeof(struct _hash_drv_spam_record)))
    {
      filepos = sizeof(struct _hash_drv_header);
      wrap = 1;
    }
    rec = map->addr+filepos;
  }
  if (rec->hashcode != wrec->hashcode && rec->hashcode != 0) {
    LOG(LOG_WARNING, "css table full. could not insert %llu. tried %lu times.", wrec->hashcode, iterations);
    return EFAILURE;
  }
  memcpy(rec, wrec, sizeof(struct _hash_drv_spam_record));

  return 0;
}

