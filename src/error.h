/* $Id: error.h,v 1.1 2004/10/24 20:49:34 jonz Exp $ */

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

#ifdef DEBUG
extern int DO_DEBUG;
#endif

char *format_date_r		(char *buf);
void  report_error		(const char *);
void  report_error_printf	(const char *fmt, ...);
void  file_error		(const char *operation,
				 const char *filename,
				 const char *error);
#ifndef HAVE_ISO_VARARGS
void debug (const char *);
void LOGDEBUG (char *, ...);
void LOG (int, char *, ...);
#else
#ifdef DEBUG
extern char debug_text[1024];
void debug (const char *text);
#	define	LOGDEBUG( ... )	\
	{ if (DO_DEBUG) { snprintf(debug_text, 1024, __VA_ARGS__ ); debug(debug_text); } }
#else
#	define LOGDEBUG( ... );
#endif

#ifdef _WIN32
#	define	LOG( ... )
#else
#	define	LOG( A, ... ) \
	{ openlog("dspam", LOG_PID, LOG_MAIL); syslog( A, __VA_ARGS__ ); \
	  closelog(); LOGDEBUG( __VA_ARGS__ ); \
          report_error_printf( __VA_ARGS__ ); \
        }
#endif
#endif

#endif /* _DSPAM_ERROR_H */
