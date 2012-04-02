/* $Id: csscompress.c,v 1.838 2011/11/16 00:19:14 sbajic Exp $ */

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

/* csscompress.c - Compress a hash database's extents */

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
#include <errno.h>

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
 
#define SYNTAX "syntax: csscompress [filename]" 

int csscompress(const char *filename);

int main(int argc, char *argv[]) {
  int r;

  if (argc<2) {
    fprintf(stderr, "%s\n", SYNTAX);
    exit(EXIT_FAILURE);
  }
   
  agent_config = read_config(NULL);
  if (!agent_config) {
    LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
    exit(EXIT_FAILURE);
  }

  r = csscompress(argv[1]);
  
  if (r) {
    fprintf(stderr, "csscompress failed on error %d\n", r);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

int csscompress(const char *filename) {
  unsigned long i;
  hash_drv_header_t header;
  void *offset;
  unsigned long reclen, extent = 0;
  struct _hash_drv_map old, new;
  hash_drv_spam_record_t rec;
  unsigned long filepos;
  char newfile[128];
  struct stat st;
  char *filenamecopy;
  unsigned long max_seek     = HASH_SEEK_MAX;
  unsigned long max_extents  = 0;
  unsigned long extent_size  = HASH_EXTENT_MAX;
  int pctincrease = 0;
  int flags = 0;

  if (READ_ATTRIB("HashExtentSize"))
    extent_size = strtol(READ_ATTRIB("HashExtentSize"), NULL, 0);

  if (READ_ATTRIB("HashMaxExtents"))
    max_extents = strtol(READ_ATTRIB("HashMaxExtents"), NULL, 0);

  if (MATCH_ATTRIB("HashAutoExtend", "on"))
    flags = HMAP_AUTOEXTEND;

  if (READ_ATTRIB("HashMaxSeek"))
     max_seek = strtol(READ_ATTRIB("HashMaxSeek"), NULL, 0);

  if (READ_ATTRIB("HashPctIncrease")) {
    pctincrease = atoi(READ_ATTRIB("HashPctIncrease"));
    if (pctincrease > 100) {
        LOG(LOG_ERR, "HashPctIncrease out of range; ignoring");
        pctincrease = 0;
    }
  }

  filenamecopy = strdup(filename);
  if (filenamecopy == NULL)
    return EFAILURE;

  snprintf(newfile, sizeof(newfile), "/%s/.dspam%u.css", dirname((char *)filenamecopy), (unsigned int) getpid());

  if (stat(filename, &st) < 0)
    return EFAILURE;

  if (_hash_drv_open(filename, &old, 0, max_seek,
                     max_extents, extent_size, pctincrease, flags))
  {
    return EFAILURE;
  }

  /* determine total record length */
  header = old.addr;
  reclen = 0;
  while((unsigned long) header < (unsigned long) ((unsigned long) old.addr + old.file_len)) {
    unsigned long max = header->hash_rec_max;
    reclen += max;
    offset = header;
    offset = (void *)((unsigned long) offset + sizeof(struct _hash_drv_header));
    max *= sizeof(struct _hash_drv_spam_record);
    offset = (void *)((unsigned long) offset + max);
    header = offset;
  }

  if (_hash_drv_open(newfile, &new, reclen,  max_seek,
                     max_extents, extent_size, pctincrease, flags))
  {
    _hash_drv_close(&old);
    return EFAILURE;
  }

  /* preserve counters, permissions, and ownership */
  memcpy(&(new.header->totals), &(old.header->totals), sizeof(new.header->totals));

  if (fchown(new.fd, st.st_uid, st.st_gid) < 0) {
    _hash_drv_close(&new);
    _hash_drv_close(&old);
    unlink(newfile);
    return EFAILURE;
  }

  if (fchmod(new.fd, st.st_mode) < 0) {
    _hash_drv_close(&new);
    _hash_drv_close(&old);
    unlink(newfile);
    return EFAILURE;
  }

  filepos = sizeof(struct _hash_drv_header);
  header = old.addr;
  while(filepos < old.file_len) {
    printf("compressing %lu records in extent %lu\n", header->hash_rec_max, extent);
    extent++;
    for(i=0;i<header->hash_rec_max;i++) {
      rec = (void *)((unsigned long) old.addr + filepos);
      if (rec->hashcode) {
        if (_hash_drv_set_spamrecord(&new, rec, 0)) {
          LOG(LOG_WARNING, "aborting on error");
          _hash_drv_close(&new);
          _hash_drv_close(&old);
          unlink(newfile);
          return EFAILURE;
        }
      }
      filepos += sizeof(struct _hash_drv_spam_record);
    }
    offset = (void *)((unsigned long) old.addr + filepos);
    header = offset;
    filepos += sizeof(struct _hash_drv_header);
  }

  _hash_drv_close(&new);
  _hash_drv_close(&old);
  if (rename(newfile, filename))
    fprintf(stderr, "rename(%s, %s): %s\n", newfile, filename, strerror(errno));
  return 0;
}
