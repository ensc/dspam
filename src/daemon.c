/* $Id: daemon.c,v 1.4 2004/11/30 19:46:09 jonz Exp $ */

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

#ifdef DAEMON

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "daemon.h"
#include "dspam.h"
#include "libdspam.h"
#include "config.h"
#include "util.h"
#include "buffer.h"
#include "language.h"

int daemon_listen() {
  int port = 1900;
  int queue = 32;
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
  struct timeval tv;
  THREAD_CTX *TTX = NULL;
  int fdmax;
  int listener;
  int yes = 1;
  int i;
  pthread_attr_t attr;
  fd_set master, read_fds;

  /* Set Defaults */
  if (_ds_read_attribute(agent_config, "ServerPort"))
    port = atoi(_ds_read_attribute(agent_config, "ServerPort"));

  if (_ds_read_attribute(agent_config, "ServerQueueSize"))
    queue = atoi(_ds_read_attribute(agent_config, "ServerQueueSize"));

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  LOGDEBUG("creating listening socket on port %d", port);

  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener == -1) {
    LOG(LOG_CRIT, ERROR_DAEMON_SOCKET, strerror(errno));
    return(EFAILURE);
  }

  if (setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
    LOG(LOG_CRIT, ERROR_DAEMON_SOCKOPT, "SO_REUSEADDR", strerror(errno));
    return(EFAILURE);
  }

  memset(&local_addr, 0, sizeof(struct sockaddr_in));
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(port);
  local_addr.sin_addr.s_addr = INADDR_ANY;

  LOGDEBUG(DAEMON_BINDING, port);

  if (bind(listener, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) == -1) {
    LOG(LOG_CRIT, ERROR_DAEMON_BIND, port, strerror(errno));
    return(EFAILURE);
  }

  if (listen(listener, queue) == -1) {
    LOG(LOG_CRIT, ERROR_DAEMON_LISTEN, strerror(errno));
    return(EFAILURE);
  }

  FD_SET(listener, &master);
  fdmax = listener;

  for(;;) {
    read_fds = master;

    tv.tv_sec = 0;
    tv.tv_usec = 50;

    if (select(fdmax+1, &read_fds, NULL, NULL, &tv)>0) {

      /* Process read-ready connections */

      for(i=0;i<=fdmax;i++) {
        if (FD_ISSET(i, &read_fds)) {

          /* Accept new connections */

          if (i == listener) {
            int newfd;
            int addrlen = sizeof(remote_addr);
            if ((newfd = accept(listener, 
                                (struct sockaddr *)&remote_addr, 
                                &addrlen)) == -1)
            {
              LOG(LOG_WARNING, ERROR_DAEMON_ACCEPT, strerror(errno));
            } else {
              LOGDEBUG("connection id %d from %s.", newfd, 
                       inet_ntoa(remote_addr.sin_addr));
              fcntl(newfd, F_SETFL, O_NONBLOCK);
              setsockopt(newfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

              TTX = calloc(1, sizeof(THREAD_CTX));
              if (TTX == NULL) {
                LOG(LOG_CRIT, ERROR_MEM_ALLOC);
                close(newfd);
                continue;
              } else {
                TTX->sockfd = newfd;
                memcpy(&TTX->remote_addr, &remote_addr, sizeof(remote_addr));
                if (pthread_create(&TTX->thread, 
                                   &attr, process_connection, (void *) TTX))
                {
                  LOG(LOG_CRIT, ERROR_DAEMON_THREAD, strerror(errno));
                  close(TTX->sockfd);
                  free(TTX);
                  continue;
                }
              }
            }
          } /* if i == listener */
        } /* if FD_SET else */
      } /* for(i.. */
    }  /* if (select)... */
  } /* for(;;) */

  /* Shutdown - We should never get here */
  close(listener);

  return 0;
}

void *process_connection(void *ptr) {
  THREAD_CTX *TTX = (THREAD_CTX *) ptr;
  char *serverpass = _ds_read_attribute(agent_config, "ServerPass");
  int tries = 0;

  TTX->packet_buffer = buffer_create(NULL);
  if (TTX->packet_buffer == NULL)
    goto CLOSE;

  TTX->authenticated = (serverpass) ? 0 : 1;

  /* Echo until authenticated */

  while(!TTX->authenticated) {
    char *input;
    struct timeval tv;

    input = socket_getline(TTX, 10);
    if (input == NULL) 
      goto CLOSE;
    else {
      char *pass = strdup(input);
      chomp(pass);
      if (!strcmp(pass, serverpass)) {
        LOGDEBUG("fd %d remote_addr %s authenticated.", TTX->sockfd, 
                  inet_ntoa(TTX->remote_addr.sin_addr));
        TTX->authenticated = 1;
      } 
      else if (send(TTX->sockfd, input, strlen(input)+1, 0)<0) {
        free(pass);
        goto CLOSE;
      }

      free(pass);
    }
      
    if (!TTX->authenticated) {

      LOGDEBUG("fd %d remote_addr %s authentication failure.", TTX->sockfd,
                inet_ntoa(TTX->remote_addr.sin_addr));
      tries++;
      if (tries>=3) {
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
        goto CLOSE;
      }
    }
  }

  /* Close connection and return */
CLOSE:
  close(TTX->sockfd);
  buffer_destroy(TTX->packet_buffer);
  free(TTX);
  return NULL;
}

char *socket_getline(THREAD_CTX *TTX, int timeout) {
  int i;
  struct timeval tv;
  fd_set fds;
  long recv_len;
  char *pop;
  char buff[1024];

  pop = pop_buffer(TTX);
  while(!pop) {
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(TTX->sockfd, &fds);
    i = select(TTX->sockfd+1, &fds, NULL, NULL, &tv);
    if (i<=0) 
      return NULL;

    recv_len = recv(TTX->sockfd, buff, sizeof(buff)-1, 0);
    buff[recv_len] = 0;
    if (recv_len == 0)
      return NULL;
    buffer_cat(TTX->packet_buffer, buff);
    pop = pop_buffer(TTX);
  }

  return pop;
}

char *pop_buffer(THREAD_CTX *TTX) {
  char *buff, *eol;
  long len;

  if (!TTX || !TTX->packet_buffer || !TTX->packet_buffer->data)
    return NULL;

  eol = strchr(TTX->packet_buffer->data, 10);
  if (!eol)
    return NULL;
  len = (eol - TTX->packet_buffer->data) + 1;
  buff = calloc(1, len+1);
  if (!buff) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }
  memcpy(buff, TTX->packet_buffer->data, len);
  memmove(TTX->packet_buffer->data, eol+1, strlen(eol+1)+1);
  TTX->packet_buffer->used -= len;
  return buff;
}

#endif /* DAEMON */
