/* $Id: pref.c,v 1.12 2005/01/18 15:06:08 jonz Exp $ */

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

agent_pref_t _ds_pref_aggregate(agent_pref_t STX, agent_pref_t UTX) {
  agent_pref_t PTX = malloc(PREF_MAX*sizeof(agent_attrib_t ));
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
            free(PTX[j]->value);
            PTX[j]->value = strdup(UTX[i]->value);
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

int _ds_pref_free(agent_pref_t PTX) {
  agent_attrib_t pref;
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
  agent_pref_t PTX,
  const char *attrib)
{
  agent_attrib_t pref;
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

agent_attrib_t _ds_pref_new(const char *attribute, const char *value) {
  agent_attrib_t pref;
                                                                                
  pref = malloc(sizeof(struct _ds_agent_attribute));
                                                                                
  if (pref == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }
                                                                                
  pref->attribute = strdup(attribute);
  pref->value = strdup(value);

  return pref;
}

#ifndef PREFERENCES_EXTENSION
agent_pref_t _ds_pref_load(
  attribute_t **config,
  const char *user, 
  const char *home,
  void *ignore)
{
  char filename[MAX_FILENAME_LENGTH];
  agent_pref_t PTX = malloc(sizeof(agent_attrib_t )*PREF_MAX);
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

      PTX[i] = _ds_pref_new(p, q);
      PTX[i+1] = NULL;
      i++;
    }
    fclose(file);
  }

  return PTX;
}

static int _ds_pref_process_file (
  const char *username,
  const char *home,
  const char *preference,
  char *filename,
  char *out_filename,
  FILE **out_file
)
{
  char line[1024];
  int plen = strlen(preference);
  FILE *in_file;

  if (username == NULL) {
    snprintf(filename, MAX_FILENAME_LENGTH, "%s/default.prefs", home);
  } else {
    _ds_userdir_path (filename, home, username, "prefs");
  }

  snprintf(out_filename, MAX_FILENAME_LENGTH, "%s.bak", filename);

  in_file = fopen(filename, "r");
  *out_file = fopen(out_filename, "w");

  if (in_file == NULL) {
    file_error("open", filename, strerror(errno));
    return -1;
  }
  if (*out_file == NULL) {
    file_error("open", out_filename, strerror(errno));
    return -1;
  }

  while (fgets(line, sizeof(line), in_file)) {
    if (!strncmp(line, preference, plen))
      continue;

    if (fputs(line, *out_file)) {
      file_error("write", out_filename, strerror(errno));
      fclose(*out_file);
      unlink(out_filename);
      return -1;
    }
  }

  fclose(in_file);

  return 0;
}

static int _ds_pref_save_file (
  const char *filename,
  const char *out_filename,
  FILE *out_file
)
{
  if (fclose(out_file)) {
    file_error("close", out_filename, strerror(errno));
    return -1;
  }

  if (rename(out_filename, filename)) {
    report_error_printf("rename %s to %s: %s\n",
                out_filename, filename, strerror(errno));
    unlink(out_filename);
    return -1;
  }

  return 0;
}

int _ds_pref_set (
  attribute_t **config,
  const char *username,
  const char *home,
  const char *preference,
  const char *value,
  void *ignore)
{
  char filename[MAX_FILENAME_LENGTH];
  char out_filename[MAX_FILENAME_LENGTH];
  FILE *out_file;
  int r;

  r = _ds_pref_process_file(username, home, preference,
            filename, out_filename, &out_file);

  if (r)
    return -1;

  fprintf(out_file, "%s=%s\n", preference, value);

  return _ds_pref_save_file(filename, out_filename, out_file);
}

int _ds_pref_del (
  attribute_t **config,
  const char *username,
  const char *home,
  const char *preference,
  void *ignore)
{
  char filename[MAX_FILENAME_LENGTH];
  char out_filename[MAX_FILENAME_LENGTH];
  FILE *out_file;
  int r;

  r = _ds_pref_process_file(username, home, preference,
            filename, out_filename, &out_file);

  if (r)
    return -1;

  return _ds_pref_save_file(filename, out_filename, out_file);
}

#endif
