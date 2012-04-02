/* $Id: error.c,v 1.21 2011/06/28 00:13:48 sbajic Exp $ */

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

/*
 * error.c - error reporting
 *
 * DESCRIPTION
 *   The error reporting facilities include:
 *     LOGDEBUG  Log to debug only
 *     LOG       Log to syslog, stderr, and debug
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
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

#ifdef DAEMON
#include <pthread.h>
pthread_mutex_t  __syslog_lock;
#endif

#ifdef _WIN32
    #include <process.h>
#endif

#ifndef _WIN32
void
LOG(int priority, const char *err, ... )
{
#if defined(USE_SYSLOG) || defined(LOGFILE)
  va_list ap;
  va_start (ap, err);
#endif
#ifdef LOGFILE
  char date[128];
  FILE *file;
#endif

#ifdef DAEMON
#if defined(USE_SYSLOG) && defined(LOGFILE)
  pthread_mutex_lock(&__syslog_lock);
#endif
#endif

#ifdef USE_SYSLOG
  openlog ("dspam", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_MAIL);
  vsyslog (priority, err, ap);
  closelog ();
#endif

#ifdef LOGFILE
  char err_text[256];
  vsnprintf(err_text, sizeof(err_text), err, ap);
  file = fopen(LOGFILE, "a");
  if (file) {
    fprintf(file, "%ld: [%s] %s\n", (long) getpid(), format_date_r(date), err_text);
    fclose(file);
  } else {
    fprintf(stderr, "%s: %s", LOGFILE, strerror(errno));
    fprintf(stderr, "%ld: [%s] %s\n", (long)getpid(), format_date_r(date), err_text);
  }
#endif

#if defined(USE_SYSLOG) || defined(LOGFILE)
  va_end (ap);
#endif

#ifdef DAEMON
#if defined(USE_SYSLOG) && defined(LOGFILE)
  pthread_mutex_unlock(&__syslog_lock);
#endif
#endif

  return;
}
#endif

#ifdef DEBUG
void
LOGDEBUG (const char *err, ... )
{
  char debug_text[1024];
  va_list args;

  if (!DO_DEBUG)
    return;

  va_start (args, err);
  vsnprintf (debug_text, sizeof (debug_text), err, args);
  va_end (args);

  debug_out(debug_text);
}

void
debug_out (const char *err)
{
  FILE *file;
  char fn[MAX_FILENAME_LENGTH];
  char buf[128];

  if (DO_DEBUG == 1) {
    snprintf (fn, sizeof (fn), "%s/dspam.debug", LOGDIR);

    file = fopen (fn, "a");
    if (file != NULL) {
      fprintf(file, "%ld: [%s] %s\n", (long) getpid(), format_date_r(buf), err);
      fclose(file);
    }
  } else if (DO_DEBUG == 2) {
    printf ("%ld: [%s] %s\n", (long) getpid (), format_date_r(buf), err);
  }
  return;
}
#endif

char *
format_date_r(char *buf)
{
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

  sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d",
          l->tm_mon+1, l->tm_mday, l->tm_year+1900, l->tm_hour, l->tm_min,
          l->tm_sec);
  return buf;
}
