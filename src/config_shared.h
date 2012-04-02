/* $Id: config_shared.h,v 1.9 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _CONFIG_SHARED_H
#define _CONFIG_SHARED_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

typedef struct attribute {
  char *key;
  char *value;
  struct attribute *next;
} *attribute_t;

typedef attribute_t *config_t;

struct attribute *_ds_find_attribute(config_t config, const char *key);
char *_ds_read_attribute(config_t config, const char *key);
int  _ds_add_attribute(config_t config, const char *key, const char *val);
int  _ds_overwrite_attribute(config_t config, const char *key, const char *val);
int  _ds_match_attribute(config_t config, const char *key, const char *val);
void _ds_destroy_config(config_t config);

#endif /* _CONFIG_SHARED_H */
