/* $Id: config_shared.h,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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

#ifndef _CONFIG_SHARED_H
#define _CONFIG_SHARED_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

typedef struct attribute {
  char *key;
  char *value;
  struct attribute *next;
} attribute_t;

struct attribute *_ds_find_attribute(struct attribute **attrib, const char *key);
char       *_ds_read_attribute(struct attribute **attrib, const char *key);
void       _ds_destroy_attributes(attribute_t **attrib);

int _ds_add_attribute(struct attribute **attrib, const char *key, const char *val);
int _ds_overwrite_attribute(struct attribute **attrib, const char *key, const char *val);
int _ds_match_attribute(struct attribute **attrib, const char *key, const char *val);

#endif /* _CONFIG_SHARED_H */
