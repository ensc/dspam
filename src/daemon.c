/* $Id: daemon.c,v 1.42 2005/02/27 20:20:04 jonz Exp $ */

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
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "daemon.h"
#include "client.h"
#include "dspam.h"
#include "libdspam.h"
#include "config.h"
#include "util.h"
#include "buffer.h"
#include "language.h"

int daemon_listen(DRIVER_CTX *DTX) {
  int port = 24;
  int queue = 32;
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
  struct timeval tv;
  THREAD_CTX *TTX = NULL;
  int fdmax;
  int listener;
  int yes = 1;
  int i, domain = 0;
  pthread_attr_t attr;
  fd_set master, read_fds;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, process_signal);
  signal(SIGTERM, process_signal);
  signal(SIGHUP, process_signal);

  /* Set Defaults */
  if (_ds_read_attribute(agent_config, "ServerPort"))
    port = atoi(_ds_read_attribute(agent_config, "ServerPort"));

  if (_ds_read_attribute(agent_config, "ServerQueueSize"))
    queue = atoi(_ds_read_attribute(agent_config, "ServerQueueSize"));

  if (_ds_read_attribute(agent_config, "ServerDomainSocketPath"))
    domain = 1;

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  if (domain) {
    struct sockaddr_un saun;
    char *address = _ds_read_attribute(agent_config, "ServerDomainSocketPath");
    int len;

    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener == -1) {
      LOG(LOG_CRIT, ERROR_DAEMON_SOCKET, strerror(errno));
      return(EFAILURE);
    }

    memset(&saun, 0, sizeof(struct sockaddr_un));
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, address);

    unlink(address);
    len = sizeof(saun.sun_family) + strlen(saun.sun_path) + 1;

    LOGDEBUG(DAEMON_DOMAIN, address);
  
    if (bind(listener, (struct sockaddr *) &saun, len)<0) {
      LOG(LOG_CRIT, ERROR_DAEMON_DOMAIN, address, strerror(errno));
      return EFAILURE;
    }    

  } else {
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
    tv.tv_usec = 0;

    if (__daemon_run == 0) {
      close(listener);

      if (_ds_read_attribute(agent_config, "ServerDomainSocketPath"))
        unlink (_ds_read_attribute(agent_config, "ServerDomainSocketPath"));
      
      return 0; 
    }

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
            } else if (!domain) {
#ifdef DEBUG
              char buff[32];
              LOGDEBUG("connection id %d from %s.", newfd, 
                       inet_ntoa_r(remote_addr.sin_addr, buff, sizeof(buff)));
#endif
            }
            fcntl(newfd, F_SETFL, O_RDWR);
            setsockopt(newfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

            TTX = calloc(1, sizeof(THREAD_CTX));
            if (TTX == NULL) {
              LOG(LOG_CRIT, ERROR_MEM_ALLOC);
              close(newfd);
              continue;
            } else {
              TTX->sockfd = newfd;
              TTX->DTX = DTX;
              memcpy(&TTX->remote_addr, &remote_addr, sizeof(remote_addr));

              inc_lock();
              if (pthread_create(&TTX->thread, 
                                 &attr, process_connection, (void *) TTX))
              {
                LOG(LOG_CRIT, ERROR_DAEMON_THREAD, strerror(errno));
                close(TTX->sockfd);
                free(TTX);
                continue;
              }
            }
          } /* if i == listener */
        } /* if FD_SET else */
      } /* for(i.. */
    }  /* if (select)... */
  } /* for(;;) */

  /* Shutdown - We should never get here */
  close(listener);

  pthread_attr_destroy(&attr);

  return 0;
}

void *process_connection(void *ptr) {
  THREAD_CTX *TTX = (THREAD_CTX *) ptr;
  AGENT_CTX *ATX = NULL;
  buffer *message = NULL;
  char *input, *cmdline = NULL, *token, *ptrptr, *oldcmd = NULL, *parms;
  char *argv[32];
  char buf[1024];
  int tries = 0;
  int argc = 0;
  int **results, *result;
  int i, locked = -1;
  FILE *fd = fdopen(TTX->sockfd, "w");
  int server_mode = SSM_DSPAM;

  if (_ds_read_attribute(agent_config, "ServerMode") &&
      !strcmp(_ds_read_attribute(agent_config, "ServerMode"), "standard"))
  {
    server_mode = SSM_STANDARD;
  }

  setbuf(fd, NULL);

  TTX->packet_buffer = buffer_create(NULL);
  if (TTX->packet_buffer == NULL)
    goto CLOSE;

  snprintf(buf, sizeof(buf), "%d DSPAM %sLMTP %s %s", 
    LMTP_GREETING, 
    (server_mode == SSM_DSPAM) ? "D" : "",
    VERSION, 
    (server_mode == SSM_DSPAM) ? "Authentication Required" : "Ready");
  if (send_socket(TTX, buf)<=0) 
    goto CLOSE;

  TTX->authenticated = 0;

  /* LHLO */

  input = daemon_expect(TTX, "LHLO");
  if (input == NULL)
    goto CLOSE;

  free(input);
  if (daemon_reply(TTX, LMTP_OK, "OK")<=0)
    goto CLOSE;

  /* Loop here */
  while(1) {

    /* Configure agent context */
    ATX = calloc(1, sizeof(AGENT_CTX));
    if (ATX == NULL) {
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      send_socket(TTX, ERROR_MEM_ALLOC); 
      goto CLOSE;
    }

    if (initialize_atx(ATX))
    {
      report_error(ERROR_INITIALIZE_ATX);
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_INITIALIZE_ATX);
      send_socket(TTX, buf);
      goto CLOSE;
    }

    /* MAIL FROM (Authentication) */

    while(!TTX->authenticated) {
      input = daemon_expect(TTX, "MAIL FROM:");
      if (input == NULL) 
        goto CLOSE;
      else {
        char *ptr, *pass, *ident;
        chomp(input);

        if (server_mode == SSM_STANDARD) {
          TTX->authenticated = 1;

          _ds_extract_address(ATX->mailfrom, input, sizeof(ATX->mailfrom));
          if (daemon_reply(TTX, LMTP_OK, "OK")<=0) {
            free(input);
            goto CLOSE;
          }
        } else {
          ptr = strtok_r(input+10, " ", &ptrptr);
          pass = strtok_r(ptr, "@", &ptrptr);
          ident = strtok_r(NULL, "@", &ptrptr);

          if (pass && ident) {
            char *serverpass;
  
            snprintf(buf, sizeof(buf), "ServerPass.%s", ident);
            serverpass = _ds_read_attribute(agent_config, buf);
  
            snprintf(buf, sizeof(buf), "ServerPass.%s", ident);
            if (serverpass && !strcmp(pass, serverpass)) {
              TTX->authenticated = 1;
              if (daemon_reply(TTX, LMTP_OK, "OK")<=0) {
                free(input);
                goto CLOSE;
              }
            } 
          }
        }

        free(input);
        if (!TTX->authenticated) {
          LOGDEBUG("fd %d authentication failure.", TTX->sockfd);

          if (daemon_reply(TTX, LMTP_AUTH_ERROR, "Authentication Required")<=0) {
            free(input);
            goto CLOSE;
          }
         
          tries++;
          if (tries>=3) {
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            select(0, NULL, NULL, NULL, &tv);
            goto CLOSE;
          }
        }
      }
    }
  
    snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);

    parms = _ds_read_attribute(agent_config, "ServerParameters");
    argc = 0;
    if (parms) {
      token = strtok_r(parms, " ", &ptrptr);
      while(token != NULL) {
        argv[argc] = token;
        argc++;
        token = strtok_r(NULL, " ", &ptrptr);
      }
    }

    /* RCPT TO (Userlist and arguments) */

    cmdline = daemon_getline(TTX, 300);
    while(cmdline && !strncasecmp(cmdline, "RCPT TO:", 8)) {

      if (server_mode == SSM_DSPAM) {
        /* Tokenize arguments */
        chomp(cmdline);
        argv[argc] = "--user";
        argc++;
        token = strtok_r(cmdline+8, " ", &ptrptr);
        while(token != NULL) {
          argv[argc] = token;
          argc++;
          token = strtok_r(NULL, " ", &ptrptr);
        }
    
      } else {
        char username[256];
  
        if (_ds_extract_address(username, cmdline, sizeof(username))) {
          snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_INVALID_RCPT);
          send_socket(TTX, buf);
          goto CLOSE;
        }
        if (!parms || !strstr(parms, "--user "))
          nt_add(ATX->users, username);
        strlcpy(ATX->recipient, username, sizeof(ATX->recipient));
      }

      snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
      if (send_socket(TTX, buf)<=0)
        goto CLOSE;

      oldcmd = cmdline;
      cmdline = daemon_getline(TTX, 300);
      if  (server_mode == SSM_DSPAM)
        break;
    }
  
    if (cmdline == NULL) 
      goto CLOSE;

    if (process_arguments(ATX, argc, argv) || apply_defaults(ATX))
    {
      report_error(ERROR_INITIALIZE_ATX);
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_INITIALIZE_ATX);
      send_socket(TTX, buf);
      goto CLOSE;
    }

    ATX->sockfd = fd;
    ATX->sockfd_output = 0;

    /* Determine which database handle to use */
    i = (TTX->sockfd % TTX->DTX->connection_cache);
    LOGDEBUG("using database handle id %d", i);
  
    pthread_mutex_lock(&TTX->DTX->connections[i]->lock);
    ATX->dbh = TTX->DTX->connections[i]->dbh;
    locked = i;

    if (check_configuration(ATX)) {
      report_error(ERROR_DSPAM_MISCONFIGURED);
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_DSPAM_MISCONFIGURED);
      send_socket(TTX, buf);
      goto CLOSE;
    }

    /* DATA */
  
    /* Check for at least one recipient */

    if (ATX->users->items == 0)
    {
      LOG (LOG_ERR, ERROR_USER_UNDEFINED);
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_USER_UNDEFINED);
      send_socket(TTX, buf);
      goto CLOSE;
    }

    if (strncasecmp(cmdline, "DATA", 4)) {
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, "Need DATA Here");
      if (send_socket(TTX, buf)<=0)
        goto CLOSE;
      input = daemon_expect(TTX, "LHLO");
      if (input == NULL)
        goto CLOSE;
    }

    cmdline = oldcmd;

    if (daemon_reply(TTX, LMTP_DATA, DAEMON_DATA)<=0)
      goto CLOSE;
  
    message = read_sock(TTX, ATX);
    if (message == NULL || message->data == NULL || message->used == 0) {
      daemon_reply(TTX, LMTP_FAILURE, ERROR_MESSAGE_NULL);
      goto CLOSE;
    }

    if (ATX->users->items == 0)
    {
      LOG (LOG_ERR, ERROR_USER_UNDEFINED);
      snprintf(buf, sizeof(buf), "%d %s", LMTP_BAD_CMD, ERROR_USER_UNDEFINED);
      send_socket(TTX, buf);
      goto CLOSE;
    }
  
    results = process_users(ATX, message);
    
    if (TTX->DTX->connections[locked]->dbh != ATX->dbh) 
      TTX->DTX->connections[locked]->dbh = ATX->dbh;

    pthread_mutex_unlock(&TTX->DTX->connections[locked]->lock);
    locked = -1;

    if (ATX->sockfd_output) {
      if (send_socket(TTX, ".")<=0)
        goto CLOSE;
    } else {
      struct nt_node *node_nt;
      struct nt_c c_nt;
      node_nt = c_nt_first(ATX->users, &c_nt);

      i = 0;

      while(node_nt != NULL) {
        int r = 0;
        result = results[i];
        if (result) r = *result; 
        if (result == NULL || r == 0)
          snprintf(buf, sizeof(buf), "%d <%s> Message accepted for delivery",
                   LMTP_OK, (char *) node_nt->ptr);
        else
          snprintf(buf, sizeof(buf), "%d <%s> Error occured during processing", LMTP_ERROR_PROCESS, (char *) node_nt->ptr);

        if (send_socket(TTX, buf)<=0) {
          for(;i<=ATX->users->items;i++)
            free(results[i]);
          free(results);
          goto CLOSE;
        }
        node_nt = c_nt_next(ATX->users, &c_nt);
        i++;
      }
    }

    fflush(fd);
    for(i=0;i<=ATX->users->items;i++)
      free(results[i]);
    free(results);

    buffer_destroy(message);
    message = NULL;
    if (ATX != NULL) {
      nt_destroy(ATX->users);
      free(ATX);
      ATX = NULL;
      free(cmdline);
      cmdline = NULL;
      TTX->authenticated = 0;
      argc = 0;
    }
  }

  /* Close connection and return */

CLOSE:
  if (locked>=0)
    pthread_mutex_unlock(&TTX->DTX->connections[locked]->lock);
  fclose(fd);
//  close(TTX->sockfd);
  buffer_destroy(TTX->packet_buffer);
  if (message) 
    buffer_destroy(message);
  if (ATX != NULL)
    nt_destroy(ATX->users);
  free(ATX);
  free(cmdline);
  free(TTX);
  dec_lock();
  pthread_exit(0);
  return 0;
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

  while ((buff = daemon_getline(TTX, 300))!=NULL) {
    chomp(buff);

    if (!strcmp(buff, ".")) {
      free(buff);
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
            char *ptrptr;
            char *z = strtok_r(y, "@", &ptrptr);
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

    free(buff);
    line++;
  }

  return NULL;

bail:
  buffer_destroy(message);
  return NULL;
}

char *daemon_expect(THREAD_CTX *TTX, const char *ptr) {
  char buf[128];
  char *cmd;

  cmd = daemon_getline(TTX, 300); 
  if (cmd == NULL)
    return NULL;

  while(strncasecmp(cmd, ptr, strlen(ptr))) {
    if (!strncasecmp(cmd, "QUIT", 4)) {
      free(cmd);
      daemon_reply(TTX, LMTP_QUIT, "OK"); 
      return NULL;
    }
    snprintf(buf, sizeof(buf), "%d Need %s here.", LMTP_BAD_CMD, ptr);
    if (send_socket(TTX, buf)<=0)
      return NULL;
    free(cmd);
    cmd = daemon_getline(TTX, 300);
    if (cmd == NULL)
      return NULL;
  } 
  return cmd;
}

int daemon_reply(THREAD_CTX *TTX, int reply, const char *txt) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d %s", reply, txt);
  return send_socket(TTX, buf);
}

void process_signal(int sig) {
  __daemon_run = 0;
  if (sig == SIGHUP) {
    LOGDEBUG("reloading");
    __hup = 1;
  } else {
    LOGDEBUG("terminating on signal %d", sig);
    __hup = 0;
  } 

  return;
}

char *daemon_getline(THREAD_CTX *TTX, int timeout) {
  int i;
  struct timeval tv;
  fd_set fds;
  long recv_len;
  char *pop;
  char buff[1024];
  int total_wait = 0;

  pop = pop_buffer(TTX);
  while(!pop && total_wait<timeout) {
    if (__daemon_run == 0) 
      return NULL;
    total_wait++;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(TTX->sockfd, &fds);
    i = select(TTX->sockfd+1, &fds, NULL, NULL, &tv);
    if (i<=0)
      continue;

    recv_len = recv(TTX->sockfd, buff, sizeof(buff)-1, 0);
    buff[recv_len] = 0;
    if (recv_len == 0)
      return NULL;
    buffer_cat(TTX->packet_buffer, buff);
    pop = pop_buffer(TTX);
  }

#ifdef VERBOSE
  LOGDEBUG("SRECV: %s", pop);
#endif
  return pop;
}

void inc_lock(void) {
  pthread_mutex_lock(&__lock);
  __num_threads++;
  pthread_mutex_unlock(&__lock);
}

void dec_lock(void) {
  pthread_mutex_lock(&__lock);
  __num_threads--;
  pthread_mutex_unlock(&__lock);
}

#endif
