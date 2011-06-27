/* $Id: external_lookup.h,v 1.00 2009/12/22 12:25:59 sbajic Exp $ */

/*
 COPYRIGHT (C) 2006 HUGO MONTEIRO

 external lookup library for DSPAM v0.1

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2
 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifdef EXT_LOOKUP

#ifndef _EXT_LOOKUP_H
#define _EXT_LOOKUP_H

#include "agent_shared.h"
#include "libdspam.h"

int verified_user;

void sig_alrm(int signum);
char *transcode_query(const char *query, const char *username, char *transcoded_query);
char *external_lookup(config_t agent_config, const char *username, char *external_uid);
char *ldap_lookup(config_t agent_config, const char *username, char *external_uid);
char *program_lookup(config_t agent_config, const char *username, char *external_uid);

#endif /* _EXTERNAL_LOOKUP_H */

#endif /* USE_EXTLOOKUP */
