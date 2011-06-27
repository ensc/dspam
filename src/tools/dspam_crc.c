/* $Id: dspam_crc.c,v 1.6 2010/01/03 14:39:13 sbajic Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2010 DSPAM PROJECT

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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "util.h"

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

#include "config.h"
#include "libdspam.h"

#define SYNTAX "syntax: dspam_crc [token]"

int
main (int argc, char *argv[])
{
  unsigned long long crc;

  if (argc < 2)
  {
    printf ("%s\n", SYNTAX);
    exit (EXIT_FAILURE);
  }

  crc = _ds_getcrc64 (argv[1]);

  printf ("TOKEN: '%s' CRC: %"LLU_FMT_SPEC"\n", argv[1], crc);
  exit (EXIT_SUCCESS);
}
