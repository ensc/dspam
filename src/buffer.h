/* $Id: buffer.h,v 1.9 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _BUFFER_H
#define _BUFFER_H

typedef struct
{
  long	size;
  long	used;
  char *data;
} buffer;

buffer *buffer_create	(const char *);
buffer *buffer_ncreate	(const char *, long plen);
void	buffer_destroy	(buffer *);

int buffer_copy		(buffer *, const char *);
int buffer_ncopy	(buffer *, const char *, long plen);
int buffer_cat		(buffer *, const char *);
int buffer_ncat		(buffer *, const char *, long plen);
int buffer_clear	(buffer *);

#endif /* _BUFFER_H */
