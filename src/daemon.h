/* $Id: daemon.h,v 1.4 2004/11/30 19:46:09 jonz Exp $ */

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

#ifdef DAEMON

typedef struct {
  int sockfd;
  char authenticated;
  pthread_t thread;
  struct sockaddr_in remote_addr;
  buffer *packet_buffer;
} THREAD_CTX;

int daemon_listen(void);
void *process_connection(void *ptr);
char *socket_getline(THREAD_CTX *TTX, int timeout);
char *pop_buffer(THREAD_CTX *TTX);

#endif /* _DAEMON_H */

#endif
