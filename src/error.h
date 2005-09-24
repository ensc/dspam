/* $Id: error.h,v 1.8 2005/09/24 17:48:59 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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
