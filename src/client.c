/* $Id: client.c,v 1.2 2004/12/02 17:55:51 jonz Exp $ */

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

int client_process(AGENT_CTX *ATX, buffer *message) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  THREAD_CTX TTX;
  char buff[1024];
  int exitcode = 0;
  int i, msglen;

  TTX.sockfd = client_connect();
  if (TTX.sockfd <0) {
    LOGDEBUG(ERROR_CLIENT_CONNECT);
    return TTX.sockfd;
  }

  TTX.packet_buffer = buffer_create(NULL);
  if (TTX.packet_buffer == NULL) 
    goto BAIL;

  if (client_authenticate(&TTX)<0) {
    LOGDEBUG(ERROR_CLIENT_AUTH_FAILED);
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

  if (message->data[strlen(message->data)-1]!= '\n') 
    buffer_cat(message, "\n");

  i = 0;
  msglen = strlen(message->data)+1;
  while(i<msglen) {
    int r = send(TTX.sockfd, message->data+i, msglen - i, 0);
    if (r <= 0) 
      goto BAIL;
    i += r;
  }

  if (send_socket(&TTX, ".")<=0)
    goto BAIL;

  for(i=0;i<ATX->users->items;i++) {
    int c = client_getcode(&TTX);
    if (c != LMTP_OK) {
      exitcode--;
    }
  }

  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);
  close(TTX.sockfd);
  buffer_destroy(TTX.packet_buffer);
  return 0;

QUIT:
  send_socket(&TTX, "QUIT");
  client_getcode(&TTX);

BAIL:
  buffer_destroy(TTX.packet_buffer);
  close(TTX.sockfd);
  return EFAILURE;
}

int client_connect(void) {
  struct sockaddr_in addr;
  int yes = 1;
  int sockfd;
  int port = 24;
  char *host = _ds_read_attribute(agent_config, "ClientHost");

  if (_ds_read_attribute(agent_config, "ClientPort"))
    port = atoi(_ds_read_attribute(agent_config, "ClientPort"));

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(host);
  addr.sin_port = htons(port);

  LOGDEBUG(CLIENT_CONNECT, host, port);
  if(connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))<0) {
    LOGDEBUG(ERROR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
    return EFAILURE;
  } 

  LOGDEBUG(CLIENT_CONNECTED);
  setsockopt(sockfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

  return sockfd;
}

int client_authenticate(THREAD_CTX *TTX) {
  char buff[1024];
  char *input;

  input = client_expect(TTX, LMTP_GREETING);
  if (input == NULL) 
    return EFAILURE;

  if (send_socket(TTX, "LHLO")<=0) 
    return EFAILURE;

  if (client_getcode(TTX)!=LMTP_OK) 
    return EFAILURE;

  snprintf(buff, sizeof(buff), "MAIL FROM: %s", _ds_read_attribute(agent_config, "ClientIdent"));
  if (send_socket(TTX, buff)<=0)
    return EFAILURE;

  if (client_getcode(TTX)!=LMTP_OK) {
    LOGDEBUG(ERROR_CLIENT_AUTHENTICATE);
    return EFAILURE;
  }

  return 0;
}

char * client_expect(THREAD_CTX *TTX, int response_code) {
  char *input, *dup; 
  char *ptr, *ptrptr;
  int code;

  input = socket_getline(TTX, 300);
  while(input != NULL) {
    code = 0;
    dup = strdup(input);
    if (!dup) {
      free(input);
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      return NULL;
    }
    ptr = strtok_r(dup, " ", &ptrptr);
    if (ptr) 
      code = atoi(ptr);
    free(dup);
    if (code == response_code) 
      return input;
    
    free(input);
    input = socket_getline(TTX, 300);
  }

  return NULL;
}

int client_getcode(THREAD_CTX *TTX) {
  char *input, *ptr, *ptrptr;

  input = socket_getline(TTX, 300);
  if (input == NULL)
    return EFAILURE;
  ptr = strtok_r(input, " ", &ptrptr);
  if (ptr == NULL)
    return EFAILURE;
  return atoi(ptr);
}

#endif /* DAEMON */

