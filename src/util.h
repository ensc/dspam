/* $Id: util.h,v 1.20 2011/06/28 00:13:48 sbajic Exp $ */

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

void	chomp	(char *string);
char *	ltrim	(char *str);
char *	rtrim	(char *str);
int	lc	(char *buff, const char *string);
#ifndef HAVE_STRCASESTR
char *  strcasestr (const char *, const char *);
#endif

#define ALLTRIM(str)  ltrim(rtrim(str))

#ifndef HAVE_STRSEP
char *strsep (char **stringp, const char *delim);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy (char *, const char *, size_t);
size_t strlcat (char *, const char *, size_t);
#endif

/*
 * Specialized Functions
 * Utilities specialized for DSPAM functions.
 *
 *  _ds_userdir_path()
 *    Generates the path for files within DSPAM's filesystem, according to the
 *    filesystem structure specified at configure time.
 *
 *  _ds_prepare_path_for()
 *    Creates any necessary subdirectories to support the supplied path
 *
 *  _ds_get_crc64()
 *    Generates the CRC of the supplied string, using CRC64
 *
 *  _ds_compute_complexity()
 *    Calculates the complexity of a token based on its nGram depth
 *
 *  _ds_compute_sparse()
 *    Calculates the number of sparse skips in a token
 *
 *  _ds_compute_weight()
 *    Calculates the markovian weight of a token
 *
 *  _ds_compute_weight_osb()
 *    Calculates the OSB/OSBF/WINNOW weight of a token
 */

#ifndef HAVE_STRTOK_R
char * strtok_r(char *s1, const char *s2, char **lasts);
#endif

#ifndef HAVE_INET_NTOA_R
unsigned int i2a
  (char* dest,unsigned int x);
char *inet_ntoa_r
  (struct in_addr in, char *buf, int len);
#endif

#ifdef EXT_LOOKUP
  int verified_user;
#endif

const char *	_ds_userdir_path (
  char *buff,
  const char *home,
  const char *filename,
  const char *extension);

int _ds_prepare_path_for   (const char *filename);
int _ds_compute_complexity (const char *token);
int _ds_compute_sparse     (const char *token);
int _ds_compute_weight     (const char *token);
int _ds_compute_weight_osb (const char *token);
char *_ds_truncate_token   (const char *token);

int	_ds_extract_address(
  char *buf, 
  const char *address, 
  size_t len);

double	_ds_gettime(void);

void timeout(void);

int _ds_get_fcntl_lock  (int fd);
int _ds_free_fcntl_lock (int fd);

unsigned long long _ds_getcrc64
  (const char *);
int _ds_pow
  (int base, unsigned int exp);
int _ds_pow2
  (int exp);
double chi2Q
  (double x, int v);
float _ds_round
  (float n);

int _ds_validate_address
  (const char *address);
#endif /* _UTIL_H */
