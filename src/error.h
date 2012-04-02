/* $Id: error.h,v 1.13 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#ifndef _DSPAM_ERROR_H
#  define _DSPAM_ERROR_H

#ifdef _WIN32
#   define LOG_CRIT     2
#   define LOG_ERR      3
#   define LOG_WARNING  4
#   define LOG_INFO     6
#else
#include <syslog.h>
#endif
#ifdef DAEMON
#include <pthread.h>
extern pthread_mutex_t  __syslog_lock;
#endif

#ifdef DEBUG
extern int DO_DEBUG;
#endif

#ifndef DEBUG
#define LOGDEBUG( ... );
#else
void LOGDEBUG (const char *err, ... );
#endif

#ifdef _WIN32
#define LOG ( ... );
#else
void LOG (int priority, const char *err, ... );
#endif

char *format_date_r (char *buf);

void  debug_out (const char *);

#endif /* _DSPAM_ERROR_H */
