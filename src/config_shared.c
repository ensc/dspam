/* $Id: config_shared.c,v 1.2 2004/12/24 02:19:06 jonz Exp $ */

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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "config_shared.h"
#include "error.h"
#include "language.h"
#include "string.h"
#include "libdspam.h"

attribute_t *_ds_find_attribute(attribute_t **attrib, const char *key) {
  int i;

  for(i=0;attrib[i];i++) {
    attribute_t *x = attrib[i];
    if (!strcasecmp(x->key, key)) {
      return x;
    }
  } 

  return NULL;
}

int _ds_add_attribute(attribute_t **attrib, const char *key, const char *val) {
  attribute_t *x;
  int i;

#ifdef VERBOSE
  LOGDEBUG("Attribute %s = %s", key, val);
#endif

  x = _ds_find_attribute(attrib, key);
  if (x == NULL) {
    for(i=0;attrib[i];i++) { }
    attrib[i+1] = 0;
    attrib[i] = malloc(sizeof(attribute_t));
    if (!attrib[i]) {
      report_error(ERROR_MEM_ALLOC);
      return EUNKNOWN;
    }
    x = attrib[i];
  } else {
    while(x->next != NULL) {
      x = x->next;
    }
    x->next = malloc(sizeof(attribute_t));
    if (!x->next) {
      report_error(ERROR_MEM_ALLOC);
      return EUNKNOWN;
    }
    x = x->next;
  }
  x->key = strdup(key);
  x->value = strdup(val);
  x->next = NULL;

  return 0;
}

int _ds_overwrite_attribute(attribute_t **attrib, const char *key, const char *val) {
  attribute_t *x;
  int i;
                                                                                
#ifdef VERBOSE
  LOGDEBUG("Overwriting Attribute %s with '%s'", key, val);
#endif
                                                                                
  x = _ds_find_attribute(attrib, key);
  if (x == NULL) {
    for(i=0;attrib[i];i++) { }
    attrib[i+1] = 0;
    attrib[i] = malloc(sizeof(attribute_t));
    if (!attrib[i]) {
      report_error(ERROR_MEM_ALLOC);
      return EUNKNOWN;
    }
    x = attrib[i];
    x->key = strdup(key);
    x->value = strdup(val);
    x->next = NULL;
  } else {
    free(x->value);
    x->value = strdup(val);
  }
                                                                                
  return 0;
}

char *_ds_read_attribute(attribute_t **attrib, const char *key) {
  int i;
                                                                                
  if (attrib == NULL)
    return NULL;

  for(i=0;attrib[i];i++) {
    attribute_t *x = attrib[i];
    if (!strcasecmp(x->key, key)) {
      return x->value;
    }
  }
                                                                                
  return NULL;
}

void _ds_destroy_attributes(attribute_t **attrib) {
  attribute_t *x, *y;
  int i;

  for(i=0;attrib[i];i++) {
    x = attrib[i];

    while(x) {
      y = x->next;
      free(x->key);
      free(x->value);
      free(x);
      x = y;
    }
  } 
  free(attrib);
  return;
}

int _ds_match_attribute(attribute_t **attrib, const char *key, const char *val) {
  attribute_t *x;
                                                                                
  x = _ds_find_attribute(attrib, key);
  if (!x) 
    return 0;

  while(strcasecmp(x->value, val) && x->next != NULL) 
    x = x->next;

  if (!strcasecmp(x->value, val))
    return 1;

  return 0;
}

