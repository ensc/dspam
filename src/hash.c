/* $Id: hash.c,v 1.1 2004/12/29 04:03:21 jonz Exp $ */

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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hash.h"

/* Read-only */

static unsigned long hash_prime_list[hash_num_primes] = {
  53ul, 97ul, 193ul, 389ul, 769ul,
  1543ul, 3079ul, 6151ul, 12289ul, 24593ul,
  49157ul, 98317ul, 196613ul, 393241ul, 786433ul,
  1572869ul, 3145739ul, 6291469ul, 12582917ul, 25165843ul,
  50331653ul, 100663319ul, 201326611ul, 402653189ul, 805306457ul,
  1610612741ul, 3221225473ul, 4294967291ul
};

struct hash_node *
hash_node_create (const char *name)
{
  struct hash_node *node
    = (struct hash_node *) calloc (1, sizeof (struct hash_node));

  if (node)
    node->name = strdup(name);

  return node;
}

struct hash *
hash_create (unsigned long size)
{
  unsigned long i = 0;
  struct hash *hash = (struct hash *) malloc (sizeof (struct hash));

  if (hash == NULL) 
    return NULL;
  while (hash_prime_list[i] < size)
    i++;
  hash->size = hash_prime_list[i];
  hash->items = 0;
  hash->tbl =
    (struct hash_node **) calloc (hash->size, sizeof (struct hash_node *));
  if (hash->tbl == NULL) {
    free (hash);
    return NULL;
  }
  return (hash);
}

int
hash_destroy (struct hash *hash)
{
  struct hash_node *node, *next;
  struct hash_c c;

  if (hash == NULL)
    return -1;

  node = c_hash_first (hash, &c);
  while (node != NULL)
  {
    next = c_hash_next (hash, &c);
    free (node->name);
    hash_delete (hash, node->name);
    node = next;
  }

  free (hash->tbl);
  free (hash);

  return 0;
}

int
hash_delete (struct hash *hash, const char *name)
{
  unsigned long hash_code;
  struct hash_node *node;
  struct hash_node *del_node = NULL;
  struct hash_node *parent = NULL;

  hash_code = hash_hashcode(hash, name);
  node = hash->tbl[hash_code];

  while (node)
  {
    if (!strcmp(name, node->name))
    {
      del_node = node;
      node = NULL;
    }
    else
    {
      parent = node;
      node = node->next;
    }
  }

  if (del_node == NULL)
    return -2;

  if (parent)
    parent->next = del_node->next;
  else
    hash->tbl[hash_code] = del_node->next;

  free (del_node);
  hash->items--;

  return 0;
}

struct hash_node *
c_hash_first (struct hash *hash, struct hash_c *c)
{
  c->iter_index = 0;
  c->iter_next = (struct hash_node *) NULL;
  return (c_hash_next (hash, c));
}

struct hash_node *
c_hash_next (struct hash *hash, struct hash_c *c)
{
  unsigned long index;
  struct hash_node *node = c->iter_next;

  if (node) {
    c->iter_next = node->next;
    return (node);
  }

  while (c->iter_index < hash->size)
  {
    index = c->iter_index++;
    if (hash->tbl[index]) {
      c->iter_next = hash->tbl[index]->next;
      return (hash->tbl[index]);
    }
  }
  return NULL;
}

int
hash_hit (struct hash *hash, const char *name)
{
  unsigned long hash_code;
  struct hash_node *parent;
  struct hash_node *node;
  struct hash_node *new_node = NULL;

  hash_code = hash_hashcode(hash, name);
  parent = NULL;
  node = hash->tbl[hash_code];

  while (node != NULL)
  {
    if (!strcmp(name, node->name)) {
      new_node = node;
      node = NULL;
    } else {
      parent = node;
      node = node->next;
    }
  }

  if (new_node != NULL) 
    return 0;

  /* Create a new node */
  new_node = hash_node_create(name);
  hash->items++;

  /* Insert */
  if (parent) {
    parent->next = new_node;
    return 0;
  }

  /* No existing parent; add directly to hash table */
  hash->tbl[hash_code] = new_node;

  return 0;
}

float
hash_value (struct hash *hash, const char *name)
{
  unsigned long hash_code;
  struct hash_node *parent;
  struct hash_node *node;

  hash_code = hash_hashcode(hash, name);
  parent = NULL;
  node = hash->tbl[hash_code];

  while (node != NULL)
  {
    if (!strcmp(node->name, name))
      return node->value;

    parent = node;
    node = node->next;
  }

  return 0.00000;
}

int
hash_set (struct hash *hash, const char *name, float value)
{
  unsigned long hash_code;
  struct hash_node *parent;
  struct hash_node *node;

  if (!name)
    return 0;

  hash_code = hash_hashcode(hash, name);
  parent = NULL;
  node = hash->tbl[hash_code];

  while (node != NULL)
  {
    if (!strcmp(node->name, name)) {
      node->value = value;
      return 0;
    }

    parent = node;
    node = node->next;
  }

  return 0;
}

long hash_hashcode(struct hash *hash, const char *name) {
  unsigned long val = 0;

  if (!name)
    return 0;
  for (; *name; ++name) val = 5 * val + *name;
  return(val % hash->size);
}

