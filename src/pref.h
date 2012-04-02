/* $Id: pref.h,v 1.20 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _PREF_H
#  define _PREF_H

#include <sys/types.h>
#ifndef _WIN32
#include <pwd.h>
#endif

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "config_shared.h"

#define PREF_MAX	32

/* a single preference attribute */

typedef struct _ds_agent_attribute {
  char *attribute;
  char *value;
} *agent_attrib_t;

typedef agent_attrib_t *agent_pref_t;

/* preference utilities */

const char *	_ds_pref_val (agent_pref_t PTX, const char *attrib);
int             _ds_pref_free (agent_pref_t PTX);
agent_pref_t	_ds_pref_aggregate (agent_pref_t, agent_pref_t);
agent_attrib_t	_ds_pref_new (const char *attribute, const char *value);

agent_pref_t  _ds_ff_pref_load(
  config_t config,
  const char *user, 
  const char *home, 
  void *ignore);
int _ds_ff_pref_set(
  config_t config,
  const char *user,
  const char *home,
  const char *preference,
  const char *value,
  void *ignore);
int _ds_ff_pref_del(
  config_t config,
  const char *user,
  const char *home, 
  const char *preference,
  void *ignore);
FILE *_ds_ff_pref_prepare_file(
  const char *filename,
  const char *omission,
  int *nlines);
int _ds_ff_pref_commit (
  const char *filename,
  FILE *out_file);

#endif /* _PREF_H */
