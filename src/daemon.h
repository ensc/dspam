/* $Id: daemon.h,v 1.11 2004/12/03 01:30:32 jonz Exp $ */

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

#ifndef _DAEMON_H
#  define _DAEMON_H

#include <sys/types.h>
#ifndef _WIN32
#include <pwd.h>
#endif

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "buffer.h"
#include "dspam.h"
#include "libdspam.h"

#ifdef DAEMON

typedef struct {
  int sockfd;
  char authenticated;
  pthread_t thread;
  struct sockaddr_in remote_addr;
  buffer *packet_buffer;
  DRIVER_CTX *DTX;
} THREAD_CTX;

int daemon_listen(DRIVER_CTX *DTX);
void *process_connection(void *ptr);
buffer * read_sock(THREAD_CTX *TTX, AGENT_CTX *ATX);
int process_users_daemon(THREAD_CTX *TTX, AGENT_CTX *ATX, buffer *message);
char *daemon_expect(THREAD_CTX *TTX, const char *ptr);
int daemon_reply(THREAD_CTX *TTX, int reply, const char *txt);

#define LMTP_GREETING		220
#define LMTP_QUIT		221
#define LMTP_OK			250
#define LMTP_DATA		354
#define LMTP_ERROR_PROCESS	500
#define LMTP_FAILURE		530
#define LMTP_AUTH_ERROR		503
#define LMTP_BAD_CMD		503

#endif
#endif /* _DAEMON_H */
