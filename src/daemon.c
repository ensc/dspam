/* $Id: daemon.c,v 1.10 2004/12/01 00:32:15 jonz Exp $ */

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

    tv.tv_sec = 2;
    tv.tv_usec = 1000;
// HERE

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
                fcntl(newfd, F_SETFL, O_RDWR);
//              fcntl(newfd, F_SETFL, O_NONBLOCK);
//              setsockopt(newfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

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
  AGENT_CTX *ATX = NULL;
  buffer *message = NULL;
  char *serverpass = _ds_read_attribute(agent_config, "ServerPass");
  char *input, *cmdline = NULL, *token, *ptrptr;
  char *argv[32];
  char buf[1024];
  int tries = 0;
  int argc = 0;
  int exitcode;

  TTX->packet_buffer = buffer_create(NULL);
  if (TTX->packet_buffer == NULL)
    goto CLOSE;

  snprintf(buf, sizeof(buf), "%d DSPAM LMTP %s Authentication Required", LMTP_GREETING, VERSION);
  if (socket_send(TTX, buf)<=0) 
    goto CLOSE;

  TTX->authenticated = 0;

  /* Echo until authenticated */

  while(!TTX->authenticated) {
    struct timeval tv;

    input = socket_getline(TTX, 300);
    if (input == NULL) 
      goto CLOSE;
    else {
      char *t, *pass;
      chomp(input);

      t = strtok_r(input, " ", &ptrptr);
      if (t) {
        if (!strncasecmp(t, "QUIT", 4)) {
          snprintf(buf, sizeof(buf), "%d OK", LMTP_QUIT);
          socket_send(TTX, buf);
          goto CLOSE;
        }

        pass = strtok_r(NULL, " ", &ptrptr);
      }
      if (t == NULL || strcasecmp(t, "LHLO") || (serverpass && (!pass || strcmp(pass, serverpass)))) {
        snprintf(buf, sizeof(buf), "%d Authentication Required", LMTP_AUTH_ERROR);
        if (socket_send(TTX, buf)<=0)
          goto CLOSE;
      } else {
        LOGDEBUG("fd %d remote_addr %s authenticated.", TTX->sockfd, 
                  inet_ntoa(TTX->remote_addr.sin_addr));
        TTX->authenticated = 1;
        snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
        if (socket_send(TTX, buf)<=0)
          goto CLOSE;
      } 
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

  snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);

  /* Client is authenticated, receive parameters */
  cmdline = socket_getline(TTX, 300);
  if (cmdline == NULL)
    goto CLOSE;

  while(strncasecmp(cmdline, "MAIL FROM:", 10)) {
    if (!strncasecmp(cmdline, "QUIT", 4)) {
      snprintf(buf, sizeof(buf), "%d OK", LMTP_QUIT);
      socket_send(TTX, buf);
    }
    if (cmdline == NULL || !strncasecmp(cmdline, "QUIT", 4))
      goto CLOSE;
    snprintf(buf, sizeof(buf), "%d Need MAIL here", LMTP_BAD_CMD);
    if (socket_send(TTX, buf)<=0)
      goto CLOSE;
    cmdline = socket_getline(TTX, 300);
  }

  chomp(cmdline);
  /* Tokenize arguments */
  token = strtok_r(cmdline+10, " ", &ptrptr);
  while(token != NULL) {
    argv[argc] = token;
    argc++;
    token = strtok_r(NULL, " ", &ptrptr);
  }

  /* Configure agent context */
  ATX = calloc(1, sizeof(AGENT_CTX));
  if (ATX == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    socket_send(TTX, ERROR_MEM_ALLOC);
    goto CLOSE;
  }

  if (initialize_atx(ATX) || process_arguments(ATX, argc, argv) ||
      apply_defaults(ATX))
  {
    report_error(ERROR_INITIALIZE_ATX);
    snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_INITIALIZE_ATX);
    socket_send(TTX, buf);
    goto CLOSE;
  } 

  ATX->sockfd = TTX->sockfd;
  ATX->sockfd_output = 0;

  /* Get recipients */
  snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
  if (socket_send(TTX, buf)<=0)
    goto CLOSE;
 
  cmdline = socket_getline(TTX, 300);
  if (cmdline == NULL)
    goto CLOSE;

  while(strncasecmp(cmdline, "RCPT TO:", 8)) {
    if (!strncasecmp(cmdline, "QUIT", 4)) {
      snprintf(buf, sizeof(buf), "%d OK", LMTP_QUIT);
      socket_send(TTX, buf);
    }
    if (cmdline == NULL || !strncasecmp(cmdline, "QUIT", 4))
      goto CLOSE;
    snprintf(buf, sizeof(buf), "%d Need RCPT here", LMTP_BAD_CMD);
    if (socket_send(TTX, buf)<=0)
      goto CLOSE;
    cmdline = socket_getline(TTX, 300);
  }

  /* Tokenize arguments */
  chomp(cmdline);
  token = strtok_r(cmdline+8, " ", &ptrptr);
  while(token != NULL) {
    nt_add(ATX->users, token);
    token = strtok_r(NULL, " ", &ptrptr);
  }

  if (check_configuration(ATX)) {
    report_error(ERROR_DSPAM_MISCONFIGURED);
    snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_DSPAM_MISCONFIGURED);
    socket_send(TTX, buf);
    goto CLOSE;
  }

  /* Get DATA */
  snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
  if (socket_send(TTX, buf)<=0)
    goto CLOSE;

  cmdline = socket_getline(TTX, 300);
  if (cmdline == NULL)
    goto CLOSE;

  while(strncasecmp(cmdline, "DATA", 4)) {
    if (!strncasecmp(cmdline, "QUIT", 4)) {
      snprintf(buf, sizeof(buf), "%d OK", LMTP_QUIT);
      socket_send(TTX, buf);
    }
    if (cmdline == NULL || !strncasecmp(cmdline, "QUIT", 4))
      goto CLOSE;
    snprintf(buf, sizeof(buf), "%d Need RCPT here", LMTP_BAD_CMD);
    if (socket_send(TTX, buf)<=0)
      goto CLOSE;
    cmdline = socket_getline(TTX, 300);
  }

  snprintf(buf, sizeof(buf), "%d Enter mail, end with \".\" on a line by itself", LMTP_DATA);
  if (socket_send(TTX, buf)<=0)
    goto CLOSE;

  message = read_sock(TTX, ATX);

  if (ATX->users->items == 0)
  {
    LOG (LOG_ERR, ERROR_USER_UNDEFINED);
    snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_USER_UNDEFINED);
    socket_send(TTX, buf);
    goto CLOSE;
  }

  exitcode = process_users(ATX, message);
  LOGDEBUG("process_users() returned with exit code %d", exitcode);

  if (!exitcode) {
    if (! ATX->sockfd_output)
      snprintf(buf, sizeof(buf), "%d 2.5.0 Message accepted for delivery", LMTP_OK);
    else
     strcpy(buf, ".");
  } else {
    snprintf(buf, sizeof(buf), "%d Error occured during processing.", LMTP_ERROR);
  }
  socket_send(TTX, buf);

  /* Close connection and return */

CLOSE:
  close(TTX->sockfd);
  buffer_destroy(TTX->packet_buffer);
  if (message) 
    buffer_destroy(message);
  free(TTX);
  if (ATX != NULL)
    nt_destroy(ATX->users);
  free(ATX);
  free(cmdline);
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

int socket_send(THREAD_CTX *TTX, const char *ptr) {
  int i;

  i = send(TTX->sockfd, ptr, strlen(ptr), 0);
  if (i>0) 
    i = send(TTX->sockfd, "\n", 2, 0);
  return i;
}

buffer * read_sock(THREAD_CTX *TTX, AGENT_CTX *ATX) {
  char *buff;
  buffer *message;
  int body = 0, line = 1;

  message = buffer_create(NULL);
  if (message == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  while ((buff = socket_getline(TTX, 300))!=NULL) {
    chomp(buff);
    if (!strcmp(buff, ".")) {
      return message;
    }

    if (_ds_match_attribute(agent_config, "Broken", "lineStripping")) {
      size_t len = strlen(buff);
  
      while (len>1 && buff[len-2]==13) {
        buff[len-2] = buff[len-1];
        buff[len-1] = 0;
        len--;
      }
    }
  
    if (line > 1 || strncmp (buff, "From QUARANTINE", 15))
    {
      if (_ds_match_attribute(agent_config, "ParseToHeaders", "on")) {

        /* Parse the To: address for a username */
        if (buff[0] == 0)
          body = 1;
        if (ATX->classification != -1 && !body && 
            !strncasecmp(buff, "To: ", 4))
        {
          char *y = NULL;
          if (ATX->classification == DSR_ISSPAM) {
            char *x = strstr(buff, "spam-");
            if (x != NULL) {
              y = strdup(x+5);
            }
          } else {
            char *x = strstr(buff, "fp-");
            if (x != NULL) {
              y = strdup(x+3);
            }
          }
  
          if (y) {
            char *z = strtok(y, "@");
            nt_add (ATX->users, z);
            free(y);
          }
        }
      }
 
      if (buffer_cat (message, buff) || buffer_cat(message, "\n"))
      {
        LOG (LOG_CRIT, ERROR_MEM_ALLOC);
        goto bail;
      }
    }
  
    /* Use the original user id if we are reversing a false positive */
    if (!strncasecmp (buff, "X-DSPAM-User: ", 14) && 
        ATX->managed_group[0] != 0                 &&
        ATX->operating_mode == DSM_PROCESS    &&
        ATX->classification == DSR_ISINNOCENT &&
        ATX->source         == DSS_ERROR)
    {
      char user[MAX_USERNAME_LENGTH];
      strlcpy (user, buff + 14, sizeof (user));
      chomp (user);
      nt_destroy (ATX->users);
      ATX->users = nt_create (NT_CHAR);
      if (ATX->users == NULL)
      {
        report_error (ERROR_MEM_ALLOC);
        goto bail;
      }
      nt_add (ATX->users, user);
    }

    line++;
  }

  return NULL;

bail:
  buffer_destroy(message);
  return NULL;
}

#endif
