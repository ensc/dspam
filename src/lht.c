/* $Id: lht.c,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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

#include "libdspam_objects.h"
#include "lht.h"
#include "nodetree.h"

/* Read-only */

static unsigned long lht_prime_list[lht_num_primes] = {
  53ul, 97ul, 193ul, 389ul, 769ul,
  1543ul, 3079ul, 6151ul, 12289ul, 24593ul,
  49157ul, 98317ul, 196613ul, 393241ul, 786433ul,
  1572869ul, 3145739ul, 6291469ul, 12582917ul, 25165843ul,
  50331653ul, 100663319ul, 201326611ul, 402653189ul, 805306457ul,
  1610612741ul, 3221225473ul, 4294967291ul
};

struct lht_node *
lht_node_create (unsigned long long key)
{
  struct lht_node *node;

  node = (struct lht_node *) malloc (sizeof (struct lht_node));
  if (node == NULL)
    return NULL;

  node->key = key;
  node->next = NULL;
  node->frequency = 0;
  node->token_name = NULL;
  memset (&node->s, 0, sizeof (struct _ds_spam_stat));

  return (node);
}

struct lht *
lht_create (unsigned long size)
{
  unsigned long i = 0;
  struct lht *lht = (struct lht *) malloc (sizeof (struct lht));

  if (lht == NULL)
    return NULL;
  while (lht_prime_list[i] < size)
    i++;
  lht->order = nt_create(NT_INDEX);
  lht->chained_order = nt_create(NT_INDEX);
  if (lht->order == NULL || lht->chained_order == NULL) {
    nt_destroy(lht->order);
    nt_destroy(lht->chained_order);
    return NULL;
  }
  lht->size = lht_prime_list[i];
  lht->items = 0;
  lht->whitelist_token = 0;
  lht->tbl =
    (struct lht_node **) calloc (lht->size, sizeof (struct lht_node *));
  if (lht->tbl == NULL)
  {
    nt_destroy(lht->order);
    nt_destroy(lht->chained_order);
    free (lht);
    return NULL;
  }
  return (lht);
}

int
lht_destroy (struct lht *lht)
{
  struct lht_node *node, *next;
  struct lht_c c;

  if (lht == NULL)
    return -1;

  node = c_lht_first (lht, &c);
  while (node != NULL)
  {
    next = c_lht_next (lht, &c);
    free (node->token_name);
    lht_delete (lht, node->key);
    node = next;
  }

  free (lht->tbl);
  nt_destroy(lht->order);
  nt_destroy(lht->chained_order);
  free (lht);

  return 0;
}

int
lht_getfrequency (struct lht *lht, unsigned long long key)
{
  unsigned long hash_code;
  struct lht_node *node;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  node = lht->tbl[hash_code];
  while (node)
  {
    if (key == node->key)
      return node->frequency;
    node = node->next;
  }
  return -1;
}

char *
lht_gettoken (struct lht *lht, unsigned long long key)
{
  unsigned long hash_code;
  struct lht_node *node;

  if (lht == NULL)
    return NULL;

  hash_code = key % lht->size;
  node = lht->tbl[hash_code];
  while (node)
  {
    if (key == node->key)
      return node->token_name;
    node = node->next;
  }
  return NULL;
}

int
lht_getspamstat (struct lht *lht, unsigned long long key,
                 struct _ds_spam_stat *s)
{
  unsigned long hash_code;
  struct lht_node *node;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  node = lht->tbl[hash_code];
  while (node)
  {
    if (key == node->key)
    {
      s->probability = node->s.probability;
      s->spam_hits = node->s.spam_hits;
      s->innocent_hits = node->s.innocent_hits;
      s->status = node->s.status;
      return 0;
    }
    node = node->next;
  }
  return -1;
}


int
lht_delete (struct lht *lht, unsigned long long key)
{
  unsigned long hash_code;
  struct lht_node *node;
  struct lht_node *del_node = NULL;
  struct lht_node *parent = NULL;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  node = lht->tbl[hash_code];

  while (node)
  {
    if (key == node->key)
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
    lht->tbl[hash_code] = del_node->next;

  free (del_node);
  lht->items--;

  return 0;
}

struct lht_node *
c_lht_first (struct lht *lht, struct lht_c *c)
{
  c->iter_index = 0;
  c->iter_next = (struct lht_node *) NULL;
  return (c_lht_next (lht, c));
}

struct lht_node *
c_lht_next (struct lht *lht, struct lht_c *c)
{
  unsigned long index;
  struct lht_node *node = c->iter_next;

  if (lht == NULL)
    return NULL;

  if (node)
  {
    if (node != NULL)
    {
      c->iter_next = node->next;
      return (node);
    }
  }

  if (!node)
  {
    while (c->iter_index < lht->size)
    {
      index = c->iter_index++;
      if (lht->tbl[index])
      {
        c->iter_next = lht->tbl[index]->next;
        return (lht->tbl[index]);
      }
    }
  }
  return ((struct lht_node *) NULL);
}


int
lht_hit (struct lht *lht, unsigned long long key, const char *token_name)
{
  unsigned long hash_code;
  struct lht_node *parent;
  struct lht_node *node;
  struct lht_node *new_node = NULL;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  parent = NULL;
  node = lht->tbl[hash_code];

  while (node != NULL)
  {
    if (key == node->key)
    {
      new_node = node;
      node = NULL;
    }

    if (new_node == NULL)
    {
      parent = node;
      node = node->next;
    }
  }

  if (new_node != NULL)
  {
    new_node->frequency++;
    if (new_node->token_name == NULL && token_name != NULL)
      new_node->token_name = strdup (token_name);
      goto ADD;
  }

  /* Create a new node */
  new_node = lht_node_create (key);
  new_node->frequency++;
  if (new_node->token_name == NULL && token_name != NULL)
    new_node->token_name = strdup (token_name);

  lht->items++;

  /* Insert */
  if (parent)
  {                             /* Existing parent */
    parent->next = new_node;
    goto ADD;
  }

  /* No existing parent; add directly to hash table */
  lht->tbl[hash_code] = new_node;

ADD:
  if (strchr(token_name, '+'))
    nt_add(lht->chained_order, new_node);
  else
    nt_add(lht->order, new_node);
  return 0;
}

int
lht_setfrequency (struct lht *lht, unsigned long long key,
                  int frequency)
{
  unsigned long hash_code;
  struct lht_node *parent;
  struct lht_node *node;
  struct lht_node *new_node = NULL;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  parent = NULL;
  node = lht->tbl[hash_code];

  while (node != NULL)
  {
    if (key == node->key)
    {
      new_node = node;
      node = NULL;
    }

    if (new_node == NULL)
    {
      parent = node;
      node = node->next;
    }
  }

  if (new_node != NULL)
  {
    new_node->frequency = frequency;
    return 0;
  }

  /* Create a new node */
  new_node = lht_node_create (key);
  new_node->frequency = frequency;

  lht->items++;

  /* Insert */
  if (parent)
  {                             /* Existing parent */
    parent->next = new_node;
    return 0;
  }

  /* No existing parent; add directly to hash table */
  lht->tbl[hash_code] = new_node;
  return 0;
}

int
lht_settoken (struct lht *lht, unsigned long long key, const char *token_name)
{
  unsigned long hash_code;
  struct lht_node *parent;
  struct lht_node *node;
  struct lht_node *new_node = NULL;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  parent = NULL;
  node = lht->tbl[hash_code];

  while (node != NULL)
  {
    if (key == node->key)
    {
      new_node = node;
      node = NULL;
    }

    if (new_node == NULL)
    {
      parent = node;
      node = node->next;
    }
  }

  if (new_node != NULL)
  {
    if (new_node->token_name != NULL)
      free (new_node->token_name);
    if (token_name != NULL)
      new_node->token_name = strdup (token_name);
    else
      new_node->token_name = NULL;
    return 0;
  }

  /* Create a new node */
  new_node = lht_node_create (key);
  if (new_node->token_name != NULL)
    free (new_node->token_name);
  if (token_name != NULL)
    new_node->token_name = strdup (token_name);
  else
    new_node->token_name = NULL;

  lht->items++;

  /* Insert */
  if (parent)
  {                             /* Existing parent */
    parent->next = new_node;
    return 0;
  }

  /* No existing parent; add directly to hash table */
  lht->tbl[hash_code] = new_node;
  return 0;
}

int
lht_setspamstat (struct lht *lht, unsigned long long key,
                 struct _ds_spam_stat *s)
{
  unsigned long hash_code;
  struct lht_node *parent;
  struct lht_node *node;
  struct lht_node *new_node = NULL;

  if (lht == NULL)
    return -1;

  hash_code = key % lht->size;
  parent = NULL;
  node = lht->tbl[hash_code];

  while (node != NULL)
  {
    if (key == node->key)
    {
      new_node = node;
      node = NULL;
    }

    if (new_node == NULL)
    {
      parent = node;
      node = node->next;
    }
  }

  if (new_node != NULL)
  {
    new_node->s.probability = s->probability;
    new_node->s.spam_hits = s->spam_hits;
    new_node->s.innocent_hits = s->innocent_hits;
    new_node->s.status = s->status;
    return 0;
  }

  /* Create a new node */
  new_node = lht_node_create (key);
  new_node->s.probability = s->probability;
  new_node->s.spam_hits = s->spam_hits;
  new_node->s.innocent_hits = s->innocent_hits;
  new_node->s.status = s->status;

  lht->items++;

  /* Insert */
  if (parent)
  {                             /* Existing parent */
    parent->next = new_node;
    return 0;
  }

  /* No existing parent; add directly to hash table */
  lht->tbl[hash_code] = new_node;
  return 0;
}

int
lht_addspamstat (struct lht *lht, unsigned long long key,
                 struct _ds_spam_stat *s)
{
  unsigned long hash_code;
  struct lht_node *parent;
  struct lht_node *node;
  struct lht_node *new_node = NULL;
                                                                                
  if (lht == NULL)
    return -1;
                                                                                
  hash_code = key % lht->size;
  parent = NULL;
  node = lht->tbl[hash_code];
                                                                                
  while (node != NULL)
  {
    if (key == node->key)
    {
      new_node = node;
      node = NULL;
    }
                                                                                
    if (new_node == NULL)
    {
      parent = node;
      node = node->next;
    }
  }
                                                                                
  if (new_node != NULL)
  {
    new_node->s.probability += s->probability;
    new_node->s.spam_hits += s->spam_hits;
    new_node->s.innocent_hits += s->innocent_hits;
    if (s->status & TST_DISK)
      new_node->s.status |= TST_DISK;
    if (s->status & TST_DIRTY)
      new_node->s.status |= TST_DIRTY;
    return 0;
  }

  /* Create a new node */
  new_node = lht_node_create (key);
  new_node->s.probability = s->probability;
  new_node->s.spam_hits = s->spam_hits;
  new_node->s.innocent_hits = s->innocent_hits;
  new_node->s.status = s->status;

  lht->items++;
                                                                                
  /* Insert */
  if (parent)
  {                             /* Existing parent */
    parent->next = new_node;
    return 0;
  }
                                                                                
  /* No existing parent; add directly to hash table */
  lht->tbl[hash_code] = new_node;
  return 0;
}

