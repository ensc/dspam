/* $Id: bnr.h,v 1.14 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef __BNR_H
#define	__BNR_H

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

#include "list.h"
#include "hash.h"

typedef struct {
  long eliminations;
  struct bnr_list *stream;
  struct bnr_hash *patterns;
  char identifier;

  struct bnr_list_c c_stream;
  struct bnr_hash_c c_pattern;
  int stream_iter;
  int pattern_iter;
  int window_size;
  float ex_radius;
  float in_radius;
} BNR_CTX;

BNR_CTX *bnr_init(int type, char identifier);
int bnr_destroy(BNR_CTX *BTX);

int bnr_add		(BNR_CTX *BTX, void *token, float value);
int bnr_instantiate	(BNR_CTX *BTX);
int bnr_set_pattern	(BNR_CTX *BTX, const char *name, float value);
int bnr_finalize	(BNR_CTX *BTX);

void * bnr_get_token	(BNR_CTX *BTX, int *eliminated);
char * bnr_get_pattern	(BNR_CTX *BTX);

float _bnr_round(float n);

/*
 BTX_CHAR       Character-Based Context
                Treats the passed token identifier as a const char * and
		creates new storage space for each string (strdup)

 BTX_INDEX      Pointer-Based Context
                Treats the passed token identifier as a void * to your own
		token structure, whose pointers are used only for indexing
*/

#define BNR_CHAR	0x00
#define BNR_INDEX	0x01

#ifndef EFAILURE
#define EFAILURE	-0x01
#endif
#endif
