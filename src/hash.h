/* $Id: hash.h,v 1.1 2004/12/29 04:03:21 jonz Exp $ */

/*
  Bayesian Noise Reduction - Contextual Symmetry Logic
  http://bnr.nuclearelephant.com
  Copyright (c) 2004 Jonathan A. Zdziarski

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

#ifndef _HASH_H
#define _HASH_H

enum
{ hash_num_primes = 28 };

/* hash root */
struct hash
{
  unsigned long size;
  unsigned long items;
  struct hash_node **tbl;
};

/* hash node */
struct hash_node
{
  struct hash_node *next;

  char *name;
  float value;
};

/* hash cursor */
struct hash_c
{
  unsigned long iter_index;
  struct hash_node *iter_next;
};

/* constructor and destructor */
struct hash *	hash_create (unsigned long size);
int		hash_destroy (struct hash *hash);

int hash_set	(struct hash *hash, const char *name, float value);
int hash_hit	(struct hash *hash, const char *name);
int hash_delete	(struct hash *hash, const char *name);
float hash_value(struct hash *hash, const char *name);

struct hash_node *hash_node_create (const char *name);
long hash_hashcode(struct hash *hash, const char *name);

/* iteration functions */
struct hash_node *c_hash_first	(struct hash *hash, struct hash_c *c);
struct hash_node *c_hash_next	(struct hash *hash, struct hash_c *c);

#endif /* _HASH_H */
