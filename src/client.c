/* $Id: client.c,v 1.38 2005/03/15 14:06:32 jonz Exp $ */

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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
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

#include "client.h"
#include "libdspam.h"
#include "dspam.h"
#include "config.h"
#include "util.h"
#include "language.h"
#include "buffer.h"

/*

 client_process: process as a DLMTP client

 This function is called by the DSPAM agent in lieu of standard processing
 when --client is specified. It passes off all responsibility for processing 
 to the configured server and returns the appropriate response. This should
 be ultimately transparent to any tools outside of the agent process.

*/

int client_process(AGENT_CTX *ATX, buffer *message) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  THREAD_CTX TTX;	/* Needed for compatibility */
  char buff[1024];
  int exitcode = 0, i, msglen;

  TTX.sockfd = client_connect(0);
  if (TTX.sockfd <0) {
    report_error(ERROR_CLIENT_CONNECT);
    return TTX.sockfd;
  }

  TTX.packet_buffer = buffer_create(NULL);
  if (TTX.packet_buffer == NULL) 
    goto BAIL;

  /* LHLO and MAIL FROM - Authenticate on the server */
  /* ----------------------------------------------- */

  if (client_authenticate(&TTX)<0) {
    report_error(ERROR_CLIENT_AUTH_FAILED);
    goto QUIT;
  }

  /* RCPT TO - Send recipient information */
  /* ------------------------------------ */

  strcpy(buff, "RCPT TO: ");
  node_nt = c_nt_first(ATX->users, &c_nt);
  while(node_nt != NULL) {
    strlcat(buff, (const char *) node_nt->ptr, sizeof(buff));
    strlcat(buff, " ", sizeof(buff));
    node_nt = c_nt_next(ATX->users, &c_nt);
  }
  strlcat(buff, ATX->client_args, sizeof(buff));

  if (send_socket(&TTX, buff)<=0) 
    goto BAIL;

  if (client_getcode(&TTX)!=LMTP_OK) 
    goto QUIT;

  /* DATA - Send message */
  /* ------------------- */

  if (send_socket(&TTX, "DATA")<=0) 
    goto BAIL;

  if (client_getcode(&TTX)!=LMTP_DATA)
    goto QUIT;

  i = 0;
  msglen = strlen(message->data);
  while(i<msglen) {
    int r = send(TTX.sockfd, message->data+i, msglen - i, 0);
    if (r <= 0) 
      goto BAIL;
    i += r;
  }

  if (message->data[strlen(message->data)-1]!= '\n')
    if (send_socket(&TTX, "")<=0)
     goto BAIL;

  if (send_socket(&TTX, ".")<=0)
    goto BAIL;

  /* Server Response */
  /* --------------- */

  if (ATX->flags & DAF_STDOUT || ATX->operating_mode == DSM_CLASSIFY) {
    char *line = NULL;

    line = client_getline(&TTX, 300);
    if (line)
      chomp(line);

    while(line != NULL && strcmp(line, ".")) {
      chomp(line);
      printf("%s\n", line);
      if (strstr(line, "250")) {
        free(line); 
        goto QUIT; 
      }
      free(line);
      line = client_getline(&TTX, 300);
      if (line) chomp(line);
    }
    free(line);
    if (line == NULL)
      goto BAIL;
  } else {
    for(i=0;i<ATX->users->items;i++) {
      int c = client_getcode(&TTX);
      if (c<0) 
        goto BAIL;
      if (c != LMTP_OK) {
        exitcode--;
      }
    }
  }

  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);
  close(TTX.sockfd);
  buffer_destroy(TTX.packet_buffer);
  return exitcode;

QUIT:
  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);

BAIL:
  exitcode = EFAILURE;
  buffer_destroy(TTX.packet_buffer);
  close(TTX.sockfd);
  return exitcode;
}

/*

 client_connect: establish a connection to an LMTP server

 Depending on whether CCF_PROCESS or CCF_DELIVER was specified in flags,
 this function establishes a connection to either the DSPAM server (for
 processing) or an LMTP delivery host (for delivery).

*/

int client_connect(int flags) {
  struct sockaddr_in addr;
  struct sockaddr_un saun;
  int yes = 1;
  int sockfd;
  int port = 24;
  int domain = 0;
  int addr_len;
  char *host;

  if (flags & CCF_DELIVERY) {
    host = _ds_read_attribute(agent_config, "DeliveryHost");

    if (_ds_read_attribute(agent_config, "DeliveryPort"))
      port = atoi(_ds_read_attribute(agent_config, "DeliveryPort"));

    if (host[0] == '/') 
      domain = 1;

  } else {
    host = _ds_read_attribute(agent_config, "ClientHost");

    if (_ds_read_attribute(agent_config, "ClientPort"))
      port = atoi(_ds_read_attribute(agent_config, "ClientPort"));

    if (host[0] == '/')
      domain = 1;
  }

  if (host == NULL) {
    report_error(ERROR_INVALID_CLIENT_CONFIG);
    return EINVAL;
  }

  /* Connect (Domain Socket) */

  if (domain) {
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    saun.sun_family = AF_UNIX;
    strcpy(saun.sun_path, host);
    addr_len = sizeof(saun.sun_family) + strlen(saun.sun_path) + 1;

    LOGDEBUG(CLIENT_CONNECT, host, 0);
    if(connect(sockfd, (struct sockaddr *)&saun, addr_len)<0) {
      report_error_printf(ERROR_CLIENT_CONNECT_SOCKET, host, strerror(errno));
      return EFAILURE;
    }

  /* Connect (TCP) */

  } else {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(port);
    addr_len = sizeof(struct sockaddr_in);

    LOGDEBUG(CLIENT_CONNECT, host, port);
    if(connect(sockfd, (struct sockaddr *)&addr, addr_len)<0) {
      report_error_printf(ERROR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
      return EFAILURE;
    }
  }

  LOGDEBUG(CLIENT_CONNECTED);
  setsockopt(sockfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

  return sockfd;
}

/*

 client_authenticate: greet and authenticate on a DLMTP server

 LHLO [ident]
 MAIL FROM: <password@ident>

*/

int client_authenticate(THREAD_CTX *TTX) {
  char buff[1024];
  char *input;
  char *ident = _ds_read_attribute(agent_config, "ClientIdent");

  if (ident == NULL || !strchr(ident, '@')) {
    report_error(ERROR_CLIENT_IDENT);
    return EINVAL;
  }

  input = client_expect(TTX, LMTP_GREETING);
  if (input == NULL) 
    return EFAILURE;
  free(input);

  snprintf(buff, sizeof(buff), "LHLO %s", strchr(ident, '@')+1);
  if (send_socket(TTX, buff)<=0) 
    return EFAILURE;

  if (client_getcode(TTX)!=LMTP_OK) 
    return EFAILURE;

  snprintf(buff, sizeof(buff), "MAIL FROM: <%s>", ident);
  if (send_socket(TTX, buff)<=0)
    return EFAILURE;

  if (client_getcode(TTX)!=LMTP_OK) {
    LOGDEBUG(ERROR_CLIENT_AUTHENTICATE);
    return EFAILURE;
  }

  return 0;
}

/*

 client_expect: wait for the appropriate code and return

 Keep reading until we receive the appropriate response code, then return
 the buffer. We also want to skip over LHLO extension advertisements.

*/
 
char * client_expect(THREAD_CTX *TTX, int response_code) {
  char *input, *dup; 
  char *ptr, *ptrptr;
  int code;

  input = client_getline(TTX, 300);
  while(input != NULL) {
    code = 0;
    dup = strdup(input);
    if (!dup) {
      free(input);
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      return NULL;
    }
    if (strncmp(dup, "250-", 4)) {
      ptr = strtok_r(dup, " ", &ptrptr);
      if (ptr) 
        code = atoi(ptr);
      free(dup);
      if (code == response_code) 
        return input;
    }
    
    free(input);
    input = client_getline(TTX, 300);
  }

  return NULL;
}

/* client_getcode: read the buffer and return response code */

int client_getcode(THREAD_CTX *TTX) {
  char *input, *ptr, *ptrptr;
  int i;

  input = client_getline(TTX, 300);
  if (input == NULL)
    return EFAILURE;

  while(input != NULL && !strncmp(input, "250-", 4)) {
    free(input);
    input = client_getline(TTX, 300);
  }

  ptr = strtok_r(input, " ", &ptrptr);
  if (ptr == NULL)
    return EFAILURE;
  i = atoi(ptr);
  free(input);
  return i;
}

/*

 client_getline: read a complete line from the buffer

 Wait for a complete line in the buffer, then return the line. If the wait
 exceeds the specified timeout, NULL is returned.

*/

char *client_getline(THREAD_CTX *TTX, int timeout) {
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

#ifdef VERBOSE
  LOGDEBUG("RECV: %s", pop);
#endif

  return pop;
}

/*
  pop_buffer: pop and return a line off the buffer 

  If a complete line isn't available, return NULL.

*/

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

/* send_socket: send text to a socket */

int send_socket(THREAD_CTX *TTX, const char *ptr) {
  int i = 0, r, msglen;

#ifdef VERBOSE
  LOGDEBUG("SEND: %s", ptr);
#endif

  msglen = strlen(ptr);
  while(i<msglen) {
    r = send(TTX->sockfd, ptr+i, msglen-i, 0);
    if (r <= 0) {
      return r;
    }
    i += r;
  }

  r = send(TTX->sockfd, "\r\n", 2, 0);
  return i+1;
}

/*
  deliver_socket: delivers via LMTP or SMTP instead of TrustedDeliveryAgent 

  If DeliveryProto was specified in dspam.conf, this function will be called 
  by deliver_message(). This function connects to and delivers the message 
  using standard LMTP or SMTP. Depending on how DSPAM was originally called, 
  either the address supplied with the incoming RCPT TO or the address 
  supplied on the commandline with --rcpt-to  will be used. If neither are
  present, the username will be used. 

*/

int deliver_socket(AGENT_CTX *ATX, const char *message, int proto) {
  THREAD_CTX TTX;
  char buff[1024];
  char *input;
  char *ident = _ds_read_attribute(agent_config, "DeliveryIdent");
  int exitcode = 0;
  int i, msglen;

  TTX.sockfd = client_connect(CCF_DELIVERY);
  if (TTX.sockfd <0) {
    report_error(ERROR_CLIENT_CONNECT);
    return TTX.sockfd;
  }

  TTX.packet_buffer = buffer_create(NULL);
  if (TTX.packet_buffer == NULL) 
    goto BAIL;

  input = client_expect(&TTX, LMTP_GREETING);
  if (input == NULL)
    goto BAIL;
  free(input);

  /* LHLO */

  snprintf(buff, sizeof(buff), "%s %s", (proto == DDP_LMTP) ? "LHLO" : "HELO",
           (ident) ? ident : "localhost");
  if (send_socket(&TTX, buff)<=0)
    goto BAIL;

  /* MAIL FROM - Pass-thru MAIL FROM and SIZE */
  /* ---------------------------------------- */

  input = client_expect(&TTX, LMTP_OK);
  if (input == NULL)
    goto QUIT;
  free(input);

  if (proto == DDP_LMTP) {
    snprintf(buff, sizeof(buff), "MAIL FROM:<%s> SIZE=%ld", 
             ATX->mailfrom, (long) strlen(message));
  } else {
    snprintf(buff, sizeof(buff), "MAIL FROM:<%s>", ATX->mailfrom);
  }

  if (send_socket(&TTX, buff)<=0)
    goto BAIL;

  if (client_getcode(&TTX)!=LMTP_OK)
    goto QUIT;

  /* RCPT TO - Recipient information or pass-thru */
  /* -------------------------------------------- */

  snprintf(buff, sizeof(buff), "RCPT TO:<%s>", (ATX->recipient) ? ATX->recipient : "");
  if (send_socket(&TTX, buff)<=0) 
    goto BAIL;

  if (client_getcode(&TTX)!=LMTP_OK) 
    goto QUIT;

  /* DATA - Send Message */
  /* ------------------- */

  if (send_socket(&TTX, "DATA")<=0) 
    goto QUIT;

  if (client_getcode(&TTX)!=LMTP_DATA)
    goto QUIT;

  i = 0;
  msglen = strlen(message);
  while(i<msglen) {
    int r = send(TTX.sockfd, message+i, msglen - i, 0);
    if (r <= 0) 
      goto BAIL;
    i += r;
  }

  if (message[strlen(message)-1]!= '\n')
    if (send_socket(&TTX, "")<=0)
     goto BAIL;

  if (send_socket(&TTX, "\r\n.")<=0)
    goto BAIL;

  /* Server Response */
  /* --------------- */

  input = client_expect(&TTX, LMTP_OK);
  if (input == NULL)
    goto QUIT;
   free(input);

  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);
  close(TTX.sockfd);
  buffer_destroy(TTX.packet_buffer);
  return 0;

QUIT:
  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);

BAIL:
  LOGDEBUG("socket delivery failed");
  exitcode = EFAILURE;
  buffer_destroy(TTX.packet_buffer);
  close(TTX.sockfd);
  return EFAILURE;
}

#endif /* DAEMON */

