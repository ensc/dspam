/* $Id: lht.h,v 1.2 2004/12/18 00:21:17 jonz Exp $ */

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

/*
  long hashed token structure

  The lht structure is a way of calculating frequency of tokens in a message,
  and is used to organize the data points prior to processing.
*/

#ifndef _LHT_H
#define _LHT_H

#include "libdspam_objects.h"

enum
{ lht_num_primes = 28 };

/* lht root */
struct lht
{
  unsigned long size;
  unsigned long items;
  unsigned long long whitelist_token;
  struct lht_node **tbl;
  struct nt *order;
  struct nt *chained_order;
};

/* lht node (a single long hashed token) */
struct lht_node
{
  unsigned long long key;
  struct lht_node *next;

  /* Token-specific information */
  int frequency;
  struct _ds_spam_stat s;
  char *token_name;
};

/* lht cursor */
struct lht_c
{
  unsigned long iter_index;
  struct lht_node *iter_next;
};

/* constructor and destructor */
struct lht *	lht_create (unsigned long size);
int		lht_destroy (struct lht *lht);

/* read-only functions */
int	lht_getfrequency(struct lht *lht, unsigned long long key);
char *	lht_gettoken	(struct lht *lht, unsigned long long key);
int 	lht_getspamstat	(struct lht *lht, unsigned long long key,
			 struct _ds_spam_stat *s);

/* read-write functions */
int lht_setfrequency	(struct lht *lht, unsigned long long key, 
			 int frequency);

int lht_settoken	(struct lht *lht, unsigned long long key,
			 const char *token_name);

int lht_setspamstat	(struct lht *lht, unsigned long long key, 
			 struct _ds_spam_stat *s);

int lht_addspamstat     (struct lht *lht, unsigned long long key,
                         struct _ds_spam_stat *s);

int lht_hit		(struct lht *lht, unsigned long long key, 
			 const char *token_name, int chained);

int lht_delete		(struct lht *lht, unsigned long long key);

struct lht_node *lht_node_create (unsigned long long key);

/* iteration functions */
struct lht_node *c_lht_first	(struct lht *lht, struct lht_c *c);
struct lht_node *c_lht_next	(struct lht *lht, struct lht_c *c);

#endif /* _LHT_H */
