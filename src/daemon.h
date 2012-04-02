/* $Id: daemon.h,v 1.30 2011/06/28 00:13:48 sbajic Exp $ */

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

int __daemon_run;	/* should we keep running? */
int __num_threads;	/* number of live threads */
int __hup;		/* should we reload? */
pthread_mutex_t __lock;	/* global var lock */

typedef struct {
  int sockfd;
  char authenticated;
  pthread_t thread;
  struct sockaddr_in remote_addr;
  buffer *packet_buffer;
  DRIVER_CTX *DTX;
} THREAD_CTX;

int	daemon_listen(DRIVER_CTX *DTX);
int	daemon_reply(THREAD_CTX *TTX, int reply, const char *ecode, 
        const char *text);
int	daemon_extension(THREAD_CTX *TTX, const char *extension);
void *	process_connection(void *ptr);
char *	daemon_expect(THREAD_CTX *TTX, const char *command);
char *	daemon_getline(THREAD_CTX *TTX, int timeout);
void	process_signal(int sig);
void	increment_thread_count(void);
void	decrement_thread_count(void);
buffer *read_sock(THREAD_CTX *TTX, AGENT_CTX *ATX);

#define LMTP_GREETING		220
#define LMTP_QUIT		221
#define LMTP_OK			250
#define LMTP_DATA		354
#define LMTP_ERROR_PROCESS	500
#define LMTP_FAILURE		530
#define LMTP_AUTH_ERROR		503
#define LMTP_BAD_CMD		503
#define LMTP_NO_RCPT		554
#define LMTP_TEMP_FAIL		421

#define SSM_DSPAM	0x00
#define SSM_STANDARD	0x01
#define SSM_AUTO	0x02

#endif
#endif /* _DAEMON_H */
