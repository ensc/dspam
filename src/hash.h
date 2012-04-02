/* $Id: hash.h,v 1.6 2011/06/28 00:13:48 sbajic Exp $ */

/*
  Bayesian Noise Reduction - Jonathan A. Zdziarski
  http://www.zdziarski.com/papers/bnr.html
  COPYRIGHT (C) 2004-2012 DSPAM PROJECT

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

#ifndef _BNR_HASH_H
#define _BNR_HASH_H

enum
{ bnr_hash_num_primes = 28 };

/* bnr_hash root */
struct bnr_hash
{
  unsigned long size;
  unsigned long items;
  struct bnr_hash_node **tbl;
};

/* bnr_hash node */
struct bnr_hash_node
{
  struct bnr_hash_node *next;

  char *name;
  float value;
};

/* bnr_hash cursor */
struct bnr_hash_c
{
  unsigned long iter_index;
  struct bnr_hash_node *iter_next;
};

/* constructor and destructor */
struct bnr_hash *	bnr_hash_create (unsigned long size);
int		bnr_hash_destroy (struct bnr_hash *hash);

int bnr_hash_set	(struct bnr_hash *hash, const char *name, float value);
int bnr_hash_hit	(struct bnr_hash *hash, const char *name);
int bnr_hash_delete	(struct bnr_hash *hash, const char *name);
float bnr_hash_value(struct bnr_hash *hash, const char *name);

struct bnr_hash_node *bnr_hash_node_create (const char *name);
long bnr_hash_hashcode(struct bnr_hash *hash, const char *name);

/* iteration functions */
struct bnr_hash_node *c_bnr_hash_first	(struct bnr_hash *hash, struct bnr_hash_c *c);
struct bnr_hash_node *c_bnr_hash_next	(struct bnr_hash *hash, struct bnr_hash_c *c);

#endif /* _BNR_HASH_H */
