/* $Id: heap.c,v 1.4 2004/12/19 03:29:21 jonz Exp $ */

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

#include <stdlib.h>
#include <math.h>
#include "heap.h"

/*
 *  Heap - Heap-Based Sorting Algorithm with Maximum Window Size
 *
 *  This sorting algorithm is designed to perform well when there is a small
 *  window-size of 'peak' values, such as the 15 bayes slots open.
 */

struct heap *
heap_create(int size)
{
  struct heap *h;

  h = calloc(1, sizeof(struct heap));
  h->size = size;
  return h;
}

int
heap_destroy(struct heap *h)
{
  struct heap_node *node, *next;

  if (h == NULL)
    return 0;

  node = h->root;
  while(node) {
    next = node->next;
    free(node);
    node = next;
  }
  free(h);
  return 0;
}

struct heap_node *
heap_node_create (double probability, 
                  unsigned long long token, 
                  unsigned long frequency,
                  int complexity)
{
  struct heap_node *node = calloc(1, sizeof(struct heap_node));

  if (node == NULL)
    return NULL;

  node->delta       = fabs(0.5-probability);
  node->probability = probability;
  node->token       = token;
  node->frequency   = frequency;
  node->complexity  = complexity;

  return node;
}

int
heap_insert (struct heap *h,
             double probability,
             unsigned long long token,
             unsigned long frequency,
             int complexity)
{
  struct heap_node *current = NULL, *insert = NULL, *node;
  float delta = fabs(0.5-probability);

  current = h->root;

  /* Determine if and where we should insert this item */
  while(current) {
    if (delta > current->delta) 
      insert = current;
    else if (delta == current->delta) {
      if (frequency > current->frequency)
        insert = current;
      else if (frequency == current->frequency)
        if (complexity >= current->complexity)
          insert = current;
    }
    if (!insert)
      break;
    else
      current = current->next;
  }

  if (insert != NULL) {

    /* Insert item, throw out new least significant item if necessary */
    node = heap_node_create(probability, token, frequency, complexity);
    node->next = insert->next;
    insert->next = node;
    h->items++;
    if (h->items > h->size) {
      node = h->root;
      h->root = node->next;
      free(node);
      h->items--;
    }
  } else {
    
    /* Item is least significant; throw it out or grow the heap */
    if (h->items == h->size)
      return -1;

    /* Grow heap */
    node = heap_node_create(probability, token, frequency, complexity);
    node->next = h->root;
    h->root = node;
    h->items++;
  }

  return 0;
}

