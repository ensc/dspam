/* $Id: client.h,v 1.7 2005/03/12 21:23:09 jonz Exp $ */

/*

 DSPAM
 COPYRIGHT (C) 2002-2005 NETWORK DWEEBS CORPORATION

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

#ifndef _CLIENT_H
#  define _CLIENT_H

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#ifdef DAEMON

#include <sys/types.h>
#ifndef _WIN32
#include <pwd.h>
#endif

#include "dspam.h"
#include "buffer.h"

int client_process      (AGENT_CTX *ATX, buffer *message);
int client_connect      (int flags);
int client_authenticate (THREAD_CTX *TTX);
int client_getcode      (THREAD_CTX *TTX);
char * client_expect    (THREAD_CTX *TTX, int response_code);
char * client_getline   (THREAD_CTX *TTX, int timeout);
int deliver_socket      (AGENT_CTX *ATX, const char *message, int proto);

/* Shared between client and server */
char *pop_buffer  (THREAD_CTX *TTX);
int   send_socket (THREAD_CTX *TTX, const char *ptr);

#define CCF_PROCESS	0x00	/* Establish connection to process host */
#define CCF_DELIVERY	0x01	/* Establish connection to delivery host */

#define DDP_LMTP	0x00
#define DDP_SMTP	0x01

#endif /* _CLIENT_H */

#endif /* DAEMON */
