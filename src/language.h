/* $Id: language.h,v 1.2 2004/11/24 17:57:47 jonz Exp $ */

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

#ifndef _LANGUAGE_H
#  define _LANGUAGE_H

#define LANGUAGE	"en"

/* Error Messages */

#define ERROR_USER_UNDEFINED	"Unable to determine the destination user"
#define ERROR_MEM_ALLOC         "Memory allocation error"
#define ERROR_FILE_OPEN		"Unable to open file for reading"
#define ERROR_FILE_WRITE	"Unable to open file for writing"
#define ERROR_DIR_CREATE	"Unable to create directory"
#define ERROR_READ_CONFIG	"Unable to read dspam.conf"
#define ERROR_DSPAM_HOME	"DSPAM Home must be defined in dspam.conf"
#define ERROR_RUNTIME_USER	"Unable to determine the runtime user"
#define ERROR_NO_SUCH_PROFILE	"No Such Profile: %s"
#define ERROR_UNKNOWN_CLASS	"Unknown Class '%s'"
#define ERROR_UNKNOWN_SOURCE	"Unknown Source '%s'"
#define ERROR_UNKNOWN_DELIVER	"Unknown Deliver Option '%s'"
#define ERROR_UNKNOWN_FEATURE	"Unknown Feature '%s'"
#define ERROR_NO_AGENT		"No %s configured, and --stdout not specified"
#define ERROR_NO_SOURCE		"No source was specified (required with class)"
#define ERROR_NO_CLASS		"No class was specified (required with source)"
#define ERROR_NO_OP_MODE	"No operating mode was specified"
#define ERROR_NO_TR_MODE	"No training mode was specified"
#define ERROR_TB_INVALID	"Training buffer level specified is invalid"
#define ERROR_TR_MODE_INVALID	"Invalid training mode specified"
#define ERROR_IGNORING_PREF	"Ignoring Disallowed Preference '%s'"
#define ERROR_INITIALIZE_ATX	"Unable to initialize agent context"
#define ERROR_DSPAM_MISCONFIGURED	"DSPAM misconfigured. Aborting."

/* Delivery Agent Failures */

#define ERROR_AGENT_RETURN	"Delivery agent returned error, exit code: %d, command line: %s"
#define ERROR_AGENT_SIGNAL	"Delivery agent terminated by signal #%d, command line: %s"
#define ERROR_AGENT_CLOSE	"Unexpected return code when closing pipe: %d"

/* Trusted-User Related */

#define ERROR_TRUSTED_USER	"Option --user requires special privileges when user does not match current user, e.g.. root or Trusted User [uid=%d(%s)]"
#define ERROR_TRUSTED_OPTION	"Option %s requires special privileges; e.g.. root or Trusted User [uid=%d(%s)]"
#define ERROR_TRUSTED_MODE	"Program mode requires special privileges, e.g., root or Trusted User"

/* libdspam errors */

#define ERROR_DSPAM_INIT	"dspam_init() failed: %s"
#define ERROR_NO_HOME		"no dspam home was provided"

/* storage driver errors */

#define ERROR_NO_ATTACH		"Driver does not support attaching database handles. Second argument of dspam_attach() must be NULL."
#define ERROR_NO_MERGED		"Driver does not support merged groups"

/* daemon process */

#define DAEMON_START		"Daemon process starting"
#define DAEMON_EXIT		"Daemon process exiting"

#endif /* _LANGUAGE_H */
