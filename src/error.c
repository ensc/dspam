/* $Id: error.c,v 1.2 2004/11/30 19:46:09 jonz Exp $ */

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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   ifdef HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif

#include "error.h"
#include "util.h"
#include "config.h"

#ifdef _WIN32
    #include <process.h>
#endif

/*  Subroutine: report_error
   Description: Reports an error to the supported facilities (currently stderr)
    Parameters: Error message to report
*/

void
report_error (const char *error)
{
  char buf[128];

  fprintf (stderr, "%ld: [%s] %s\n", (long) getpid (), format_date_r(buf), error);

#ifdef DEBUG
  debug (error);
#endif

  return;
}

void
report_error_printf (const char *fmt, ...)
{
  va_list ap;
  char buff[1024];
  va_start (ap, fmt);
  vsnprintf (buff, sizeof (buff), fmt, ap);
  report_error (buff);
  va_end (ap);
  return;
}

void
file_error (const char *operation, const char *filename, const char *error)
{
  char errmsg[1024];

  snprintf (errmsg, sizeof (errmsg), "%s: %s: %s", operation, filename,
            error);
  report_error (errmsg);
  return;
}

#ifdef DEBUG
void
debug (const char *text)
{
  FILE *file;
  char fn[MAX_FILENAME_LENGTH];
  char buf[128];

  if (DO_DEBUG == 1) {
    snprintf (fn, sizeof (fn), "%s/dspam.debug", LOGDIR);

    file = fopen (fn, "a");
    if (file != NULL)
    {
      fprintf (file, "%ld: [%s] %s\n", (long) getpid (), format_date_r(buf), text);
      fclose (file);
    }
  } else if (DO_DEBUG == 2) {
    printf ("%ld: [%s] %s\n", (long) getpid (), format_date_r(buf), text);
  }
  return;
}
#endif

#ifndef HAVE_ISO_VARARGS
void
LOGDEBUG (char *text, ...)
{
#ifdef DEBUG
  char debug_text[1024];
  va_list args;

  if (!DO_DEBUG)
    return;

  va_start (args, text);
  vsnprintf (debug_text, sizeof (debug_text), text, args);
  va_end (args);

  debug (debug_text);
#endif
}

void
LOG (int priority, char *text, ...)
{
#ifdef DEBUG
#ifdef _WIN32
  char log_text[1024];
  va_list args;
  va_start (args, text);
  vsnprintf (log_text, sizeof (log_text), text, args);
  va_end (args);
  debug(log_text);
#else
  va_list args;

  va_start (args, text);
  openlog ("dspam", LOG_PID, LOG_MAIL);
  syslog (priority, text, args);
  closelog ();
  LOGDEBUG (text, args);
  va_end (args);
#endif
#endif
}
#endif

char *
format_date_r(char *buf) {
  struct tm *l;
#ifdef HAVE_LOCALTIME_R
  struct tm lt;
#endif
  time_t t = time(NULL);
                                                                                
#ifdef HAVE_LOCALTIME_R
  l = localtime_r(&t, &lt);
#else
  l = localtime(&t);
#endif
                                                                                
  sprintf(buf, "%d/%d/%d %d:%d:%d",
           l->tm_mon+1, l->tm_mday, l->tm_year+1900, l->tm_hour, l->tm_min,
           l->tm_sec);
  return buf;
}
