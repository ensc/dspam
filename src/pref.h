/* $Id: pref.h,v 1.5 2005/01/11 19:24:30 jonz Exp $ */

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

typedef struct {
  char *attribute;
  char *value;
} AGENT_ATTRIB;

#define AGENT_PREF AGENT_ATTRIB**

/* preference utilities */

const char *	_ds_pref_val (AGENT_PREF PTX, const char *attrib);
int             _ds_pref_free (AGENT_PREF PTX);
AGENT_PREF	_ds_pref_aggregate(AGENT_PREF, AGENT_PREF);
AGENT_ATTRIB	*_ds_pref_new(const char *attribute, const char *value);

#ifndef PREFERENCES_EXTENSION
AGENT_PREF      _ds_pref_load(attribute_t **config, const char *user, const char *home, void *ignore);
#endif

#endif /* _PREF_H */
