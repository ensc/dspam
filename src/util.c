/* $Id: util.c,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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
#ifndef _WIN32
#   include <pwd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif
#include <stdio.h>
#include <math.h>
#include <fcntl.h>

#include "language.h"
#include "config.h"
#include "util.h"
#include "libdspam.h"

#ifdef _WIN32
    #include <direct.h>

    #define mkdir(filename, perm) _mkdir(filename)
#endif

#ifndef HAVE_STRTOK_R
char *
strtok_r(char *s1, const char *s2, char **lasts)
{
  char *ret;
                                                                                
  if (s1 == NULL)
    s1 = *lasts;
  while(*s1 && strchr(s2, *s1))
    ++s1;
  if(*s1 == '\0')
    return NULL;
  ret = s1;
  while(*s1 && !strchr(s2, *s1))
    ++s1;
  if(*s1)
    *s1++ = '\0';
  *lasts = s1;
  return ret;
}
#endif /* HAVE_STRTOK_R */

/* Compliments of Jay Freeman <saurik@saurik.com> */

#ifndef HAVE_STRSEP
char *
strsep (char **stringp, const char *delim)
{
  char *ret = *stringp;
  if (ret == NULL)
    return (NULL);
  if ((*stringp = strpbrk (*stringp, delim)) != NULL)
    *((*stringp)++) = '\0';
  return (ret);
}
#endif

/*
    Subroutine: chomp
   Description: removes a trailing newline character
    Parameters: char *string    string to chomp
*/

void
chomp (char *string)
{
  int len;
  if (string == NULL)
    return;
  len = strlen (string);
  if (len && string[len - 1] == 10)
  {
    string[len - 1] = 0;
    len--;
  }
  if (len && string[len - 1] == 13)
    string[len - 1] = 0;
  return;
}

char *
ltrim (char *str)
{
  char *p;
  if (!str || !str[0])
    return str;
  for (p = str; isspace ((int) *p); ++p)
  {
    /* do nothing */
  }
  if (p > str)
    strcpy (str, p);            /* __STRCPY_CHECKED__ */
  return str;
}

char *
rtrim (char *str)
{
  size_t offset;
  char *p;
  if (!str || !str[0])
    return str;
  offset = strlen (str);
  p = str + offset - 1;         /* now p points to the last character in
                                 * string */
  for (; p >= str && isspace ((int) *p); --p)
  {
    *p = 0;
  }
  return str;
}

#ifndef HAVE_STRLCPY
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat (dst, src, siz)
     char *dst;
     const char *src;
     size_t siz;
{
  register char *d = dst;
  register const char *s = src;
  register size_t n = siz;
  size_t dlen;

  /* Find the end of dst and adjust bytes left but don't go past end */
  while (n-- != 0 && *d != '\0')
    d++;
  dlen = d - dst;
  n = siz - dlen;

  if (n == 0)
    return (dlen + strlen (s));
  while (*s != '\0')
  {
    if (n != 1)
    {
      *d++ = *s;
      n--;
    }
    s++;
  }
  *d = '\0';

  return (dlen + (s - src));    /* count does not include NUL */
}

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy (dst, src, siz)
     char *dst;
     const char *src;
     size_t siz;
{
  register char *d = dst;
  register const char *s = src;
  register size_t n = siz;

  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
  {
    do
    {
      if ((*d++ = *s++) == 0)
        break;
    }
    while (--n != 0);
  }

  /* Not enough room in dst, add NUL and traverse rest of src */
  if (n == 0)
  {
    if (siz != 0)
      *d = '\0';                /* NUL-terminate dst */
    while (*s++)
      ;
  }

  return (s - src - 1);         /* count does not include NUL */
}
#endif

const char * _ds_userdir_path (char *path, const char *home, const char *filename, const char *extension)
{
  char username[MAX_FILENAME_LENGTH];
  char userpath[MAX_FILENAME_LENGTH];
#ifdef DOMAINSCALE
  char *f, *domain, *user;
  char *ptrptr;
#endif
#ifdef HOMEDIR
  struct passwd *p;
#if defined(_REENTRANT) && defined(HAVE_GETPWNAM_R)
  struct passwd pwbuf;
  char buf[1024];
#endif
  char userhome[MAX_FILENAME_LENGTH];
#endif

  if (filename == NULL || filename[0] == 0)
  {
    path[0] = 0;
    return path;
  }

#ifdef HOMEDIR
#if defined(_REENTRANT) && defined(HAVE_GETPWNAM_R)
  if (getpwnam_r(filename, &pwbuf, buf, sizeof(buf), &p))
    p = NULL;
#else
  p = getpwnam(filename);
#endif

  if (p == NULL) 
      strcpy(userhome, home);
  else
      strlcpy(userhome, p->pw_dir, sizeof(userhome));

  if (extension != NULL
      && (!strcmp (extension, "nodspam") || !strcmp (extension, "dspam")))
  {
    if (p != NULL) {
      snprintf (path, MAX_FILENAME_LENGTH, "%s/.%s", p->pw_dir, extension);
#ifdef DEBUG
      LOGDEBUG ("using %s as path", path);
#endif
      return path;
    }
  }
#endif /* HOMEDIR */

#ifdef DOMAINSCALE
  f = strdup(filename);
  user = strtok_r(f, "@", &ptrptr);
  domain = strtok_r(NULL, "@", &ptrptr);
                                                                                
  if (domain == NULL)
    domain = "local";
  snprintf(userpath, MAX_FILENAME_LENGTH, "%s/%s", domain, user);
  strlcpy(username, user, MAX_FILENAME_LENGTH);
  free(f);
#else
  strlcpy(username, filename, MAX_FILENAME_LENGTH);
  strcpy(userpath, username);
#endif

  /* Use home/opt-in/ and home/opt-out/ to store opt files, instead of
     each user's directory */

  if (extension != NULL
      && (!strcmp (extension, "nodspam") || !strcmp (extension, "dspam")))
  {
    snprintf (path, MAX_FILENAME_LENGTH, "%s/opt-%s/%s.%s", home, 
      (!strcmp(extension, "nodspam")) ? "out" : "in", userpath, extension);
#ifdef DEBUG
      LOGDEBUG ("using %s as path", path);
#endif
      return path; 
  }

#ifdef LARGESCALE
  if (filename[1] != 0)
  {
    if (extension == NULL)
    {
      snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%c/%s",
                home, filename[0], filename[1], filename);
    }
    else
    {
      if (extension[0] == 0)
        snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%c/%s/%s",
                  home, filename[0], filename[1], filename, filename);
      else
        snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%c/%s/%s.%s",
                  home, filename[0], filename[1], filename, filename,
                  extension);
    }
  }
  else
  {
    if (extension == NULL)
    {
      snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%s",
                home, filename[0], filename);
    }
    else
    {
      if (extension[0] == 0)
        snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%s/%s",
                  home, filename[0], filename, filename);
      else
        snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%c/%s/%s.%s",
                  home, filename[0], filename, filename, extension);
    }
  }
#else
  if (extension == NULL)
  {
#ifdef HOMEDIR
    snprintf (path, MAX_FILENAME_LENGTH, "%s/.dspam", userhome);
#else
    snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%s", home, userpath);
#endif
  }
  else
  {
#ifdef HOMEDIR
    snprintf(path, MAX_FILENAME_LENGTH, "%s/.dspam/%s.%s", userhome, username,
             extension);
#else
    snprintf (path, MAX_FILENAME_LENGTH, "%s/data/%s/%s.%s",
              home, userpath, username, extension);
#endif
  }
#endif

  return path;
}

int
_ds_prepare_path_for (const char *filename)
{
  char path[MAX_FILENAME_LENGTH];
  char *dir, *file;
  char *file_buffer_start;
  struct stat s;
                                                                                
  if (!filename)
  {
    LOG (LOG_ERR,
         "_ds_prepare_path_for(): invalid argument: filename == NULL");
    return EUNKNOWN;
  }
                                                                                
  file = strdup (filename);
  if (!file)
  {
    LOG (LOG_ERR, "not enought memory");
    return EFAILURE;
  }

#ifdef _WIN32
  /*
     Windows uses both slash and backslash as path separators while the code
     below only deals with slashes -- make it work by adjusting the path.
   */
  {
    char *p;
    for ( p = strchr(file, '\\'); p; p = strchr(p + 1, '\\') )
    {
      *p = '/';
    }
  }
#endif

  file_buffer_start = file;
  path[0] = 0;
                                                                                
  dir = strsep (&file, "/");
  while (dir != NULL)
  {
    strlcat (path, dir, sizeof (path));
    dir = strsep (&file, "/");

#ifdef _WIN32
    /* don't try to create root directory of a drive */
    if ( path[2] != '\0' || path[1] != ':' )
#endif
    {
      if (dir != NULL && stat (path, &s) && path[0] != 0)
      {
        int x;
        LOGDEBUG ("creating directory '%s'", path);
        x = mkdir (path, 0770);
        if (x)
        {
          file_error (ERROR_DIR_CREATE, path, strerror (errno));
          free (file_buffer_start);
          return EFILE;
        }
      }
    }

    strlcat (path, "/", sizeof (path));
  }
  free (file_buffer_start);
  return 0;
}


/*  Subroutine: lc
   Description: converts a string to lowercase
    Parameters: char *buffer            pointer to the target buffer
                const char *string      pointer to the source text
       Returns: the number of characters converted to lowercase
                                                                                
         Notes: buffer and string can safely be set to the same
                memory pointer
*/

int
lc (char *buff, const char *string)
{
  char *buffer;
  int i, j = 0;
  int len = strlen (string);

  buffer = malloc (len + 1);

  if (len == 0)
  {
    buff[0] = 0;
    free (buffer);
    return 0;
  }

  for (i = 0; i < len; i++)
  {
    if (isupper ((int) string[i]))
    {
      buffer[i] = tolower (string[i]);
      j++;
    }
    else
    {
      buffer[i] = string[i];
    }
  }

  buffer[len] = 0;
  strcpy (buff, buffer);

  free (buffer);
  return j;
}

unsigned long long
_ds_getcrc64 (const char *s)
{
  static unsigned long long CRCTable[256];
  unsigned long long crc = 0;
  static int init = 0;
                                                                                
  if (!init)
  {
    int i;
    init = 1;
    for (i = 0; i <= 255; i++)
    {
      int j;
      unsigned long long part = i;
      for (j = 0; j < 8; j++)
      {
        if (part & 1)
          part = (part >> 1) ^ POLY64REV;
        else
          part >>= 1;
      }
      CRCTable[i] = part;
    }
  }
  for (; *s; s++)
  {
    unsigned long long temp1 = crc >> 8;
    unsigned long long temp2 =
      CRCTable[(crc ^ (unsigned long long) *s) & 0xff];
    crc = temp1 ^ temp2;
  }
                                                                                
  return crc;
}


/* Compute the complexity of a token */
int _ds_compute_complexity(const char *token) {
  int i, complexity = 1;
                                                                                
  if (token == NULL)
    return 1;
                                                                                
  for(i=0;token[i];i++) {
    if (token[i] == '+')
      complexity++;
  }
                                                                                
  return complexity;
}

/* Truncate tokens with EOT delimiters */
char * _ds_truncate_token(const char *token) {
  char *tweaked;
  int i;

  if (token == NULL)
    return NULL;

  tweaked = strdup(token);

  if (tweaked == NULL)
    return NULL;

  i = strlen(tweaked);
  while(i>1 && strspn(tweaked+i-2, DELIMITERS_EOT)) {
    tweaked[i-1] = 0;
    i--;
  }

  return tweaked;
}

double chi2Q (double x, int v)
{
  int i;
  double m, s, t;

  m = x / 2.0;
  s = exp(-m);
  t = s;

  for(i=1;i<(v/2);i++) {
    t *= m / i;
    s += t;
  }

  return MIN(s, 1.0);
}

int _ds_get_fcntl_lock(int fd) {
#ifdef _WIN32
  return 0;
#else
  struct flock f;

  f.l_type = F_WRLCK;
  f.l_whence = SEEK_SET;
  f.l_start = 0;
  f.l_len = 0;

  return fcntl(fd, F_SETLKW, &f);
#endif
}

int _ds_free_fcntl_lock(int fd) {
#ifdef _WIN32
  return 0;
#else
  struct flock f;
                                                                                
  f.l_type = F_UNLCK;
  f.l_whence = SEEK_SET;
  f.l_start = 0;
  f.l_len = 0;

  return fcntl(fd, F_SETLKW, &f);
#endif
} 

int _ds_pow2(int exp) {
  int j = 1, i;
  if (!exp)
    return j;
  for(i=0;i<exp;i++)
    j *= 2;
  return j;
}

