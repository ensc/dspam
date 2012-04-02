/* $Id: list.h,v 1.10 2011/06/28 00:13:48 sbajic Exp $ */

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

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifndef _LIST_H
#define _LIST_H

struct bnr_list_node
{
  void *ptr;		/* Token name or pointer */
  float value;		/* Token value (probability) */
  int eliminated;	/* Token eliminated == 1 */
  struct bnr_list_node *next;
};

struct bnr_list
{
  struct bnr_list_node *first;
  struct bnr_list_node *insert;	/* Next insertion point */
  int items;
  int nodetype;
};

struct bnr_list_c
{
  struct bnr_list_node *iter_index;
};

struct bnr_list_node *bnr_list_insert(
  struct bnr_list *list, 
  void *data, float v);
struct bnr_list_node *c_bnr_list_first(
  struct bnr_list *list, 
  struct bnr_list_c *c);
struct bnr_list_node *c_bnr_list_next(
  struct bnr_list *list, 
  struct bnr_list_c *c);
struct bnr_list *	bnr_list_create	(int node_type);
void			bnr_list_destroy(struct bnr_list *list);

#endif /* _LIST_H */

