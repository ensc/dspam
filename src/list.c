/* $Id: list.c,v 1.76 2011/06/28 00:13:48 sbajic Exp $ */

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

#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>

#include "list.h"
#include "bnr.h"

/* list_node_create (used internally) to allocate space for a new node */

static struct bnr_list_node *
bnr_list_node_create (void *data)
{
  struct bnr_list_node *node;
  if ((node = (struct bnr_list_node *) malloc (sizeof (struct bnr_list_node))) == NULL)
  {
    perror("list_node_create: memory allocation error");
    return NULL;
  }
  node->ptr = data;
  node->next = (struct bnr_list_node *) NULL;
  return (node);
}

/* list_create allocates space for and initializes a nodetree */

struct bnr_list *
bnr_list_create (int nodetype)
{
  struct bnr_list *list = (struct bnr_list *) malloc (sizeof (struct bnr_list));
  if (list == NULL)
  {
    perror("bnr_list_create: memory allocation error");
    return NULL;
  }
  list->first = (struct bnr_list_node *) NULL;
  list->insert = (struct bnr_list_node *) NULL;
  list->items = 0;
  list->nodetype = nodetype;
  return (list);
}

/* list_destroy methodically destroys a nodetree, freeing resources */

void
bnr_list_destroy (struct bnr_list *list)
{
  struct bnr_list_node *cur, *next;
  int i;
  if (list == NULL)
  {
    return;
  }
  cur = list->first;
  for (i = 0; i < list->items; i++)
  {
    next = cur->next;
    if (list->nodetype != BNR_INDEX)
      free (cur->ptr);
    free (cur);
    cur = next;
  }
  free (list);
  /* list = (struct bnr_list *) NULL; */
}

/* list_insert adds an item to the nodetree */

struct bnr_list_node *
bnr_list_insert (struct bnr_list *list, void *data, float value)
{
  struct bnr_list_node *prev;
  struct bnr_list_c c;
  struct bnr_list_node *node = c_bnr_list_first (list, &c);
  void *vptr;

  if (list->insert) {
    prev = list->insert;
  } else {
    prev = 0;
    while (node)
    {
      prev = node;
      node = node->next;
    }
  }
  list->items++;

  if (list->nodetype == BNR_CHAR)
  {
    long size = strlen ((char *) data) + 1;
    vptr = malloc (size);
    if (vptr == NULL)
    {
      perror("bnr_list_insert: memory allocation error");
      return NULL;
    }
    strcpy (vptr, data);
  }
  else
  {
    vptr = data;
  }

  if (prev)
  {
    node = bnr_list_node_create (vptr);
    if (node == NULL)
      return NULL;
    node->value = value;
    node->eliminated = 0;
    prev->next = node;
    list->insert = node;
    return (node);
  }
  else
  {
    node = bnr_list_node_create (vptr);
    if (node == NULL)
      return NULL;
    node->value = value;
    node->eliminated = 0;
    list->first = node;
    list->insert = node;
    return (node);
  }
}

/* c_bnr_list_next returns the next item in a nodetree */

struct bnr_list_node *
c_bnr_list_next (struct bnr_list *list, struct bnr_list_c *c)
{
  struct bnr_list_node *node = c->iter_index;
  if (node)
  {
    c->iter_index = node->next;
    return (node->next);
  }
  else
  {
    if (list->items > 0)
    {
      c->iter_index = list->first;
      return list->first;
    }
  }

  return ((struct bnr_list_node *) NULL);
}

/* list_first returns the first item in a nodetree */

struct bnr_list_node *
c_bnr_list_first (struct bnr_list *list, struct bnr_list_c *c)
{
  c->iter_index = list->first;
  return (list->first);
}
