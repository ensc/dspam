/* $Id: util.h,v 1.5 2005/02/25 14:52:14 jonz Exp $ */

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

#ifndef _UTIL_H
#  define _UTIL_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef _WIN32
#include <pwd.h>
#endif

/*
  general utilities

  chomp()
    Removes a trailing newline from a string

  ltrim(), rtrim(), ALLTRIM()
    Trims leading ant/or trailing whitespace from a string

  lc()
    Convert and copy a string to lowercase

  strsep()
    Extract tokens from strings 

  strlcpy(), strlcat()
    Secure string copy/concat functions

*/

void	chomp	(char *string);
char *	ltrim	(char *str);
char *	rtrim	(char *str);
int	lc	(char *buff, const char *string);
char *  strcasestr (const char *, const char *);

#define ALLTRIM(str)  ltrim(rtrim(str))

#ifndef HAVE_STRSEP
char *strsep (char **stringp, const char *delim);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy (char *, const char *, size_t);
size_t strlcat (char *, const char *, size_t);
#endif

/*
  specialized utilities
 
  Utilities specialized for DSPAM functions.

  _ds_userdir_path()
    Generates the path for files within DSPAM's filesystem, according to the
    filesystem structure specified at configure time.

  _ds_prepare_path_for()
    Creates any necessary subdirectories to support the supplied path

  _ds_get_crc64()
    Generates the CRC of the supplied string, using CRC64

  _ds_compute_complexity()
    Calculates the complexity of a token based on its nGram depth
*/

#ifndef HAVE_STRTOK_R
char * strtok_r(char *s1, const char *s2, char **lasts);
#endif

#ifndef HAVE_INET_NTOA_R
unsigned int i2a(char* dest,unsigned int x);
char *inet_ntoa_r(struct in_addr in, char *buf, int len);
#endif

const char *	_ds_userdir_path (char *buff, const char *home, const char *filename, const char *extension);
int		_ds_prepare_path_for	(const char *filename);
int		_ds_compute_complexity	(const char *token);
char *		_ds_truncate_token	(const char *token);
int		_ds_extract_address(char *buf, const char *address, size_t len);

int _ds_get_fcntl_lock	(int fd);
int _ds_free_fcntl_lock	(int fd);

unsigned long long _ds_getcrc64		(const char *);
int _ds_pow2(int exp);

double chi2Q (double x, int v);
float _ds_round(float n);


#endif /* _UTIL_H */
