/* $Id: tokenizer.c,v 1.4 2005/09/22 17:52:04 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

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

/*
 * tokenizer.c - tokenizer functions
 *
 * DESCRIPTION
 *   The tokenizer subroutines are responsible for decomposing a message into
 *   its colloquial components. All components are stored collectively in
 *   a diction object, passed into the function.
 *
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   ifdef HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif

#include "config.h"
#include "tokenizer.h"
#include "util.h"
#include "libdspam.h"
#include "language.h"
#ifdef NCORE
#include <ncore/ncore.h>
#include <ncore/types.h>
#include "ncore_adp.h"
#endif

#ifdef NCORE
extern nc_dev_t		g_ncDevice;
extern NC_STREAM_CTX	g_ncDelimiters;
#endif

/*
 * _ds_tokenize() - tokenize the message
 *
 * DESCRIPTION
 *    tokenizes the supplied message
 *
 * INPUT ARGUMENTS
 *     DSPAM_CTX *CTX    pointer to context
 *     char *header      pointer to message header
 *     char *body        pointer to message body
 *     ds_diction_t      diction to store components
 *
 * RETURN VALUES
 *   standard errors on failure
 *   zero if successful
 *
 */

int
_ds_tokenize (DSPAM_CTX * CTX, char *headers, char *body, ds_diction_t diction)
{
  if (diction == NULL)
    return EINVAL;

  if (CTX->flags & DSF_SBPH) 
    return _ds_tokenize_sbph(CTX, headers, body, diction);
  else
    return _ds_tokenize_ngram(CTX, headers, body, diction);
}

int _ds_tokenize_ngram(
  DSPAM_CTX *CTX, 
  char *headers, 
  char *body, 
  ds_diction_t diction)
{
  char *token;				/* current token */
  char joined_token[32] = { 0 };	/* used for de-obfuscating tokens */
  char *previous_token = NULL;		/* used for chained tokens */
#ifdef NCORE
  nc_strtok_t NTX;
#endif

  char *line = NULL;			/* header broken up into lines */
  char *ptrptr;

  char heading[128];			/* current heading */
  int alloc = 0;			/* did we alloc previous_token? */
  int l, jl;

  struct nt *header = NULL;      /* Header array */
  struct nt_node *node_nt;
  struct nt_c c_nt;

  /* Tokenize URLs in message */

  _ds_url_tokenize(diction, body, "http://");
  _ds_url_tokenize(diction, body, "www.");
  _ds_url_tokenize(diction, body, "href=");

  /*
   * Header Tokenization 
   */
 
  header = nt_create (NT_CHAR);
  if (header == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  line = strtok_r (headers, "\n", &ptrptr);
  while (line) {
    nt_add (header, line);
    line = strtok_r (NULL, "\n", &ptrptr);
  }

  node_nt = c_nt_first (header, &c_nt);
  heading[0] = 0;
  while (node_nt) {
    int is_received, multiline;
    joined_token[0] = 0;

#ifdef VERBOSE
    LOGDEBUG("processing line: %s", node_nt->ptr);
#endif
    line = node_nt->ptr;
    token = strtok_r (line, ":", &ptrptr);
    if (token && token[0] != 32 && token[0] != 9 && !strstr (token, " "))
    {
      multiline = 0;
      strlcpy (heading, token, 128);
      if (alloc) {
        free(previous_token);
        alloc = 0;
      }
      previous_token = NULL;
    } else {
      multiline = 1;
    }

#ifdef VERBOSE
    LOGDEBUG ("Reading '%s' header from: '%s'", heading, line);
#endif

    if (CTX->flags & DSF_WHITELIST) {
      /* Use the entire From: line for auto-whitelisting */

      if (!strcmp(heading, "From")) {
        char wl[256];
        char *fromline = line + 5;
        unsigned long long whitelist_token;

        if (fromline[0] == 32) 
          fromline++;
        snprintf(wl, sizeof(wl), "%s*%s", heading, fromline);
        whitelist_token = _ds_getcrc64(wl); 
        ds_diction_touch(diction, whitelist_token, wl, 0);
        diction->whitelist_token = whitelist_token;
      }
    }

    /* Received headers use a different set of delimiters to preserve things
       like ip addresses */

    is_received = (!strcmp (heading, "Received") ? 1 : 0);
    if (is_received)
      token = strtok_r ((multiline) ? line : NULL, DELIMITERS_HEADING, &ptrptr);
    else
      token = strtok_r ((multiline) ? line : NULL, DELIMITERS, &ptrptr);

    while (token)
    {
      l = strlen(token);

      if (l > 1 && l < 25)
      {
#ifdef VERBOSE
        LOGDEBUG ("Processing '%s' token in '%s' header", token, heading);
#endif

        /* Process reassembled tokens (e.g. V/I/A/G/R/A) */
        jl = strlen(joined_token);
        if (jl > 1 && jl < 25)
        {
          if (!_ds_process_header_token
                (CTX, joined_token, previous_token, diction, heading)
              && (CTX->flags & DSF_CHAINED))
          {
            /* shift previous token */
            if (alloc)
              free(previous_token);
            alloc = 1;
            previous_token = strdup (joined_token);
          }
          joined_token[0] = 0;
        }

        /* Process "current" token */
        if (!_ds_process_header_token
            (CTX, token, previous_token, diction, heading) && 
            (CTX->flags & DSF_CHAINED))
        {
          /* shift previous token */
          if (alloc) {
            free (previous_token);
            alloc = 0;
          }
          previous_token = token;
        }
      }

      /* push single letters onto reassembly buffer */
      else if (l == 1
               || (l == 2 && (strchr (token, '$') || strchr (token, '!'))))
      {
        strlcat (joined_token, token, sizeof (joined_token));
      }

      if (is_received)
        token = strtok_r (NULL, DELIMITERS_HEADING, &ptrptr);
      else
        token = strtok_r (NULL, DELIMITERS, &ptrptr);
    }

    /* Final token reassembly (anything left in the buffer) */
    jl = strlen(joined_token);
    if (jl > 1 && jl < 25) {
      _ds_process_header_token 
        (CTX, joined_token, previous_token, diction, heading);
    }

    if (alloc)
      free(previous_token);
    previous_token = NULL;
    alloc = 0;
    node_nt = c_nt_next (header, &c_nt);
  }

  nt_destroy (header);

  /*
   * Body Tokenization
   */

#ifdef VERBOSE
  LOGDEBUG("parsing message body");
#endif
  joined_token[0] = 0;
  alloc = 0;
#ifdef NCORE
  token = strtok_n (body, &g_ncDelimiters, &NTX);
#else
  token = strtok_r (body, DELIMITERS, &ptrptr);
#endif
  while (token != NULL)
  {
    l = strlen (token);
    if (l > 1 && l < 25)
    {
      jl = strlen(joined_token);

      /* Process reassembled tokens (e.g. V/I/A/G/R/A) */
      if (jl > 2 && jl < 25)
      {
        if (!_ds_process_body_token
            (CTX, joined_token, previous_token, diction) && 
            (CTX->flags & DSF_CHAINED))
        {
          /* shift previous token */
          if (alloc)
            free(previous_token);
          alloc = 1;
          previous_token = strdup (joined_token);
        }
        joined_token[0] = 0;
      }

      /* Process "current" token */ 
      if (!_ds_process_body_token
          (CTX, token, previous_token, diction) && (CTX->flags & DSF_CHAINED))
      {
        if (alloc) {
          alloc = 0;
          free (previous_token);
        }
        previous_token = token;
      }
    } 
    /* push single letters onto reassembly buffer */
    else if (l == 1
         || (l == 2 && (strchr (token, '$') || strchr (token, '!'))))
    {
      strlcat (joined_token, token, sizeof (joined_token));
    }
#ifdef NCORE
    token = strtok_n (NULL, NULL, &NTX);
#else
    token = strtok_r (NULL, DELIMITERS, &ptrptr);
#endif
  }

  /* Final token reassembly (anything left in the buffer) */

  jl = strlen(joined_token);
  if (jl > 2 && jl < 25) {
    _ds_process_body_token (CTX, joined_token, previous_token, diction);
  }

  if (alloc) 
    free (previous_token);

  return 0;
}

int _ds_tokenize_sbph(
  DSPAM_CTX *CTX, 
  char *headers, 
  char *body, 
  ds_diction_t diction)
{
  int i;
  char *token;				/* current token */
  char *previous_tokens[SBPH_SIZE];	/* sbph chain */
#ifdef NCORE
  nc_strtok_t NTX;
#endif

  char *line = NULL;			/* header broken up into lines */
  char *ptrptr;

  char heading[128];			/* current heading */
  int l;

  struct nt *header = NULL;      /* Header array */
  struct nt_node *node_nt;
  struct nt_c c_nt;

  for(i=0;i<SBPH_SIZE;i++)
    previous_tokens[i] = NULL;

  /* Tokenize URLs in message */

  _ds_url_tokenize(diction, body, "http://");
  _ds_url_tokenize(diction, body, "www.");
  _ds_url_tokenize(diction, body, "href=");

  /*
   * Header Tokenization 
   */
 
  header = nt_create (NT_CHAR);
  if (header == NULL)
  {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  line = strtok_r (headers, "\n", &ptrptr);
  while (line) {
    nt_add (header, line);
    line = strtok_r (NULL, "\n", &ptrptr);
  }

  node_nt = c_nt_first (header, &c_nt);
  heading[0] = 0;
  while (node_nt) {
    int is_received, multiline;

    _ds_sbph_clear(previous_tokens);

    line = node_nt->ptr;
    token = strtok_r (line, ":", &ptrptr);
    if (token && token[0] != 32 && token[0] != 9 && !strstr (token, " "))
    {
      multiline = 0;
      strlcpy (heading, token, 128);
      _ds_sbph_clear(previous_tokens);
    } else {
      multiline = 1;
    }

#ifdef VERBOSE
    LOGDEBUG ("Reading '%s' header from: '%s'", heading, line);
#endif

    if (CTX->flags & DSF_WHITELIST) {
      /* Use the entire From: line for auto-whitelisting */

      if (!strcmp(heading, "From")) {
        char wl[256];
        char *fromline = line + 5;
        unsigned long long whitelist_token;

        if (fromline[0] == 32) 
          fromline++;
        snprintf(wl, sizeof(wl), "%s*%s", heading, fromline);
        whitelist_token = _ds_getcrc64(wl); 
        ds_diction_touch(diction, whitelist_token, wl, 0);
        diction->whitelist_token = whitelist_token;
      }
    }

    /* Received headers use a different set of delimiters to preserve things
       like ip addresses */

    is_received = (!strcmp (heading, "Received") ? 1 : 0);
    if (is_received)
      token = strtok_r ((multiline) ? line : NULL, DELIMITERS_HEADING, &ptrptr);
    else
      token = strtok_r ((multiline) ? line : NULL, DELIMITERS, &ptrptr);

    while (token)
    {
      l = strlen(token);

      if (l > 0 && l < 25)
      {
#ifdef VERBOSE
        LOGDEBUG ("Processing '%s' token in '%s' header", token, heading);
#endif
        _ds_map_header_token (CTX, token, previous_tokens, diction, heading);
      }

      if (is_received)
        token = strtok_r (NULL, DELIMITERS_HEADING, &ptrptr);
      else
        token = strtok_r (NULL, DELIMITERS, &ptrptr);
    }

    for(i=0;i<SBPH_SIZE;i++) {
      _ds_map_header_token(CTX, NULL, previous_tokens, diction, heading);
    }

    _ds_sbph_clear(previous_tokens);
    node_nt = c_nt_next (header, &c_nt);
  }
  nt_destroy (header);

  /*
   * Body Tokenization
   */

#ifdef NCORE
  token = strtok_n (body, &g_ncDelimiters, &NTX);
#else
  token = strtok_r (body, DELIMITERS, &ptrptr);
#endif
  while (token != NULL)
  {
    l = strlen (token);
    if (l > 0 && l < 25)
    {
      /* Process "current" token */ 
      _ds_map_body_token (CTX, token, previous_tokens, diction);
    } 

#ifdef NCORE
    token = strtok_n (NULL, NULL, &NTX);
#else
    token = strtok_r (NULL, DELIMITERS, &ptrptr);
#endif
  }

  for(i=0;i<SBPH_SIZE;i++) {
    _ds_map_body_token(CTX, NULL, previous_tokens, diction);
  }

  _ds_sbph_clear(previous_tokens);

  return 0;
}

/*
 * _ds_{process,map}_{header,body}_token()
 *
 * DESCRIPTION
 *  Token processing and mapping functions
 *    _ds_process_header_token
 *    _ds_process_body_token
 *    _ds_map_header_token
 *    _ds_map_body_token
 *
 *  These functions are responsible to converting the input words into
 *  full blown tokens with CRCs, probabilities, and producing variants
 *  based on the tokenizer approach applied. 
 */
 
int
_ds_process_header_token (DSPAM_CTX * CTX, char *token,
                          const char *previous_token, ds_diction_t diction,
                          const char *heading)
{
  int all_num = 1, i;
  char combined_token[256];
  int len = 0;
  int is_received;
  unsigned long long crc;
  char *tweaked_token;

  if (_ds_match_attribute(CTX->config->attributes, "IgnoreHeader", heading))
    return 0;

  is_received = (!strcmp (heading, "Received") ? 1 : 0);

  if (is_received && strlen (token) < 6)
    return EINVAL;

  for (i = 0; token[i] != 0; i++)
  {
    if (!isdigit ((unsigned char) token[i]))
      all_num = 0;
    if (iscntrl ((unsigned char) token[i])) {
      token[i] = 'z';
      all_num = 0;
    }
  }

  len = i - 1;

  if (isdigit ((unsigned char) token[0]))
  {
    if (token[len - 1] != '%')
      all_num = 1;
  }

  if (!(isalnum ((unsigned char) token[0]) || (unsigned char) token[0] > 127) && token[0] != '$' && token[0] != '#')
    all_num = 1;

  if (is_received)
    all_num = 0;

  /* Ignore tokens that are all numbers, or contain high ASCII characters */
  if (all_num)
    return EINVAL;

  /* This is where we used to ignore certain headings */

  if (heading[0] != 0)
    snprintf (combined_token, sizeof (combined_token),
              "%s*%s", heading, token);
  else
    strlcpy (combined_token, token, sizeof (combined_token));

  tweaked_token = _ds_truncate_token(token);
  if (tweaked_token == NULL)
    return EUNKNOWN;

  snprintf(combined_token, sizeof(combined_token), "%s*%s", heading, tweaked_token);

  crc = _ds_getcrc64 (combined_token);
#ifdef VERBOSE
  LOGDEBUG ("Token Hit: '%s'", combined_token);
#endif
  ds_diction_touch(diction, crc, combined_token, 0);

  if ((CTX->flags & DSF_CHAINED) && previous_token != NULL && !is_received)
  {
    char *tweaked_previous;

    tweaked_previous = _ds_truncate_token(previous_token);
    if (tweaked_previous == NULL)
      return EUNKNOWN;

    snprintf (combined_token, sizeof (combined_token),
              "%s*%s+%s", heading, tweaked_previous, tweaked_token);
    crc = _ds_getcrc64 (combined_token);

    ds_diction_touch(diction, crc, combined_token, DSD_CHAINED);
    free(tweaked_previous);
  }

  free(tweaked_token);
  return 0;
}

int
_ds_process_body_token (DSPAM_CTX * CTX, char *token,
                        const char *previous_token, ds_diction_t diction)
{
  int all_num = 1, i;
  char combined_token[256];
  int len;
  unsigned long long crc;
  char *tweaked_token;

  for (i = 0; token[i] != 0; i++)
  {
    if (!isdigit ((unsigned char) token[i]))
      all_num = 0;
    if (iscntrl ((unsigned char) token[i])) {
      token[i] = 'z';
      all_num = 0;
    }
  }

  len = i - 1;

  if (isdigit ((unsigned char) token[0]))
  {
    int l = len - 1;
    if (token[l] != '%')
      all_num = 1;
  }

  if (!(isalnum ((unsigned char) token[0]) || (unsigned char) token[0] > 127) && token[0] != '$' && token[0] != '#')
    all_num = 1;

  /* Ignore tokens that are all numbers, or contain high ASCII characters */
  if (all_num)
    return EINVAL;

  tweaked_token = _ds_truncate_token(token);
  if (tweaked_token == NULL)
    return EUNKNOWN;

  crc = _ds_getcrc64 (tweaked_token);

  ds_diction_touch(diction, crc, tweaked_token, DSD_CONTEXT);

  if ((CTX->flags & DSF_CHAINED) && previous_token != NULL)
  {
    char *tweaked_previous = _ds_truncate_token(previous_token);
    if (tweaked_previous == NULL)
      return EUNKNOWN;

    snprintf (combined_token, sizeof (combined_token), "%s+%s",
              tweaked_previous, tweaked_token);
    crc = _ds_getcrc64 (combined_token);

    ds_diction_touch(diction, crc, combined_token, DSD_CHAINED | DSD_CONTEXT);
    free(tweaked_previous);
  }
  free(tweaked_token);

  return 0;
}


int
_ds_map_header_token (DSPAM_CTX * CTX, char *token,
                      char **previous_tokens, ds_diction_t diction,
                      const char *heading)
{
  int i, mask, t;
  unsigned long long crc;
  char key[256];
  int active = 0, top;

  if (_ds_match_attribute(CTX->config->attributes, "IgnoreHeader", heading))
    return 0;

  /* Shift all previous tokens up */
  for(i=0;i<SBPH_SIZE-1;i++) {
    previous_tokens[i] = previous_tokens[i+1];
    if (previous_tokens[i])
      active++;
  }

  previous_tokens[SBPH_SIZE-1] = token;

  if (token) 
    active++;

  /* Iterate and generate all keys necessary */
  for(mask=0;mask < _ds_pow2(active);mask++) {
    int terms = 0;
    key[0] = 0;
    t = 0;
    top = 1;
                                                                                
    /* Each Bit */
    for(i=0;i<SBPH_SIZE;i++) {
      if (t) 
        strlcat(key, "+", sizeof(key));

      if (mask & (_ds_pow2(i+1)/2)) {
        if (previous_tokens[i] == NULL ||!strcmp(previous_tokens[i], ""))
          strlcat(key, "#", sizeof(key));
        else
        {
          strlcat(key, previous_tokens[i], sizeof(key));
          terms++;
        }
      } else {
        strlcat(key, "#", sizeof(key));
      }
      t++;
    }

    /* If the bucket has at least 2 literals, hit it */
    if (terms) {
      char hkey[256];
      char *k = key;
      int kl = strlen(key);
      while(kl>2 && !strcmp((key+kl)-2, "+#")) {
        key[kl-2] = 0;
        kl -=2;
      }
      while(!strncmp(k, "#+", 2)) {
        top = 0; 
        k+=2;
      }

      if (top) {
        snprintf(hkey, sizeof(hkey), "%s*%s", heading, k);
        crc = _ds_getcrc64(hkey);
        ds_diction_touch(diction, crc, hkey, DSD_CONTEXT);
      }
    }
  }

  return 0;
}

int
_ds_map_body_token (DSPAM_CTX * CTX, char *token,
                        char **previous_tokens, ds_diction_t diction)
{
  int i,  mask, t;
  int top;
  unsigned long long crc;
  char key[256];
  int active = 0;

  /* Shift all previous tokens up */
  for(i=0;i<SBPH_SIZE-1;i++) {
    previous_tokens[i] = previous_tokens[i+1];
    if (previous_tokens[i]) 
      active++;
  }

  previous_tokens[SBPH_SIZE-1] = token;

  if (token) 
    active++;

  /* Iterate and generate all keys necessary */
  for(mask=0;mask < _ds_pow2(active);mask++) {
    int terms = 0;
    t = 0;

    key[0] = 0;
    top = 1;

    /* Each Bit */
    for(i=0;i<SBPH_SIZE;i++) {
      if (t)
        strlcat(key, "+", sizeof(key));
      if (mask & (_ds_pow2(i+1)/2)) {
        if (previous_tokens[i] == NULL ||!strcmp(previous_tokens[i], ""))
          strlcat(key, "#", sizeof(key));
        else
        {
          strlcat(key, previous_tokens[i], sizeof(key));
          terms++;
        }
      } else {
        strlcat(key, "#", sizeof(key));
      }
      t++;
    }

    /* If the bucket has at least 2 literals, hit it */
    if (terms) {
      char *k = key;
      int kl = strlen(key);
      while(kl>2 && !strcmp((key+kl)-2, "+#")) {
        key[kl-2] = 0;
        kl -=2;
      }
      while(!strncmp(k, "#+", 2)) {
        top = 0; 
        k+=2;
      }
 
      if (top) {
        crc = _ds_getcrc64(k);
        ds_diction_touch(diction, crc, k, DSD_CONTEXT);
      }
    }
  }

  return 0;
}

/* 
 *  _ds_degenerate_message()
 *
 * DESCRIPTION
 *   Degenerate the message into tokenizable pieces
 *
 *   This function is responsible for analyzing the actualized message and
 *   degenerating it into only the components which are tokenizable.  This 
 *   process  effectively eliminates much HTML noise, special symbols,  or
 *   other  non-tokenizable/non-desirable components. What is left  is the
 *   bulk of  the message  and only  desired tags,  URLs, and other  data.
 *
 * INPUT ARGUMENTS
 *      header    pointer to buffer containing headers
 *      body      pointer to buffer containing message body
 */

int _ds_degenerate_message(DSPAM_CTX *CTX, buffer * header, buffer * body)
{
  char *decode, *x, *y;
  struct nt_node *node_nt, *node_header;
  struct nt_c c_nt, c_nt2;
  int i = 0;
  char heading[1024];

  if (CTX->message == NULL)
  {
    LOG (LOG_WARNING, "_ds_actualize_message() failed");
    return EUNKNOWN;
  }

  /* Iterate through each component and create large header/body buffers */

  node_nt = c_nt_first (CTX->message->components, &c_nt);
  while (node_nt != NULL)
  {
    struct _ds_message_block *block = (struct _ds_message_block *) node_nt->ptr;

#ifdef VERBOSE
    LOGDEBUG ("Processing component %d", i);
#endif

    if (block->headers == NULL || block->headers->items == 0)
    {
#ifdef VERBOSE
      LOGDEBUG ("  : End of Message Identifier");
#endif
    }

    /* Skip Attachments */

    else 
    {
      /* Accumulate the headers */
      node_header = c_nt_first (block->headers, &c_nt2);
      while (node_header != NULL)
      {
        struct _ds_header_field *current_header =
          (struct _ds_header_field *) node_header->ptr;
        snprintf (heading, sizeof (heading),
                  "%s: %s\n", current_header->heading,
                  current_header->data);
        buffer_cat (header, heading);
        node_header = c_nt_next (block->headers, &c_nt2);
      }

      decode = block->body->data;

      if (block->media_type == MT_TEXT    ||
               block->media_type == MT_MESSAGE ||
               block->media_type == MT_UNKNOWN ||
               (i == 0 && (block->media_type == MT_TEXT      ||
                           block->media_type == MT_MULTIPART ||
                           block->media_type == MT_MESSAGE)))
      {

        /* Accumulate the bodies */
        if (
             (block->encoding == EN_BASE64 || 
              block->encoding == EN_QUOTED_PRINTABLE) && 
             block->original_signed_body == NULL
           )
        {
          struct _ds_header_field *field;
          int is_attachment = 0;
          struct nt_node *node_hnt;
          struct nt_c c_hnt;
  
          node_hnt = c_nt_first (block->headers, &c_hnt);
          while (node_hnt != NULL)
          {
            field = (struct _ds_header_field *) node_hnt->ptr;
            if (field != NULL && field->heading != NULL && field->data != NULL)
              if (!strncasecmp (field->heading, "Content-Disposition", 19))
                if (!strncasecmp (field->data, "attachment", 10))
                  is_attachment = 1;
            node_hnt = c_nt_next (block->headers, &c_hnt);
          }
  
          if (!is_attachment)
          {
            LOGDEBUG ("decoding message block from encoding type %d",
                      block->encoding);
            decode = _ds_decode_block (block);
          }
        }
  
        if (decode != NULL)
        {
          char *decode2 = strdup(decode);
  
          /* -- PREFORMATTING BEGIN -- */
  
          // Hexadecimal decoding
          if (block->encoding == EN_8BIT) {
            char hex[5] = "0x00";
            int conv;

            x = strchr(decode2, '%');
            while(x != NULL) {
              if (isxdigit((unsigned char) x[1]) && 
                  isxdigit((unsigned char) x[2])) 
               {
                hex[2] = x[1];
                hex[3] = x[2];
                                                                                
                conv = strtol(hex, NULL, 16);
                if (conv) {
                  x[0] = conv;
                  memmove(x+1, x+3, strlen(x+3));
                }
              }
              x = strchr(x+1, '%');
            }
          }
  
          if (block->media_subtype == MST_HTML) {
  
            /* Remove long HTML Comments */
            x = strstr (decode2, "<!--");
            while (x != NULL)
            {
              y = strstr (x, "-->");
              if (y != NULL)
              {
                memmove (x, y + 3, strlen (y + 3) + 1);
                x = strstr (x, "<!--");
              }
              else
              {
                x = strstr (x + 4, "<!--");
              }
            }
                                                                                
            /* Remove short HTML Comments */
            x = strstr (decode2, "<!");
            while (x != NULL)
            {
              y = strchr (x, '>');
              if (y != NULL)
              {
                memmove (x, y + 1, strlen (y + 1) + 1);
                x = strstr (x, "<!");
              }
              else
              {
                x = strstr (x + 2, "<!");
              }
            }
  
            /* Remove short html tags and useless tags */
            x = strchr (decode2, '<');
            while (x != NULL)
            {
              y = strchr (x, '>');
              if (y != NULL
                  && (y - x <= 15
                      || !strncasecmp (x + 1, "td ", 3)
                      || !strncasecmp (x + 1, "!doctype", 8)
                      || !strncasecmp (x + 1, "blockquote", 10)
                      || !strncasecmp (x + 1, "table ", 6)
                      || !strncasecmp (x + 1, "tr ", 3)
                      || !strncasecmp (x + 1, "div ", 4)
                      || !strncasecmp (x + 1, "p ", 2)
                      || !strncasecmp (x + 1, "body ", 5) || !strchr (x, ' ')
                      || strchr (x, ' ') > y))
              {
                memmove (x, y + 1, strlen (y + 1) + 1);
                x = strstr (x, "<");
              }
              else
              {
                x = strstr (x + 1, "<");
              }
            }
  
            /* -- PREFORMATTING END -- */
          }
  
          buffer_cat (body, decode2);
          free(decode2);
          if (decode != block->body->data)
          {
            block->original_signed_body = block->body;
  
            block->body = buffer_create (decode);
            free (decode);
          }
        }
      }
    }
    node_nt = c_nt_next (CTX->message->components, &c_nt);
    i++;
  } /* while (node_nt != NULL) */

  if (header->data == NULL)
    buffer_cat (header, " ");

  if (body->data == NULL)
    buffer_cat (body, " ");

  return 0;
}

int _ds_url_tokenize(ds_diction_t diction, char *body, const char *key) 
{
  char *url_body, *token, *url_ptr;
  char combined_token[256];
  int url_length;
  unsigned long long crc;

#ifdef VERBOSE
  LOGDEBUG("scanning for urls: %s\n", key);
#endif
  if (!body)
    return EINVAL;
  url_body = strdup(body);
  url_ptr = url_body;

  token = strcasestr(url_ptr, key);
  while (token != NULL)
  {
    char *ptrurl;
    char *const url_end = token + strlen(token);

    url_ptr = token;
    token = strtok_r (token, " \n\">", &ptrurl);
    if (token != NULL)
    {
      char *urltoken;
      char *ptrurl2;
      url_length = strlen (token);

      /* Individual tokens form the URL */
      urltoken = strtok_r (token, DELIMITERS, &ptrurl2);
      while (urltoken != NULL)
      {
        snprintf (combined_token, sizeof (combined_token), "Url*%s",
                  urltoken);
        crc = _ds_getcrc64 (combined_token);
        ds_diction_touch(diction, crc, combined_token, 0);
        urltoken = strtok_r (NULL, DELIMITERS, &ptrurl2);
      }

      memset (body + (url_ptr - url_body), 32, url_length);
      url_ptr += url_length + 1;
      if (url_ptr >= url_end)
        token = NULL;
      else 
        token = strcasestr(url_ptr, key);
    }
    else
      token = NULL;
  }
  free (url_body);
  return 0;
}

/* Truncate tokens with EOT delimiters */
char * _ds_truncate_token(const char *token) {
  char *tweaked;
  int i; 

  if (token == NULL)
    return NULL;

  tweaked = strdup(token);

  if (tweaked == NULL)
    return NULL;

  i = strlen(tweaked);
  while(i>1 && strspn(tweaked+i-2, DELIMITERS_EOT)) {
    tweaked[i-1] = 0;
    i--;
  } 

  return tweaked;
}

/*
 *  _ds_spbh_clear
 *
 * DESCRIPTION
 *   Clears the SBPH stack
 *   
 *   Clears and frees all of the tokens in the SBPH stack. Used when a
 *   boundary has been crossed (such as a new message header) where
 *   tokens from the previous boundary are no longer useful.
 */

void _ds_sbph_clear(char **previous_tokens) {
  int i;
  for(i=0;i<SBPH_SIZE;i++) 
    previous_tokens[i] = NULL;
  return;
}
