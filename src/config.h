/* $Id: config.h,v 1.16 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _DEFS_H
#define _DEFS_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <limits.h>
#ifndef _WIN32
#include <sys/param.h>
#endif

#ifndef CONFIG_DEFAULT
#	warning CONFIG_DEFAULT is not defined by configure/Makefile
#	warning Fallback to hardcoded defaults
#define CONFIG_DEFAULT	"/usr/local/etc/dspam.conf"
#endif

#ifndef CONFIGURE_ARGS
#define CONFIGURE_ARGS "default"
#endif

/* Acceptable Word Delimiters */
/*
OLD DELIMITERS
#define DELIMITERS              " .,;:\"/\\[]}{=+_()<>|&\n\t\r@-*~`?"
#define DELIMITERS_HEADING      " ,;:\"/\\[]}{=+()<>|&\n\t\r@*~`?"

| " : +FP,+ FN
 - : -FP, -FN
+ *: 
*/

#define SPARSE_DELIMITERS           " .,;:\"/\\[]}{=+_()<>|&\n\t\r@-*~`?#$%^"
#define SPARSE_DELIMITERS_HEADING   " ,;:\"/\\[]}{=+()<>|&\n\t\r@*~`?#$%^"

#define DELIMITERS              " .,;:\n\t\r@-+*"
#define DELIMITERS_HEADING      " ,;:\n\t\r@-+*"

#define DELIMITERS_EOT		"!"

/* Our 64-bit Polynomial */
#define POLY64REV       0xd800000000000000ULL

#ifndef LOGDIR
#	warning LOGDIR is not defined by configure/Makefile
#	warning Fallback to hardcoded defaults
#define LOGDIR "/usr/local/var/dspam/log"
#endif

/* General-Purpose Character Array Sizes */
#ifdef PATH_MAX
#	define MAX_FILENAME_LENGTH	PATH_MAX
#else
#	define MAX_FILENAME_LENGTH	128
#endif

/* General-Purpose Character Array Sizes */
#ifdef LONG_USERNAMES
#	define MAX_USERNAME_LENGTH	256
#else
#ifdef LOGIN_NAME_MAX
#       define MAX_USERNAME_LENGTH      LOGIN_NAME_MAX
#else
#       define MAX_USERNAME_LENGTH      256
#endif
#endif

#ifndef MAX
#define MAX(a,b)  ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b)  ((a)<(b)?(a):(b))
#endif

#ifndef LLU_FMT_SPEC
#define LLD_FMT_SPEC "lld"
#define LLU_FMT_SPEC "llu"
#endif

#endif
