/* $Id: daemon.c,v 1.78 2005/03/19 19:43:29 jonz Exp $ */

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

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#ifdef DAEMON

#define RSET(A)	( A && !strcmp(A, "RSET") )

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

/*

 daemon_listen: primary server listen loop

 This function is called by the main agent loop, and is responsible for
 initializing and managing core daemon services which include listening for
 and accepting incoming connections and spawning new protocol handler threads.

*/

int daemon_listen(DRIVER_CTX *DTX) {
  int port = 24;                 /* Default TCP port */
  int queue = 32;                /* Default connection queue size */
  THREAD_CTX *TTX = NULL;        /* Thread context created for new threads */
  struct sockaddr_in local_addr;
  struct sockaddr_in remote_addr;
  struct timeval tv;
  int fdmax;
  int listener;
  int i, domain = 0, yes = 1;
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

  /* Initialize */

  FD_ZERO(&master);
  FD_ZERO(&read_fds);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* Bind to a domain socket */

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
      close(listener);
      LOG(LOG_CRIT, ERROR_DAEMON_DOMAIN, address, strerror(errno));
      return EFAILURE;
    }    

  /* Bind to a TCP socket */

  } else {
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
      LOG(LOG_CRIT, ERROR_DAEMON_SOCKET, strerror(errno));
      return(EFAILURE);
    }

    if (setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
      close(listener);
      LOG(LOG_CRIT, ERROR_DAEMON_SOCKOPT, "SO_REUSEADDR", strerror(errno));
      return(EFAILURE);
    }

    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    LOGDEBUG(DAEMON_BINDING, port);

    if (bind(listener, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) == -1) {
      close(listener);
      LOG(LOG_CRIT, ERROR_DAEMON_BIND, port, strerror(errno));
      return(EFAILURE);
    }
  }

  /* Listen */

  if (listen(listener, queue) == -1) {
    close(listener);
    LOG(LOG_CRIT, ERROR_DAEMON_LISTEN, strerror(errno));
    return(EFAILURE);
  }

  FD_SET(listener, &master);
  fdmax = listener;

  /* Process new connections (until death or reload) */

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
              continue;
#ifdef DEBUG
            } else if (!domain) {
              char buff[32];
              LOGDEBUG("connection id %d from %s.", newfd, 
                       inet_ntoa_r(remote_addr.sin_addr, buff, sizeof(buff)));
#endif
            }
            fcntl(newfd, F_SETFL, O_RDWR);
            setsockopt(newfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

            /* Each new connection gets its own thread, so we create a new
               thread context and send it on its way */

            TTX = calloc(1, sizeof(THREAD_CTX));
            if (TTX == NULL) {
              LOG(LOG_CRIT, ERROR_MEM_ALLOC);
              close(newfd);
              continue;
            } else {
              TTX->sockfd = newfd;
              TTX->DTX = DTX;
              memcpy(&TTX->remote_addr, &remote_addr, sizeof(remote_addr));

              increment_thread_count();
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

/*

 process_connection: server process thread

 This function instantiates for each thread at the beginning of a connection
 and is the springboard for all server communication and functions. The
 server operates in one of three modes:

 SSM_DSPAM	DSPAM's proprietary DLMTP protocol
 This server mode performs authentication against a set of configured relays
 in dspam.conf and allows for trusted user arguments to be passed in. This is
 the protocol used when dspamc (or dspam --client) is communicating with the
 server.

 SSM_STANDARD	Standard LMTP protocol
 This server mode attempts to emulate standard LMTP protocol as per RFC2033 and
 is designed to communicate with standard LMTP clients such as mail servers
 or other tools. Because the client doesn't authenticate, it is assumed
 "untrusted", therefore default parameters are set in dspam.conf using the
 ServerParameters property. 

 SSM_AUTO	Auto-detect the native client protocol
 Use of SSM_AUTO allows the server process to speak both DLMTP and LMTP 
 protocols, and auto-detects based on the ident provided with the client LHLO.
 If the ident matches a relay named in dspam.conf, DLMTP is assumed. For this
 reason, authenticated DLMTP relays should NOT be assigned an ident matching
 their hostname.

*/

void *process_connection(void *ptr) {
  THREAD_CTX *TTX = (THREAD_CTX *) ptr;
  AGENT_CTX *ATX = NULL;
  buffer *message = NULL;
  char *input, *cmdline = NULL, *token, *ptrptr;
  char *oldcmd = NULL, *parms=NULL, *p=NULL;
  char *server_ident = _ds_read_attribute(agent_config, "ServerIdent");
  char *argv[64];
  char buf[1024];
  int tries = 0;
  int argc = 0;
  int i, locked = -1, invalid = 0;
  FILE *fd = 0;
  int server_mode = SSM_DSPAM;

  /* Set defaults */

  if (_ds_read_attribute(agent_config, "ServerMode") &&
      !strcasecmp(_ds_read_attribute(agent_config, "ServerMode"), "standard"))
  {
    server_mode = SSM_STANDARD;
  }

  if (_ds_read_attribute(agent_config, "ServerMode") &&
      !strcasecmp(_ds_read_attribute(agent_config, "ServerMode"), "auto"))
  {
    server_mode = SSM_AUTO;
  }

  /* Initialize */
  fd = fdopen(TTX->sockfd, "w");
  if (!fd) {
    close(TTX->sockfd);
    goto CLOSE;
  }

  setbuf(fd, NULL);

  TTX->packet_buffer = buffer_create(NULL);
  if (TTX->packet_buffer == NULL)
    goto CLOSE;

  /* Send greeting banner

     NOTE: In auto mode, look like a regular LMTP server so we don't cause
           any compatibility problems
  */

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
  if (server_mode == SSM_AUTO && input[4]) {
    char buff[128];

    /* Auto-detect */
    snprintf(buff, sizeof(buff), "ServerPass.%s", input + 5);
    chomp(buff);
    if (_ds_read_attribute(agent_config, buff))
      server_mode = SSM_DSPAM;
    else
      server_mode = SSM_STANDARD;
  }

  free(input);

  /* LHLO Response */
  if (daemon_extension(TTX, (server_ident) ? server_ident : 
                                             "localhost.localdomain")<=0)
    goto CLOSE;

  if (daemon_extension(TTX, "PIPELINING")<=0)
    goto CLOSE;

  if (daemon_extension(TTX, "ENHANCEDSTATUSCODES")<=0)
    goto CLOSE;

  if (server_mode == SSM_DSPAM)
    if (daemon_extension(TTX, "DSPAMPROCESSMODE")<=0)
      goto CLOSE;

  if (daemon_reply(TTX, LMTP_OK, "", "SIZE")<=0)
    goto CLOSE;

  /* Loop until QUIT or disconnect */
  while(1) {
    char processmode[256];
    parms = NULL;

    /* Configure a new agent context for each pass */
    ATX = calloc(1, sizeof(AGENT_CTX));
    if (ATX == NULL) {
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      daemon_reply(TTX, LMTP_ERROR_PROCESS, "5.3.0", ERROR_MEM_ALLOC);
      goto CLOSE;
    }

    if (initialize_atx(ATX)) {
      report_error(ERROR_INITIALIZE_ATX);
      daemon_reply(TTX, LMTP_BAD_CMD, "5.3.0", ERROR_INITIALIZE_ATX);
      goto CLOSE;
    }

    /* MAIL FROM (and authentication, if SSM_DSPAM) */

    processmode[0] = 0;
    while(!TTX->authenticated) {
      input = daemon_expect(TTX, "MAIL FROM");

      if (RSET(input))
        goto RSET;

      if (input == NULL) 
        goto CLOSE;
      else {
        char *pass, *ident;
        chomp(input);

        if (server_mode == SSM_STANDARD) {
          TTX->authenticated = 1;

          ATX->mailfrom[0] = 0;
          _ds_extract_address(ATX->mailfrom, input, sizeof(ATX->mailfrom));

          if (daemon_reply(TTX, LMTP_OK, "2.1.0", "OK")<=0) {
            free(input);
            goto CLOSE;
          }
        } else {
          char id[256];
          pass = ident = NULL;
          id[0] = 0;
          if (!_ds_extract_address(id, input, sizeof(id))) {
            pass = strtok_r(id, "@", &ptrptr);
            ident = strtok_r(NULL, "@", &ptrptr);
          }

          if (pass && ident) {
            char *serverpass;
            char *ptr, *ptr2, *ptr3;
  
            snprintf(buf, sizeof(buf), "ServerPass.%s", ident);
            serverpass = _ds_read_attribute(agent_config, buf);
  
            snprintf(buf, sizeof(buf), "ServerPass.%s", ident);
            if (serverpass && !strcmp(pass, serverpass)) {
              TTX->authenticated = 1;

              /* Parse for PROCESSMODE */
 
              ptr = strstr(input, "DSPAMPROCESSMODE=\"");
              if (ptr) {
                char *mode;
                int i;
                ptr2 = strchr(ptr, '"')+1;
                mode = ptr2;
                while((ptr3 = strstr(ptr2, "\\\""))) 
                  ptr2 = ptr3+2;
                ptr3 = strchr(ptr2+2, '"');
                if (ptr3)
                  ptr3[0] = 0;
                strlcpy(processmode, mode, sizeof(processmode)); 
                
                ptr = processmode;
                for(i=0;ptr[i];i++) {
                  if (ptr[i] == '\\' && ptr[i+1] == '"') {
                    strcpy(ptr+i, ptr+i+1);
                  }
                }
                LOGDEBUG("process mode: '%s'", processmode);
              }
                
              if (daemon_reply(TTX, LMTP_OK, "2.1.0", "OK")<=0) {
                free(input);
                goto CLOSE;
              }
            } 
          }
        }

        free(input);
        if (!TTX->authenticated) {
          LOGDEBUG("fd %d authentication failure.", TTX->sockfd);

          if (daemon_reply(TTX, LMTP_AUTH_ERROR, "5.1.0", "Authentication Required")<=0) {
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
  
    /* MAIL FROM Response */
    snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);

    argc = 1;
    argv[0] = "dspam";
    argv[1] = 0;

    /* Load open-LMTP parameters from config */
    if (server_mode == SSM_STANDARD) {
      parms = _ds_read_attribute(agent_config, "ServerParameters");
      if (parms) {
        p = strdup(parms);
        if (p) {
          token = strtok_r(p, " ", &ptrptr);
          while(token != NULL && argc<63) {
            argv[argc] = token;
            argc++;
            argv[argc] = 0;
            token = strtok_r(NULL, " ", &ptrptr);
          }
        }
      }
    }

    /* RCPT TO */

    while(ATX->users->items == 0 || invalid) {
      cmdline = daemon_getline(TTX, 300);
 
      while(cmdline && (!strncasecmp(cmdline, "RCPT TO:", 8) || (!strncasecmp(cmdline, "RSET", 4)))) {

        if (!strncasecmp(cmdline, "RSET", 4)) {
          snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
          if (send_socket(TTX, buf)<=0)
            goto CLOSE; 
          goto RSET;
        }

        if (server_mode == SSM_DSPAM) {
          char username[256];
   
          if (_ds_extract_address(username, cmdline, sizeof(username)) ||
              username[0] == 0 || username[0] == '-')
          {
            daemon_reply(TTX, LMTP_BAD_CMD, "5.1.2", ERROR_INVALID_RCPT);
            goto GETCMD;
//            break;
          }

          nt_add(ATX->users, username);

        } else {
          char username[256];
    
          if (_ds_extract_address(username, cmdline, sizeof(username)) ||
              username[0] == 0 || username[0] == '-') 
          {
            daemon_reply(TTX, LMTP_BAD_CMD, "5.1.2", ERROR_INVALID_RCPT);
            goto GETCMD;
            break;
          }

          if (!parms || !strstr(parms, "--user ")) 
            nt_add(ATX->users, username);

          if (!ATX->recipients) {
            ATX->recipients = nt_create(NT_CHAR);
            if (ATX->recipients == NULL) {
              report_error(ERROR_MEM_ALLOC);
              goto CLOSE;
            }
          }
          nt_add(ATX->recipients, username);
        }

        if (daemon_reply(TTX, LMTP_OK, "2.1.5", "OK")<=0)
          goto CLOSE;

GETCMD:
        oldcmd = cmdline;
        cmdline = daemon_getline(TTX, 300);
//        if  (server_mode == SSM_DSPAM)
//          break;
      }

      if (cmdline == NULL)
        goto CLOSE;

      if (!strncasecmp(cmdline, "RSET", 4)) {
        snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
        if (send_socket(TTX, buf)<=0)
          goto CLOSE;
        goto RSET;
      }

      if (server_mode == SSM_DSPAM && processmode[0] != 0) {
        token = strtok_r(processmode, " ", &ptrptr);
        while(token != NULL && argc<63) {
          argv[argc] = token;
          argc++;
          argv[argc] = 0;
          token = strtok_r(NULL, " ", &ptrptr);
        }
      }

      if (!strncasecmp(cmdline, "quit", 4)) {
        daemon_reply(TTX, LMTP_OK, "2.0.0", "OK");
        goto CLOSE;
      }

      invalid = 0;
      if (process_arguments(ATX, argc, argv) || apply_defaults(ATX))
      {
        report_error(ERROR_INITIALIZE_ATX);
        daemon_reply(TTX, LMTP_NO_RCPT, "5.1.0", ERROR_INITIALIZE_ATX);
        invalid = 1;
      } else if (ATX->users->items == 0) {
        daemon_reply(TTX, LMTP_NO_RCPT, "5.1.1", ERROR_USER_UNDEFINED);
      }
    }

    ATX->sockfd = fd;
    ATX->sockfd_output = 0;

    /* Kaboom! */

    if (check_configuration(ATX)) {
      report_error(ERROR_DSPAM_MISCONFIGURED);
      daemon_reply(TTX, LMTP_BAD_CMD, "5.3.5", ERROR_DSPAM_MISCONFIGURED);
      goto CLOSE;
    }

    /* DATA */
  
    if (strncasecmp(cmdline, "DATA", 4)) {
      if (daemon_reply(TTX, LMTP_BAD_CMD, "5.0.0", "Need DATA Here")<0) 
        goto CLOSE;
      input = daemon_expect(TTX, "DATA");
      if (input == NULL)
        goto CLOSE;
      if (RSET(input)) 
        goto RSET;
    }

    cmdline = oldcmd;

    if (daemon_reply(TTX, LMTP_DATA, "", DAEMON_DATA)<=0)
      goto CLOSE;
  
    /*
       Read in the message from a DATA. I personally like to just hang up on
       a client stupid enough to pass in a NULL message for DATA, but you're 
       welcome to do whatever you want. 
    */

    message = read_sock(TTX, ATX);
    if (message == NULL || message->data == NULL || message->used == 0) {
      daemon_reply(TTX, LMTP_FAILURE, "5.2.0", ERROR_MESSAGE_NULL);
      goto CLOSE;
    }

    /*
       Lock a database handle. We currently use the modulus of the
       socket id against the number of database connections in the cache.
       This seems to work rather well, as we would need to lock up the entire
       cache to wrap back around. And if we do wrap back around, that means
       we're busy enough to justify spinning on the current lock (vs. seeking
       out a free handle, which there likely are none).
    */

    i = (TTX->sockfd % TTX->DTX->connection_cache);
    LOGDEBUG("using database handle id %d", i);
    pthread_mutex_lock(&TTX->DTX->connections[i]->lock);
    ATX->dbh = TTX->DTX->connections[i]->dbh;
    locked = i;

    /* Process the message using the existing agent functions */
    ATX->results = nt_create(NT_PTR);
    if (ATX->results == NULL) {
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      goto CLOSE;
    } 
    process_users(ATX, message);
  
    /*
       Unlock the database handle as soon as we're done. We also need to
       refresh our handle index with a new handle if for some reason we
       had to re-establish a dewedged connection.
    */
    if (TTX->DTX->connections[locked]->dbh != ATX->dbh) 
      TTX->DTX->connections[locked]->dbh = ATX->dbh;

    pthread_mutex_unlock(&TTX->DTX->connections[locked]->lock);
    locked = -1;

    /* Send a terminating '.' if --stdout in DLMTP mode */
    if (ATX->sockfd_output) {
      if (send_socket(TTX, ".")<=0)
        goto CLOSE;

    /* Otherwise, produce standard delivery results */
    } else {
      struct nt_node *node_nt, *node_res = NULL;
      struct nt_c c_nt;
      if (ATX->recipients)
        node_nt = c_nt_first(ATX->recipients, &c_nt);
      else
        node_nt = c_nt_next(ATX->users, &c_nt);

      if (ATX->results)
        node_res = ATX->results->first;

      while(node_nt != NULL) {
        agent_result_t result = (agent_result_t) node_res->ptr;

        if (result != NULL && result->exitcode == ERC_SUCCESS)
        {
          if (server_mode == SSM_DSPAM) {
            snprintf(buf, sizeof(buf),
                    "%d 2.6.0 <%s> Message accepted for delivery: %s",
                    LMTP_OK, (char *) node_nt->ptr, 
              (result->classification == DSR_ISSPAM) ? "SPAM" : "INNOCENT");
          } else {
            snprintf(buf, sizeof(buf),
                    "%d 2.6.0 <%s> Message accepted for delivery",
                    LMTP_OK, (char *) node_nt->ptr);
          }
        } 
        else
        {
          snprintf(buf, sizeof(buf),
                   "%d 5.3.0 <%s> Error occured during %s", 
                   LMTP_ERROR_PROCESS, (char *) node_nt->ptr,
            (result->exitcode == ERC_DELIVERY) ? "delivery" : "processing");
        }

        if (send_socket(TTX, buf)<=0) 
          goto CLOSE;
        if (ATX->recipients)
          node_nt = c_nt_next(ATX->recipients, &c_nt);
        else
          node_nt = c_nt_next(ATX->users, &c_nt);
        if (node_res)
          node_res = node_res->next;
      }
    }

    /* Cleanup and get ready for another message */

RSET:
    fflush(fd);

    buffer_destroy(message);
    message = NULL;
    if (ATX != NULL) {
      nt_destroy(ATX->users);
      nt_destroy(ATX->recipients);
      nt_destroy(ATX->results);
      free(ATX);
      ATX = NULL;
      free(cmdline);
      cmdline = NULL;
      TTX->authenticated = 0;
      argc = 0;
    }

    free(p);
    p = NULL;

  } /* while(1) */

  /* Close connection and return */

CLOSE:
  if (locked>=0)
    pthread_mutex_unlock(&TTX->DTX->connections[locked]->lock);
  if (fd)
    fclose(fd);
  buffer_destroy(TTX->packet_buffer);
  if (message) 
    buffer_destroy(message);
  if (ATX != NULL) {
    nt_destroy(ATX->users);
    nt_destroy(ATX->recipients);
    nt_destroy(ATX->results);
  }
  free(ATX);
  free(cmdline);
  free(TTX);
  decrement_thread_count();
  pthread_exit(0);
  return 0;
}

/*

 read_sock: read a message via socket

 This is a dupe of read_stdin, only geared around a server socket, with
 timeouts and other types of checks written in. 

*/

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
        if (!body && !strncasecmp(buff, "To: ", 4))
        {
          char *y = NULL;
          char *x;

          /* Check for spam- */

          x = strstr(buff, "<spam-");
          if (!x)
            x = strstr(buff, " spam-");
          if (!x)
            x = strstr(buff, ":spam-");

          if (x != NULL) {
            y = strdup(x+6);

            if (_ds_match_attribute(agent_config, "ChangeModeOnParse", "on"))
            {
              ATX->classification = DSR_ISSPAM;
              ATX->source = DSS_ERROR;
            }
          } else {

            /* Check for notspam- */

            x = strstr(buff, "<notspam-");
            if (!x)
              x = strstr(buff, " notspam-");
            if (!x)
              x = strstr(buff, ":notspam-");

            if (x != NULL) {
              y = strdup(x+9);

              if (_ds_match_attribute(agent_config, "ChangeModeOnParse", "on"))
              {
                ATX->classification = DSR_ISINNOCENT;
                ATX->source = DSS_ERROR;
              }
            }
          }

          if (y && _ds_match_attribute(agent_config,
                                       "ChangeUserOnParse", "on"))
          {
            char *ptrptr;
            char *z = strtok_r(y, "@", &ptrptr);
            nt_destroy(ATX->users);
            ATX->users = nt_create(NT_CHAR);
            if (!ATX->users)
              return NULL;
            nt_add (ATX->users, z);
            free(y);
          }
        } /* To: Header */
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

/*

 daemon_expect: Wait for the right command to be issued by the client.

 Essentially, we keep throwing out errors until we get the command we want and
 then we return the commandline. This helps to enforce syntax.

*/
 
char *daemon_expect(THREAD_CTX *TTX, const char *ptr) {
  char buf[128];
  char *cmd;

  cmd = daemon_getline(TTX, 300); 
  if (cmd == NULL)
    return NULL;

  while(strncasecmp(cmd, ptr, strlen(ptr))) {
    if (!strncasecmp(cmd, "QUIT", 4)) {
      free(cmd);
      daemon_reply(TTX, LMTP_QUIT, "2.0.0", "OK"); 
      return NULL;
    }
    if (!strncasecmp(cmd, "RSET", 4)) { 
      snprintf(buf, sizeof(buf), "%d OK", LMTP_OK);
      if (send_socket(TTX, buf)<=0)
        return NULL;
      free(cmd);
      return "RSET";
    }

    snprintf(buf, sizeof(buf), "%d 5.0.0 Need %s here.", LMTP_BAD_CMD, ptr);

    if (send_socket(TTX, buf)<=0)
      return NULL;
    free(cmd);
    cmd = daemon_getline(TTX, 300);
    if (cmd == NULL)
      return NULL;
  } 
  return cmd;
}

/*

 daemon_reply: Send a response code, enhanced status code, and text response

 A very simple subroutine to send responses to the client.

*/

int daemon_reply(THREAD_CTX *TTX, int reply, const char *ecode, const char *txt)
{
  char buf[128];
  snprintf(buf, sizeof(buf), "%d %s%s%s", 
           reply, ecode, (ecode[0]) ? " " : "", txt);
  return send_socket(TTX, buf);
}

/*

 daemon_extension: Advertise supported extensions

 Send a 250 with a dash instead of a space to advertise LMTP extensions

*/
 
int daemon_extension(THREAD_CTX *TTX, const char *extension) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d-%s", LMTP_OK, extension);
  return send_socket(TTX, buf);
}

/* process_signal: reload or die!  */

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

/*

 daemon_getline: read a line of text from the socket buffer

 Waits for a full line of text to appear in the buffer and then reeturns
 the entire line. If no complete line is received within the timeout
 specified, return NULL.

*/

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

/*

 increment/decrement _thread_count: keep track of running threads

 before we can reload or quit, we have to wait for all threads to exit. these
 functions are called whenever a thread spawns or ends, and bumps the counter.

*/

void increment_thread_count(void) {
  pthread_mutex_lock(&__lock);
  __num_threads++;
  pthread_mutex_unlock(&__lock);
}

void decrement_thread_count(void) {
  pthread_mutex_lock(&__lock);
  __num_threads--;
  pthread_mutex_unlock(&__lock);
}

#endif
