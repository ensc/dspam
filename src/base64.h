/* $Id: base64.h,v 1.9 2010/01/03 14:26:31 sbajic Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2010 DSPAM PROJECT

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

#ifndef _BASE64_H
#  define _BASE64_H

char *base64decode (const char *);
char *base64encode (const char *);

#endif /* _BASE64_H */
