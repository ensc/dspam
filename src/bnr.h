/* $Id: bnr.h,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "libdspam_objects.h"
#include "libdspam.h"
#include "nodetree.h"
#include "config.h"
#include "util.h"
#include "lht.h"

/*
   Bayesian Noise Reduction - Progressive Noise Logic
   http://www.nuclearelephant.com/projects/dspam/bnr.html 
*/

typedef struct {
  long total_eliminations;
  long total_clean;
  struct nt *stream;
} BNR_CTX;

int bnr_filter_process(DSPAM_CTX *CTX, BNR_CTX *BTX);

