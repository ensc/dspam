/* $Id: cssstat.c,v 1.2 2005/09/24 01:18:58 jonz Exp $ */

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
 * cssstat.c - Print css file information
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
 
#define SYNTAX "syntax: cssstat [filename]"

int cssstat(const char *filename);

int main(int argc, char *argv[]) {
  char *filename;
  int r;

  if (argc<2) {
    fprintf(stderr, "%s\n", SYNTAX);
    exit(EXIT_FAILURE);
  }
   
  filename = argv[1];

  r = cssstat(filename);
  
  if (r) {
    fprintf(stderr, "cssstat failed on error %d\n", r);
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}

int cssstat(const char *filename) {
  struct _hash_drv_map map;
  hash_drv_spam_record_t rec;
  unsigned long filepos;
  unsigned long nfree = 0;
  unsigned long nused = 0;

  printf("filename %s\n", filename);
  if (_hash_drv_open(filename, &map, 0)) 
    return EFAILURE;

  printf("file length %ld\n", (long) map.file_len);
  printf("record max %lu\n", (unsigned long) map.header->hash_rec_max);

  filepos = sizeof(struct _hash_drv_header);
  while(filepos < map.file_len) {
    rec = map.addr+filepos;
    if (rec->hashcode) 
      nused++;
    else
      nfree++;
    filepos += sizeof(struct _hash_drv_spam_record);
  }

  _hash_drv_close(&map);

  printf("records used %lu\n", nused);
  printf("records free %lu\n", nfree);
  return 0;
}

