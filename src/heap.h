/* $Id: heap.h,v 1.2 2004/12/17 23:39:42 jonz Exp $ */

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
 *  Heap - Heap-Based Sorting Algorithm with Maximum Window Size
 *
 *  This sorting algorithm is designed to perform well when there is a small
 *  window-size of 'peak' values, such as the 15 bayes slots open.
 */

#ifndef _HEAP_H
#define _HEAP_H

struct heap
{
  unsigned int items;
  unsigned int size;
  struct heap_node *root;
};

struct heap_node
{
  double delta;
  float probability;
  unsigned long long token;
  unsigned long frequency;
  int complexity;
  struct heap_node *next;
};

struct heap *	heap_create	(int size);
int		heap_destroy	(struct heap *);

struct heap_node *heap_node_create (double probability,
 unsigned long long token, unsigned long frequency, int complexity);

int heap_insert (struct heap *heap, double probability,
 unsigned long long token, unsigned long frequency, int complexity);

#endif /* _TBT_H */
