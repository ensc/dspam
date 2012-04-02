/* $Id: config_api.h,v 1.10 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _CONFIG_API_H
#define _CONFIG_API_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "libdspam.h"

int set_libdspam_attributes(DSPAM_CTX *CTX);
int attach_context         (DSPAM_CTX *CTX, void *dbh);

#endif /* _CONFIG_API_H */
