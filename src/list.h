/* $Id: list.h,v 1.1 2004/12/29 04:03:21 jonz Exp $ */

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

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifndef _LIST_H
#define _LIST_H

struct list_node
{
  void *ptr;		/* Token name or pointer */
  float value;		/* Token value (probability) */
  int eliminated;	/* Token eliminated == 1 */
  struct list_node *next;
};

struct list
{
  struct list_node *first;
  struct list_node *insert;	/* Next insertion point */
  int items;
  int nodetype;
};

struct list_c
{
  struct list_node *iter_index;
};

struct list_node *	list_insert		(struct list *list, void *data, float v);
struct list_node *	c_list_first	(struct list *list, struct list_c *c);
struct list_node *	c_list_next	(struct list *list, struct list_c *c);
struct list *		list_create	(int node_type);
void			list_destroy	(struct list *list);

#endif /* _LIST_H */

