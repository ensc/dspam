/* $Id: pref.c,v 1.7 2005/01/11 19:41:45 jonz Exp $ */

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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "pref.h"
#include "config.h"
#include "util.h"
#include "language.h"
#include "read_config.h"

AGENT_PREF _ds_pref_aggregate(AGENT_PREF STX, AGENT_PREF UTX) {
  AGENT_PREF PTX = malloc(PREF_MAX*sizeof(AGENT_ATTRIB *));
  int i, j, size = 0;

  if (STX) {
    for(i=0;STX[i];i++) {
      PTX[i] = _ds_pref_new(STX[i]->attribute, STX[i]->value);
      PTX[i+1] = NULL;
      size++;
    }
  }

  if (UTX) {
    for(i=0;UTX[i];i++) {

      if (_ds_match_attribute(agent_config, "AllowOverride", UTX[i]->attribute))
      {
        int found = 0;
        for(j=0;PTX[j];j++) {
          if (!strcasecmp(PTX[j]->attribute, UTX[i]->attribute)) {
            found = 1;
            free(STX[j]->value);
            STX[j]->value = strdup(UTX[i]->value);
            break;
          }
        }
  
        if (!found) {
          PTX[size] = _ds_pref_new(UTX[i]->attribute, UTX[i]->value);
          PTX[size+1] = NULL;
          size++;
        }
      } else {
        report_error_printf(ERROR_IGNORING_PREF, UTX[i]->attribute);
      }
    }
  }

  return PTX;
}

int _ds_pref_free(AGENT_PREF PTX) {
  AGENT_ATTRIB *pref;
  int i;

  if (!PTX)
    return 0;

  for(i=0;PTX[i];i++) {
    pref = PTX[i];

    free(pref->attribute);
    free(pref->value);
    free(pref);
  }

  return 0;
}

const char *_ds_pref_val(
  AGENT_PREF PTX,
  const char *attrib)
{
  AGENT_ATTRIB *pref;
  int i;

  if (PTX == NULL)
    return "";

  for(i=0;PTX[i];i++) {
    pref = PTX[i];
    if (!strcmp(pref->attribute, attrib))
      return pref->value;
  }

  return "";
}

AGENT_ATTRIB *_ds_pref_new(const char *attribute, const char *value) {
  AGENT_ATTRIB *pref;
                                                                                
  pref = malloc(sizeof(AGENT_ATTRIB));
                                                                                
  if (pref == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }
                                                                                
  pref->attribute = strdup(attribute);
  pref->value = strdup(value);

  return pref;
}

#ifndef PREFERENCES_EXTENSION
AGENT_PREF _ds_pref_load(
  attribute_t **config,
  const char *user, 
  const char *home,
  void *ignore)
{
  char filename[MAX_FILENAME_LENGTH];
  AGENT_PREF PTX = malloc(sizeof(AGENT_ATTRIB *)*PREF_MAX);
  AGENT_ATTRIB *pref;
  char buff[258];
  FILE *file;
  char *p, *q, *bufptr;
  int i = 0;

  if (PTX == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }
  PTX[0] = NULL;

  if (user == NULL) {
    snprintf(filename, MAX_FILENAME_LENGTH, "%s/default.prefs", home);
  } else {
    _ds_userdir_path (filename, home, user, "prefs");
  }
  file = fopen(filename, "r");
  if (file == NULL && user != NULL) { 
    free(PTX);
    return _ds_pref_load(config, NULL, home, NULL);
  }
 
  /* Apply default preferences from dspam.conf */
                                                                                
  if (file != NULL) {
    char *ptrptr;
    while(i<(PREF_MAX-1) && fgets(buff, sizeof(buff), file)!=NULL) {
      if (buff[0] == '#' || buff[0] == 0)
        continue;
      chomp(buff);

      bufptr = buff;

      p = strtok_r(buff, "=", &ptrptr);

      if (p == NULL)
        continue;
      q = p + strlen(p)+1;

      LOGDEBUG("Loading preference '%s' = '%s'", p, q);

      pref = malloc(sizeof(AGENT_ATTRIB));
      if (pref == NULL) {
        LOG(LOG_CRIT, ERROR_MEM_ALLOC);
        fclose(file);
        return PTX;
      }

      PTX[i] = _ds_pref_new(p, q);
      PTX[i+1] = NULL;
      i++;
    }
    fclose(file);
  }

  return PTX;
}
#endif
