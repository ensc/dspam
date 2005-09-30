/* $Id: csscompress.c,v 1.1 2005/09/30 07:01:14 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2
 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
 * csscompress.c - Compress a file with extents into a single large segment.
 * Once the file is in one segment, cssgrow can be used to further reduce it.
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
 
#define SYNTAX "syntax: csscompress [filename]" 

int csscompress(const char *filename);
int set_spamrecord (hash_drv_map_t map, hash_drv_spam_record_t wrec);

int main(int argc, char *argv[]) {
  char *filename;
  int r;

  if (argc<2) {
    fprintf(stderr, "%s\n", SYNTAX);
    exit(EXIT_FAILURE);
  }
   
  filename = argv[1];

  r = csscompress(filename);
  
  if (r) {
    fprintf(stderr, "csscompress failed on error %d\n", r);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

int csscompress(const char *filename) {
  int i;
  hash_drv_header_t header;
  void *offset;
  unsigned long reclen;
  struct _hash_drv_map old, new;
  hash_drv_spam_record_t rec;
  unsigned long filepos;
  char newfile[128];

  snprintf(newfile, sizeof(newfile), "/tmp/%u.css", (unsigned int) getpid());

  if (_hash_drv_open(filename, &old, 0)) 
    return EFAILURE;

  /* determine total record length */
  header = old.addr;
  reclen = 0;
  while((unsigned long) header < (unsigned long) (old.addr + old.file_len)) {
    unsigned long max = header->hash_rec_max;
    reclen += max;
    offset = header;
    offset = offset + sizeof(struct _hash_drv_header);
    max *= sizeof(struct _hash_drv_spam_record);
    offset += max;
    header = offset;
  }

  if (_hash_drv_open(newfile, &new, reclen)) {
    _hash_drv_close(&old);
    return EFAILURE;
  }

  filepos = sizeof(struct _hash_drv_header);
  header = old.addr;
  while(filepos < old.file_len) {
    printf("processing %lu records\n", header->hash_rec_max);
    for(i=0;i<header->hash_rec_max;i++) {
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
    offset = old.addr + filepos;
    header = offset;
    filepos += sizeof(struct _hash_drv_header);
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

  filepos = sizeof(struct _hash_drv_header) + ((wrec->hashcode % map->header->hash_rec_max) * sizeof(struct _hash_drv_spam_record));
  thumb = filepos;

  wrap = 0;
  iterations = 0;
  rec = map->addr+filepos;
  while(rec->hashcode != wrec->hashcode && rec->hashcode != 0 &&
        ((wrap && filepos<thumb) || (!wrap)))
  {
    iterations++;
    filepos += sizeof(struct _hash_drv_spam_record);

    if (!wrap && filepos >= (map->header->hash_rec_max * sizeof(struct _hash_drv_spam_record)))
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

