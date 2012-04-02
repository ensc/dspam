/* $Id: config_shared.c,v 1.96 2011/06/28 00:13:48 sbajic Exp $ */

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

/*
 * config_shared.c - attributes-based configuration functionsc
 *
 * DESCRIPTION
 *   Attribtues are used by the agent and libdspam to control configuration
 *   management. The included functions perform various operations on the
 *   configuration structures supplied.
 *
 *   Because these functions are used by libdspam, they are prefixed with _ds_
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "config_shared.h"
#include "error.h"
#include "language.h"
#include "libdspam.h"

attribute_t _ds_find_attribute(config_t config, const char *key) {
  int i;

#ifdef VERBOSE
  LOGDEBUG("find attribute '%s'", key);
#endif

  if (config == NULL) {
#ifdef VERBOSE
    LOGDEBUG("_ds_find_attribute(): NULL config");
#endif
    return NULL;
  }

  for(i=0;config[i];i++) {
    attribute_t attr = config[i];
    if (!strcasecmp(attr->key, key)) {
#ifdef VERBOSE
      LOGDEBUG(" -> found attribute '%s' with value '%s'", attr->key, attr->value);
#endif
      return attr;
    }
  }

  return NULL;
}

int _ds_add_attribute(config_t config, const char *key, const char *val) {
  attribute_t attr;

#ifdef VERBOSE
  LOGDEBUG("add attribute '%s' with value '%s'", key, val);
#endif

  attr = _ds_find_attribute(config, key);
  if (!attr) {
    int i;
    for(i=0;config[i];i++) { }
    config[i+1] = 0;
    config[i] = malloc(sizeof(struct attribute));
    if (!config[i]) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      return EUNKNOWN;
    }
    attr = config[i];
  } else {
    while(attr->next != NULL) {
      attr = attr->next;
    }
    attr->next = malloc(sizeof(struct attribute));
    if (!attr->next) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      return EUNKNOWN;
    }
    attr = attr->next;
  }
  attr->key = strdup(key);
  attr->value = strdup(val);
  attr->next = NULL;

  return 0;
}

int _ds_overwrite_attribute(config_t config, const char *key, const char *val) {
  attribute_t attr;

#ifdef VERBOSE
  LOGDEBUG("overwrite attribute '%s' with value '%s'", key, val);
#endif

  attr = _ds_find_attribute(config, key);
  if (attr == NULL) {
    return _ds_add_attribute(config, key, val);
  }

  free(attr->value);
  attr->value = strdup(val);

  return 0;
}

char *_ds_read_attribute(config_t config, const char *key) {
#ifdef VERBOSE
  LOGDEBUG("read attribute '%s'", key);
#endif
  attribute_t attr = _ds_find_attribute(config, key);

  if (!attr) {
#ifdef VERBOSE
    LOGDEBUG(" -> read: not found attribute '%s'", key);
#endif
    return NULL;
  }

#ifdef VERBOSE
  LOGDEBUG(" -> read attribute '%s' with value '%s'", key, attr->value);
#endif
  return attr->value;
}

int _ds_match_attribute(config_t config, const char *key, const char *val) {
#ifdef VERBOSE
  LOGDEBUG("match attribute '%s' with value '%s'", key, val);
#endif
  attribute_t attr;

  attr = _ds_find_attribute(config, key);
  if (!attr) {
#ifdef VERBOSE
    LOGDEBUG(" -> match: not found attribute '%s'", key);
#endif
    return 0;
  }

  while(strcasecmp(attr->value, val) && attr->next != NULL)
    attr = attr->next;

  if (!strcasecmp(attr->value, val)) {
#ifdef VERBOSE
    LOGDEBUG(" -> matched attribute '%s' with value '%s'", key, val);
#endif
    return 1;
  }

#ifdef VERBOSE
  LOGDEBUG(" -> match: not found attribute '%s' with value '%s'", key, val);
#endif
  return 0;
}

void _ds_destroy_config(config_t config) {
#ifdef VERBOSE
  LOGDEBUG("destroying/freeing configuration");
#endif
  attribute_t x, y;
  int i;

  for(i=0;config[i];i++) {
    x = config[i];

    while(x) {
      y = x->next;
      free(x->key);
      free(x->value);
      free(x);
      x = y;
    }
  }
  free(config);
  return;
}
