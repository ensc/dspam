/* $Id: read_config.h,v 1.3 2005/01/12 03:12:26 jonz Exp $ */

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

#ifndef _READ_CONFIG_H
#define _READ_CONFIG_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "config_shared.h"
#include "libdspam.h"
#include "pref.h"

struct attribute **read_config(const char *path);

int configure_algorithms    (DSPAM_CTX * CTX);

agent_pref_t pref_config(void);

attribute_t **agent_config;

#endif /* _READ_CONFIG_H */
