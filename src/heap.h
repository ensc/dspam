/* $Id: heap.h,v 1.10 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _HEAP_H
#define _HEAP_H

#define HP_DELTA 0x00
#define HP_VALUE 0x01

typedef struct _ds_heap
{
  unsigned int items;
  unsigned int size;
  char type;
  struct _ds_heap_element *root;
} *ds_heap_t;

typedef struct _ds_heap_element
{
  double delta;
  float probability;
  unsigned long long token;
  unsigned long frequency;
  int complexity;
  struct _ds_heap_element *next;
} *ds_heap_element_t;

ds_heap_t	ds_heap_create	(int size, int type);
void		ds_heap_destroy	(ds_heap_t);

ds_heap_element_t ds_heap_element_create (double probability,
 unsigned long long token, unsigned long frequency, int complexity);

ds_heap_element_t ds_heap_insert (ds_heap_t h, double probability,
 unsigned long long token, unsigned long frequency, int complexity);

#endif /* _HEAP_H */
