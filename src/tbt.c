/* $Id: tbt.c,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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
#include <math.h>
#include "config.h"
#include "nodetree.h"
#include "tbt.h"

static struct tbt_node *
tbt_node_create (double probability, unsigned long long token, int type)
{
  struct tbt_node *node;

  node = (struct tbt_node *) malloc (sizeof (struct tbt_node));
  if (node == NULL)
    return NULL;

  node->probability = probability;
  if (!type)
    node->delta = fabs (.5 - probability);
  else
    node->delta = probability;
  node->token = token;
  node->left = NULL;
  node->right = NULL;
  node->parent = NULL;
  node->frequency = 0;
  node->complexity = 1;

  return (node);
}

struct tbt *
tbt_create (void)
{
  struct tbt *tbt = (struct tbt *) malloc (sizeof (struct tbt));

  if (tbt == NULL)
    return NULL;
  tbt->items = 0;
  tbt->root = NULL;
  tbt->type = 0; /* delta based */
  return (tbt);
}

/* find largest delta in tree */
struct tbt_node *
tbt_first (struct tbt *t)
{
  struct tbt_node *p;
  if (!t || !t->root)
    return NULL;
  p = t->root;
  while (p->left)
    p = p->left;
  return p;
}

/* find next smallest delta in tree */
struct tbt_node *
tbt_next (struct tbt_node *cur)
{
  struct tbt_node *p;

  if (cur == NULL)
    return NULL;

  p = cur->right;
  if (p)
  {
    while (p->left)
      p = p->left;
    return p;
  }
  while ((p = cur->parent))
  {
    if (p->left == cur)
    {
      return p;
    }
    cur = p;
  }
  return NULL;
}

/* traverse tree in postorder to delete. */
int
tbt_destroy (struct tbt *tbt)
{
  struct tbt_node *p, *q;
  int cnt;
  if (tbt == NULL)
    return 0;
  cnt = tbt->items;
  q = tbt_first (tbt);
  while (q)
  {
    /* find leftmost leaf node */
    while (q->right)
    {
      q = q->right;
      while (q->left)
        q = q->left;
    }
    /* left and right have been deleted, so safe to delete. */
    do
    {
      p = q;
      q = p->parent;
#ifdef DEBUG
      memset (p, 0x55, sizeof *p);      /* detect algorithm failure */
#endif
      free (p);
      --cnt;
    }
    while (q && q->right == p);
  }
  free (tbt);
  return cnt;
}

int
tbt_add (struct tbt *tbt, double probability, unsigned long long token,
         unsigned long frequency, int complexity)
{
  struct tbt_node *node;
  struct tbt_node *search, *parent;
  float delta;
  int d = 0, left = 1, right = -1;

  if (tbt == NULL)
    return -1;

  node = tbt_node_create (probability, token, tbt->type);
  if (node == NULL)
    return -1;

  node->frequency = frequency;
  node->complexity = complexity;

  if (tbt->root == NULL)
  {
    tbt->root = node;
    tbt->items++;
    return 0;
  }

  if (! tbt->type)
    delta = fabs (.5 - probability);
  else
    delta = probability; /* value based */

  parent = tbt->root;
  search = parent;

  if ((float) delta > (float) search->delta)
  {
    d = left;
    search = parent->left;
  }
  else if ((float) delta < (float) search->delta)
  {
    d = right;
    search = parent->right;
  }
  else
  {
   if (frequency > search->frequency) {
      d = left;
      search = parent->left;
    } else if (frequency < search->frequency) {
      d = right;
      search = parent->right;
    } else if (complexity > search->complexity) {
      d = left;
      search = parent->left;
    } else {
      d = right;
      search = parent->right;
    }
  }

  while (search != NULL)
  {
    parent = search;
    if ((float) delta > (float) search->delta)
    {
      search = parent->left;
      d = left;
    }
    else if ((float) delta < (float) search->delta)
    {
      d = right;
      search = parent->right;
    }
    else
    {
      if (frequency > search->frequency) {
        d = left;
        search = parent->left;
      } else if (frequency < search->frequency) {
        d = right;
        search = parent->right;
      } else if (complexity > search->complexity) {
        d = left;
        search = parent->left;
      } else {
        d = right;
        search = parent->right;
      }
    }
  }

  node->parent = parent;
  if (d == left)
    parent->left = node;
  else
    parent->right = node;

  tbt->items++;
  return 0;
}
