/* $Id: libdspam.c,v 1.21 2004/11/23 21:27:18 jonz Exp $ */

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
#include <fcntl.h>
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
#include "libdspam_objects.h"
#include "libdspam.h"
#include "nodetree.h"
#include "config.h"
#include "base64.h"
#include "bnr.h"
#include "util.h"
#include "storage_driver.h"
#include "buffer.h"
#include "lht.h"
#include "tbt.h"
#include "error.h"
#include "decode.h"
#include "language.h"

/* Fisher-Robinson's Inverse Chi-Square Constants */
#define CHI_S           0.1     /* Strength */
#define CHI_X           0.5000  /* Assumed Probability */

#ifdef DEBUG
int DO_DEBUG = 0;
char debug_text[1024];
#endif

/*
 *   The  dspam_init() function creates and initializes a new classification
 *   context and attaches the context to whatever backend  storage  facility
 *   was  configured. The user and group arguments provided are used to read
 *   and write information stored for the user and group specified. The home
 *   argument is used to configure libdspam’s storage around the base direc-
 *   tory specified. The mode specifies the operating mode to initialize the
 *   classification context with and may be one of:
 *
 *    DSM_PROCESS   Process the message and return a result
 *    DSM_CLASSIFY  Classify message only, no learning
 *    DSM_TOOLS     No processing, attach to storage only
 * 
 *   The  flags  provided further tune the classification context for a spe-
 *   cific function. Multiple flags may be OR’d together.
 * 
 *    DSF_CHAINED   Use a Chained (Multi-Word) Tokenizer
 *    DSF_SBPH      Use Sparse Binary Polynomial Hashing Tokenizer
 *    DSF_SIGNATURE A binary signature is requested/provided
 *    DSF_NOISE     Apply Bayesian Noise Reduction logic
 *    DSF_WHITELIST Use automatic whitelisting logic
 *    DSF_MERGED    Merge group metadata with user’s in memory
 * 
 *   Upon successful completion, dspam_init() will return a pointer to a new
 *   classification context structure containing a copy of the configuration
 *   passed into dspam_init(), a connected storage driver handle, and a  set
 *   of preliminary user control data read from storage.
 */
                                                                                

DSPAM_CTX * dspam_init  (const char *username,
                         const char *group,
                         const char *home,
                         int operating_mode,
                         u_int32_t flags)
{
  DSPAM_CTX *CTX = dspam_create(username, group, home, operating_mode, flags);
                                                                                
  if (CTX == NULL)
    return NULL;
                                                                                
  if (!dspam_attach(CTX, NULL))
    return CTX;

  dspam_destroy(CTX);

  return NULL;
}

/*
 *   The  dspam_create() function performs in exactly the same manner as the
 *   dspam_init() function, but does not attach  to  storage.  Instead,  the
 *   caller  must  also  call dspam_attach() after setting any storage- spe-
 *   cific attributes using dspam_addattribute(). This is useful  for  cases
 *   where  the  implementor  would  prefer  to configure storage internally
 *   rather than having libdspam read a configuration from a file.
 */

DSPAM_CTX * dspam_create (const char *username,
			 const char *group,
                         const char *home,
			 int operating_mode,
			 u_int32_t flags)
{
  DSPAM_CTX *CTX;

  CTX = calloc (1, sizeof (DSPAM_CTX));
  if (CTX == NULL)
    return NULL;

  CTX->config = calloc(1, sizeof(struct _ds_config));
  if (CTX->config == NULL) {
    report_error(ERROR_MEM_ALLOC);
    goto bail;
  }
  CTX->config->size = 128;
  CTX->config->attributes = calloc(1, sizeof(attribute_t *)*128);
  if (CTX->config->attributes == NULL) {
    report_error(ERROR_MEM_ALLOC);
    goto bail;
  }

  if (home != NULL && home[0] != 0)
    CTX->home = strdup (home);
  else {
#ifdef DSPAM_HOME
    CTX->home = strdup(DSPAM_HOME);
#else
    report_error_printf(ERROR_DSPAM_INIT, ERROR_NO_HOME);
#endif
  }

  if (username != NULL && username[0] != 0)
    CTX->username = strdup (username);
  else
    CTX->username = NULL;

  if (group != NULL && group[0] != 0)
    CTX->group = strdup (group);
  else
    CTX->group = NULL;

  CTX->probability     = -1;
  CTX->operating_mode  = operating_mode;
  CTX->flags           = flags;
  CTX->message         = NULL;
  CTX->confidence      = 0;
  CTX->training_mode   = DST_TEFT;
  CTX->wh_threshold    = 10;
  CTX->training_buffer = 5;
  CTX->classification  = DSR_NONE;
  CTX->source          = DSS_NONE;
  CTX->_sig_provided   = 0;
  CTX->factors         = NULL;
  CTX->algorithms      = 0;

  /* Algorithms */

#if defined(GRAHAM_BAYESIAN) 
  CTX->algorithms |= DSA_GRAHAM;
#endif

#if defined(BURTON_BAYESIAN)
  CTX->algorithms |= DSA_BURTON;
#endif

#if defined(ROBINSON)
  CTX->algorithms |= DSA_ROBINSON;
#endif

#if defined(CHI_SQUARE) 
  CTX->algorithms |= DSA_CHI_SQUARE;
#endif

  /* P-Values */

#if defined(ROBINSON_FW)
  CTX->algorithms |= DSP_ROBINSON; 
#endif

  return CTX;

bail:
  if (CTX->config)
    _ds_destroy_attributes(CTX->config->attributes);
  free(CTX->config);
  free(CTX->username);
  free(CTX->group);
  free(CTX);
  return NULL;
}

/*
 *   The dspam_addattribute() function is called to  set  attributes  within
 *   the  classification  context.  Some  storage drivers support the use of
 *   passing specific attributes such as  server  connect  information.  The
 *   driver-independent attributes supported by DSPAM include:
 * 
 *    IgnoreHeader   Specify a specific header to ignore
 *    LocalMX        Specify a local mail exchanger to assist in
 *                   correct results from dspam_getsource().
 *
 *   Only  driver-dependent  attributes  need  be  set  prior  to  a call to
 *   dspam_attach(). Driver-independent attributes may be  set  both  before
 *   and after storage has been attached.
 */                                                                                 
int dspam_addattribute (DSPAM_CTX * CTX, const char *key, const char *value) {
  int i, j = 0;
                                                                                
  if (_ds_find_attribute(CTX->config->attributes, key))
    return _ds_add_attribute(CTX->config->attributes, key, value);
                                                                                
  for(i=0;CTX->config->attributes[i];i++)
    j++;
                                                                                
  if (j >= CTX->config->size) {
    CTX->config->size *= 2;
    CTX->config->attributes = realloc(CTX->config->attributes, 1+(sizeof(attribute_t *)*CTX->config->size));
    if (CTX->config->attributes == NULL) {
      report_error(ERROR_MEM_ALLOC);
      return EUNKNOWN;
    }
  }
                                                                                
  return _ds_add_attribute(CTX->config->attributes, key, value);
}

/*
 *   The dspam_attach() function attaches the storage interface to the clas-
 *   sification context and alternatively established an initial  connection
 *   with  storage  if dbh is NULL. Some storage drivers support only a NULL
 *   value  for  dbh,  while  others  (such  as  mysql_drv,  pgsql_drv,  and
 *   sqlite_drv) allow an open database handle to be attached. This function
 *   should only be called after  an  initial  call  to  dspam_create()  and
 *   should  never  be called if using dspam_init(), as storage is automati-
 *   cally attached by a call to dspam_init().
 */

int dspam_attach (DSPAM_CTX *CTX, void *dbh) {
  if (!_ds_init_storage (CTX, dbh))
    return 0;
                                                                                
  return EFAILURE;
}

/*
 *     The dspam_detach() function can be called when a detachment from  stor-
 *     age  is desired, but the context is still needed. The storage driver is
 *     closed, leaving the classification context in place. Once  the  context
 *     is no longer needed, another call to dspam_destroy() should be made. If
 *     you are closing storage and destroying the context at the same time, it
 *     is   not  necessary  to  call  this  function.  Instead  you  may  call
 *     dspam_destroy() directly.
 */

int
dspam_detach (DSPAM_CTX * CTX)
{

  if (CTX->storage != NULL) {
                                                                                
    /* Sanity-Check Totals */
    if (CTX->totals.spam_learned < 0)
      CTX->totals.spam_learned = 0;
    if (CTX->totals.innocent_learned < 0)
      CTX->totals.innocent_learned = 0;
    if (CTX->totals.spam_misclassified < 0)
      CTX->totals.spam_misclassified = 0;
    if (CTX->totals.innocent_misclassified < 0)
      CTX->totals.innocent_misclassified = 0;
    if (CTX->totals.spam_classified < 0)
      CTX->totals.spam_classified = 0;
    if (CTX->totals.innocent_classified < 0)
      CTX->totals.innocent_classified = 0;

    _ds_shutdown_storage (CTX);
    free(CTX->storage);
    CTX->storage = NULL;
  }

  return 0;
}

/*
 *     The dspam_destroy() function should be called when the  context  is  no
 *     longer  needed.  If a connection was established to storage internally,
 *     the connection is closed and all data is flushed and written. If a han-
 *     dle was attached, the handle will remain open.
 */

int
dspam_destroy (DSPAM_CTX * CTX)
{
  if (CTX->storage != NULL)
    dspam_detach(CTX);

  _ds_factor_destroy(CTX->factors);
  if (CTX->config && CTX->config->attributes)
    _ds_destroy_attributes (CTX->config->attributes);

  free(CTX->config);
 
  free (CTX->username);
  free (CTX->group);
  free (CTX->home);

  if (! CTX->_sig_provided && CTX->signature != NULL)
  {
    free (CTX->signature->data);
    free (CTX->signature);
  }

  if (CTX->message)
    _ds_destroy_message(CTX->message);
  free (CTX);
  return 0;
}

/*
 *   The dspam_process() function performs analysis of  the  message  passed
 *   into  it  and will return zero on successful completion. If successful,
 *   CTX->result will be set to one of three classification results:
 * 
 *    DSR_ISSPAM        Message was classified as spam
 *    DSR_ISINNOCENT    Message was classified as nonspam
 *    DSR_ISWHITELISTED Recipient was automatically whitelisted
 * 
 *   Should the call fail, one of the following errors will be returned:
 * 
 *    EINVAL    An invalid call or invalid parameter used.
 *    EUNKNOWN  Unexpected error, such as malloc() failure
 *    EFILE     Error opening or writing to a file or file handle
 *    ELOCK     Locking failure
 *    EFAILURE  The operation itself has failed
 */

int
dspam_process (DSPAM_CTX * CTX, const char *message)
{
  buffer *header, *body;
  int spam_result = 0, is_toe = 0;

  if (CTX->signature != NULL)
    CTX->_sig_provided = 1;

  /* We can't ask for a classification and provide one simultaneously */
  if (CTX->operating_mode == DSM_CLASSIFY && CTX->classification != DSR_NONE)
  {
    LOG(LOG_WARNING, "DSM_CLASSIFY can't be used with a provided classification");
    return EINVAL;
  }

  if (CTX->classification != DSR_NONE && CTX->source == DSR_NONE) 
  {
    LOG(LOG_WARNING, "A classification requires a source be specified");
    return EINVAL;
  }

  if (CTX->classification == DSR_NONE && CTX->source != DSR_NONE)
  {
    LOG(LOG_WARNING, "A source was specified but no classification");
    return EINVAL;
  }
 
  if (CTX->flags & DSF_CHAINED && CTX->flags & DSF_SBPH)
  { 
    LOG(LOG_WARNING, "DSF_SBPH may not be used with DSF_CHAINED");
    return EINVAL;
  }
 
  CTX->_process_start = time (NULL);

  /* Set TOE mode if data is mature enough */
  if ((CTX->training_mode == DST_TOE     &&
      CTX->operating_mode == DSM_PROCESS &&
      CTX->classification == DSR_NONE    &&
      CTX->totals.innocent_learned > 100) || 
     (CTX->training_mode  == DST_NOTRAIN  &&
      CTX->operating_mode == DSM_PROCESS  &&
      CTX->classification == DSR_NONE))
  {
    CTX->operating_mode = DSM_CLASSIFY;
    is_toe = 1;
  }

  /* A signature has been presented for training; process it */
// HERE
/*
  if (CTX->operating_mode == DSM_PROCESS && 
      CTX->classification != DSR_NONE    &&
      CTX->flags & DSF_SIGNATURE         &&
   (! (CTX->flags & DSF_SBPH)))
  {
    int i = _ds_process_signature (CTX);
    if (is_toe)
      CTX->operating_mode = DSM_PROCESS;
    return i;
  }
*/

  /* From this point on, logic assumes that there will be no signature-based
     classification (as it was forked off from the above call) */

  header = buffer_create (NULL);
  body   = buffer_create (NULL);

  if (header == NULL || body == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    buffer_destroy (header);
    buffer_destroy (body);
    if (is_toe)
      CTX->operating_mode = DSM_PROCESS;
    return EUNKNOWN;
  }

  /* Actualize the message if it hasn't already been by the client app */
  if (CTX->message == NULL && message != NULL)
  {
    CTX->message = _ds_actualize_message (message);
  }

  /* If a signature was provided for classification, we don't need to
     analyze the message */
 
  if ( ! (CTX->flags & DSF_SIGNATURE          &&
          CTX->operating_mode == DSM_CLASSIFY &&
          CTX->signature != NULL) )
  {
    _ds_degenerate_message(CTX, header, body);
  }

  CTX->result = -1;

  if (
// HERE
// CTX->flags & DSF_SBPH &&
      CTX->operating_mode != DSM_CLASSIFY && 
      CTX->classification != DSR_NONE     &&
      CTX->flags & DSF_SIGNATURE) 
  {
    char *y, *h, *b;
    char *ptrptr;

    y = strdup((const char *) CTX->signature->data);
    h = strtok_r(y, "\001", &ptrptr);
    b = strtok_r(NULL, "\001", &ptrptr);

    spam_result = _ds_operate (CTX, h, b);
  } else {
    spam_result = _ds_operate (CTX, header->data, body->data);
  }

  if (spam_result == DSR_ISSPAM     || 
      spam_result == DSR_ISINNOCENT || 
      spam_result == DSR_ISWHITELISTED)
  {
    if (CTX->classification == DSR_ISINNOCENT)
      spam_result = DSR_ISINNOCENT;
    else if (CTX->classification == DSR_ISSPAM)
      spam_result = DSR_ISSPAM;
  }

  buffer_destroy (header);
  buffer_destroy (body);

  CTX->result = spam_result;

  if (is_toe)
    CTX->operating_mode = DSM_PROCESS;

  if (CTX->result == DSR_ISSPAM     ||
      CTX->result == DSR_ISINNOCENT ||
      CTX->result == DSR_ISWHITELISTED)
    return 0;
  else
  {
    LOG(LOG_WARNING, "received invalid result (! DSR_ISSPAM || DSR_INNOCENT || DSR_ISWHITELISTED): %d", CTX->result);
    return EUNKNOWN;
  }
}

/*
 *   The dspam_getsource() function extracts the source sender from the mes-
 *   sage  passed  in  during  a call to dspam_process() and writes not more
 *   than size bytes to buf.
 */

int
dspam_getsource (DSPAM_CTX * CTX,
		 char *buf,	
		 size_t size)
{
  struct _ds_message_block *current_block;
  struct _ds_header_field *current_heading = NULL;
  struct nt_node *node_nt;
  struct nt_c c;

  if (CTX->message == NULL)
    return EINVAL;

  node_nt = c_nt_first (CTX->message->components, &c);
  if (node_nt == NULL)
    return EINVAL;

  current_block = (struct _ds_message_block *) node_nt->ptr;

  node_nt = c_nt_first (current_block->headers, &c);
  while (node_nt != NULL)
  {
    current_heading = (struct _ds_header_field *) node_nt->ptr;
    if (!strcmp (current_heading->heading, "Received"))
    {
      char *data = strdup (current_heading->data);
      char *ptr = strstr (data, "from");
      char *tok;
      if (ptr != NULL)
      {
        char *ptrptr;
        tok = strtok_r (ptr, "[", &ptrptr);

        if (tok != NULL)
        {
          int whitelisted = 0;

          tok = strtok_r (NULL, "]", &ptrptr);
          if (tok != NULL)
          {
            if (!strcmp (tok, "127.0.0.1"))
              whitelisted = 1;

            if (_ds_match_attribute(CTX->config->attributes, "LocalMX", tok))
              whitelisted = 1;

            if (!whitelisted)
            {
              strlcpy (buf, tok, size);
              free (data);
              return 0;
            }
          }
        }
      }
      free (data);
    }
    node_nt = c_nt_next (current_block->headers, &c);
  }

  LOGDEBUG("dspam_getsource() failed")
  return EFAILURE;
}

/*
 *  _ds_operate() - operate on the message
 *    calculate the statistical probability the email is spam
 *    update tokens in dictionary according to result/mode
 *
 *  parameters: DSPAM_CTX *CTX		pointer to context
 *              char *header		pointer to message header
 *              char *body		pointer to message body
 *
 *     returns: DSR_ISSPAM		message is spam
 *              DSR_ISINNOCENT		message is innocent
 *              DSR_ISWHITELISTED	message is whitelisted
 */

int
_ds_operate (DSPAM_CTX * CTX, char *headers, char *body)
{
  char *token;				/* current token */
  char joined_token[32];		/* used for de-obfuscating tokens */
  char *previous_token = NULL;		/* used for chained tokens */
  char *previous_tokens[SBPH_SIZE];	/* used for k-mapped tokens */

  /* Variables for Bayesian Noise Reduction */
#ifdef BNR_DEBUG
  struct tbt *pindex = tbt_create();
#endif

  char *line = NULL;			/* header broken up into lines */
  char *url_body;			/* urls broken up */

  char heading[128];			/* current heading */
  int alloc_joined = 0;			/* track joined token free()'s */
  int i = 0;
  int errcode = 0;

  /* Long Hashed Token Tree: Track tokens, frequencies, and stats */
  struct lht *freq = lht_create (24593);
  struct lht *pfreq = lht_create (1543);
  struct lht_node *node_lht;
  struct lht_c c_lht;

  struct tbt *index = tbt_create ();    /* Binary tree index */

  struct nt *header = NULL;     /* header array */
  struct nt_node *node_nt;
  struct nt_c c_nt;
  long body_length = 0;

  unsigned long long whitelist_token = 0;
  int do_whitelist = 0;
  int result = -1;

  /* Allocate SBPH signature (Message Text) */

  if (
//HERE
//CTX->flags & DSF_SBPH   &&
      CTX->flags & DSF_SIGNATURE && 
     ((CTX->operating_mode != DSM_CLASSIFY && CTX->classification == DSR_NONE)
      || ! (CTX->_sig_provided)) && 
      CTX->source != DSS_CORPUS)
  {
    CTX->signature = calloc (1, sizeof (struct _ds_spam_signature));
    if (CTX->signature == NULL)
    {
      LOG (LOG_CRIT, "memory allocation error");
      errcode = EUNKNOWN;
      goto bail;
    }
                                                                                
    CTX->signature->length = strlen(headers)+strlen(body)+2;
    CTX->signature->data = malloc(CTX->signature->length);

    if (CTX->signature->data == NULL)
    {
      LOG (LOG_CRIT, "memory allocation error");
      free (CTX->signature);
      CTX->signature = NULL;
      errcode = EUNKNOWN; 
      goto bail;
    }

    strcpy(CTX->signature->data, headers);
    strcat(CTX->signature->data, "\001");
    strcat(CTX->signature->data, body);
  }

  if (body != NULL)
    body_length = strlen(body);

  if (CTX->flags & DSF_SBPH)
    for(i=0;i<SBPH_SIZE;i++)
      previous_tokens[i] = NULL;
    
  joined_token[0] = 0;

  if (freq == NULL || index == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    errcode = EUNKNOWN;
    goto bail;
  }

  CTX->result = 
    (CTX->classification == DSR_ISSPAM) ? DSR_ISSPAM : DSR_ISINNOCENT;

  /* If we are classifying based on a signature, preprogram the tree */

  if (CTX->flags & DSF_SIGNATURE          &&
      CTX->operating_mode == DSM_CLASSIFY && 
      CTX->_sig_provided)
  {
    int num_tokens =
      CTX->signature->length / sizeof (struct _ds_signature_token);
    struct _ds_signature_token t;

    for (i = 0; i < num_tokens; i++)
    {
      char x[128];
      memcpy (&t,
              (char *) CTX->signature->data +
              (i * sizeof (struct _ds_signature_token)),
              sizeof (struct _ds_signature_token));
      snprintf (x, sizeof (x), "E: %" LLU_FMT_SPEC, t.token);
      lht_hit (freq, t.token, x);
      lht_setfrequency (freq, t.token, t.frequency);
    }
  }

  /* Otherwise, tokenize the message and propagate the tree */

  else
  {
    char *ptrptr;

    header = nt_create (NT_CHAR);
    if (header == NULL)
    {
      LOG (LOG_CRIT, ERROR_MEM_ALLOC);
      errcode = EUNKNOWN;
      goto bail;
    }

    /* HEADER: Split up the text into tokens, include heading */
    line = strtok_r (headers, "\n", &ptrptr);

    while (line != NULL)
    {
      nt_add (header, line);
      line = strtok_r (NULL, "\n", &ptrptr);
    }

    node_nt = c_nt_first (header, &c_nt);
    heading[0] = 0;
    while (node_nt != NULL)
    {
      int is_received, multiline;
      joined_token[0] = 0;
      alloc_joined = 0;

      if (CTX->flags & DSF_SBPH)
        _ds_sbph_clear(previous_tokens);

      line = node_nt->ptr;
      token = strtok_r (line, ":", &ptrptr);
      if (token != NULL && token[0] != 32 && token[0] != 9
          && !strstr (token, " "))
      {
        multiline = 0;
        strlcpy (heading, token, 128);
        if (alloc_joined) {
          free(previous_token);
          alloc_joined = 0;
        }
        previous_token = NULL;
        if (CTX->flags & DSF_SBPH)
          _ds_sbph_clear(previous_tokens);
      } else {
        multiline = 1;
      }

#ifdef VERBOSE
      LOGDEBUG ("Reading '%s' header from: '%s'", heading, line);
#endif

      if (CTX->flags & DSF_WHITELIST) {
        /* Track the entire From: line for auto-whitelisting */

        if (!strcmp(heading, "From")) {
          char wl[256];
          char *fromline = line + 5;

          if (fromline[0] == 32) 
            fromline++;

          snprintf(wl, sizeof(wl), "%s*%s", heading, fromline);
          whitelist_token = _ds_getcrc64(wl); 
          lht_hit (freq, whitelist_token, wl);
          freq->whitelist_token = whitelist_token;
        }
      }

      is_received = (!strcmp (heading, "Received") ? 1 : 0);

      if (is_received)
        token = strtok_r ((multiline) ? line : NULL, DELIMITERS_HEADING, &ptrptr);
      else
        token = strtok_r ((multiline) ? line : NULL, DELIMITERS, &ptrptr);

      while (token != NULL)
      {
        int l;

        l = strlen (token);
        if (l > 1 && l < 25)
        {

#ifdef VERBOSE
          LOGDEBUG ("Processing '%s' token in '%s' header", token, heading);
#endif

          /* If we had to join a token together (e.g. S E X), process it */
          if (joined_token[0] != 0)
          {
            if (strlen (joined_token) < 25 && joined_token[1] != 0)
            {
              if (!_ds_process_header_token
                  (CTX, joined_token,
                   previous_token, freq, heading)
                  && (CTX->flags & DSF_CHAINED))
              {
                alloc_joined = 1;
                previous_token = strdup (joined_token);
              }
              /* Map the joined token */
              if (CTX->flags & DSF_SBPH)
                _ds_map_header_token (CTX, joined_token, previous_tokens, freq, heading);
            }
            joined_token[0] = 0;
          }

          if (!_ds_process_header_token
              (CTX, token, previous_token, freq,
               heading) && (CTX->flags & DSF_CHAINED))
          {
            if (alloc_joined)
            {
              free (previous_token);
              alloc_joined = 0;
            }
            previous_token = token;
          }

          /* Map the joined token */
          if (CTX->flags & DSF_SBPH)
            _ds_map_header_token (CTX, token, previous_tokens, freq, heading);
        }
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
      node_nt = c_nt_next (header, &c_nt);

      if (joined_token[0] != 0)
      {
        if (strlen (joined_token) < 25 && joined_token[1] != 0)
        {
          _ds_process_header_token (CTX, joined_token,
                                    previous_token, freq, heading);
          /* Map the joined token */
          if (CTX->flags & DSF_SBPH)
            _ds_map_header_token (CTX, joined_token, previous_tokens, freq, heading);
        }
      }

      if (alloc_joined) {
        free(previous_token);
        previous_token = NULL;
        alloc_joined = 0;
      }
    }

    nt_destroy (header);

    previous_token = NULL;

    if (CTX->flags & DSF_SBPH)
      _ds_sbph_clear(previous_tokens);

    /* BODY: Split up URLs into tokens, count frequency */

    if (body != NULL) 
      url_body = strdup (body);
    else
      url_body = NULL;
    if (url_body != NULL)
    {
      char combined_token[256];
      char *url_ptr = url_body;
      int url_length;
      unsigned long long crc;

      token = strstr (url_ptr, "http://");
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
            lht_hit (freq, crc, combined_token);
            urltoken = strtok_r (NULL, DELIMITERS, &ptrurl2);
          }

          memset (body + (url_ptr - url_body), 32, url_length);
          url_ptr += url_length + 1;
	  if (url_ptr >= url_end)
            token = NULL;
	  else
            token = strstr (url_ptr, "http://");
        }
        else
          token = NULL;
      }
      free (url_body);
    }

    if (body != NULL)
      url_body = strdup (body);
    else
      url_body = NULL;
    if (url_body != NULL)
    {
      char combined_token[256];
      char *url_ptr = url_body;
      int url_length;
      unsigned long long crc;

      url_ptr = url_body;
      token = strstr (url_ptr, "href=\"");
      while (token != NULL)
      {
        char *urltoken;
        char *ptrurl;

        url_ptr = token + 6;

        urltoken = strtok_r (url_ptr, " \n\">", &ptrurl);
        if (urltoken != NULL)
        {
          char *urlind;
          char *ptrind;
          url_length = strlen (urltoken);

          /* Individual tokens form the URL */
          urlind = strtok_r (urltoken, DELIMITERS, &ptrind);
          while (urlind != NULL)
          {
            snprintf (combined_token, sizeof (combined_token), "Url*%s",
                      urlind);
            crc = _ds_getcrc64 (combined_token);
            lht_hit (freq, crc, combined_token);
            urlind = strtok_r (NULL, DELIMITERS, &ptrind);
          }

          memset (body + ((token + 6) - url_body), 32, url_length);

          url_ptr += url_length + 1;
          token = strstr (url_ptr, "href=\"");
        }
        else
          token = NULL;
      }

      free (url_body);
    }

    /* BODY: Split up the text into tokens, count frequency */
    joined_token[0] = 0;
    alloc_joined = 0;
    
    token = strtok_r (body, DELIMITERS, &ptrptr);
    while (token != NULL)
    {
      int l = strlen (token);
      if (l > 1 && l < 25)
      {
        /* If we had to join a token together (e.g. S E X), process it */
        if (joined_token[0] != 0)
        {
          if (strlen (joined_token) < 25 && joined_token[1] != 0)
          {
            if (!_ds_process_body_token
                (CTX, joined_token,
                 previous_token, freq) && (CTX->flags & DSF_CHAINED))
            {
              alloc_joined = 1;
              previous_token = strdup (joined_token);
            }
                                                                                
            /* Map joined token */
            if (CTX->flags & DSF_SBPH)
              _ds_map_body_token (CTX, joined_token, previous_tokens, freq);

          }
          joined_token[0] = 0;
        }

        if (!_ds_process_body_token
            (CTX, token, previous_token, freq) && (CTX->flags & DSF_CHAINED))
        {
          if (alloc_joined)
          {
            alloc_joined = 0;
            free (previous_token);
          }
          previous_token = token;
        }

        if (CTX->flags & DSF_SBPH)
          _ds_map_body_token (CTX, token, previous_tokens, freq);
      }

      else if (l == 1
               || (l == 2 && (strchr (token, '$') || strchr (token, '!'))))
      {
        strlcat (joined_token, token, sizeof (joined_token));
      }
      token = strtok_r (NULL, DELIMITERS, &ptrptr);
    }


    if (joined_token[0] != 0)
    {
      if (strlen (joined_token) < 25 && joined_token[1] != 0)
      {
        _ds_process_body_token (CTX, joined_token, previous_token, freq);
                                                                                
        /* Map joined token */
        if (CTX->flags & DSF_SBPH)
          _ds_map_body_token (CTX, joined_token, previous_tokens, freq);
      }
    }
  }

  /* Load all token statistics */
  if (_ds_getall_spamrecords (CTX, freq))
  {
    LOGDEBUG ("_ds_getall_spamrecords() failed");
    errcode = EUNKNOWN;
    goto bail;
  }

  /*
     Bayesian Noise Reduction - Progressive Noise Logic
     http://www.nuclearelephant.com/projects/dspam/bnr.html 
  */

  if (CTX->flags & DSF_NOISE)
  {
    struct lht_node *node_lht;
    struct lht_c c_lht;
    BNR_CTX BTX;
    float snr;

    bnr_pattern_instantiate(CTX, pfreq, freq->order, 's');
    bnr_pattern_instantiate(CTX, pfreq, freq->chained_order, 'c');

    LOGDEBUG("Loading %ld BNR patterns", pfreq->items);
    if (_ds_getall_spamrecords (CTX, pfreq))
    {
      LOGDEBUG ("_ds_getall_spamrecords() failed");
      errcode = EUNKNOWN;
      goto bail;
    }

    /* Calculate BNR pattern probability */
    node_lht = c_lht_first (pfreq, &c_lht);
    while(node_lht != NULL) {
      _ds_calc_stat(CTX, node_lht->key, &node_lht->s, DTT_BNR);
      node_lht = c_lht_next(pfreq, &c_lht);
    }

    /* Perform BNR Processing */

    if (CTX->classification == DSR_NONE &&
        CTX->totals.innocent_learned + CTX->totals.innocent_classified > 2500) {
      BTX.stream = freq->order;
      BTX.total_eliminations = 0;
      BTX.total_clean = 0;
      BTX.patterns = pfreq;
      BTX.type = 's';
      bnr_filter_process(CTX, &BTX);

      BTX.stream = freq->chained_order;
      BTX.type = 'c';
      bnr_filter_process(CTX, &BTX);

      if (BTX.total_clean + BTX.total_eliminations > 0)
        snr = 100.0*((BTX.total_eliminations+0.0)/
              (BTX.total_clean+BTX.total_eliminations));
      else
        snr = 0;

      LOGDEBUG("bnr_filter_process() reported snr of %02.3f (%ld %ld)", 
               snr, BTX.total_eliminations, BTX.total_clean); 


#ifdef BNR_VERBOSE_DEBUG
      printf("BNR FILTER PROCESS RESULTS:\n");
      printf("Eliminations:\n");
      node_nt = c_nt_first(freq->order, &c_nt);
      while(node_nt != NULL) {
        node_lht = (struct lht_node *) node_nt->ptr;
        if (node_lht->frequency <= 0)
          printf("%s ", node_lht->token_name);
        node_nt = c_nt_next(freq->order, &c_nt);
      }
      printf("\n[");
      node_nt = c_nt_first(freq->order, &c_nt);
      while(node_nt != NULL) {
        node_lht = (struct lht_node *) node_nt->ptr;
        if (node_lht->frequency <= 0)
          printf("%1.2f ", node_lht->s.probability);
        node_nt = c_nt_next(freq->order, &c_nt);
      }
  
      printf("]\n\nRemaining:\n");
      node_nt = c_nt_first(freq->order, &c_nt);
      while(node_nt != NULL) {
        node_lht = (struct lht_node *) node_nt->ptr;
        if (node_lht->frequency > 0)
          printf("%s ", node_lht->token_name);
        node_nt = c_nt_next(freq->order, &c_nt);
      }
      printf("\n[");
       node_nt = c_nt_first(freq->order, &c_nt);
       while(node_nt != NULL) {
         node_lht = (struct lht_node *) node_nt->ptr;
         if (node_lht->frequency > 0)
  
          printf("%1.2f ", node_lht->s.probability);
         node_nt = c_nt_next(freq->order, &c_nt);
      }

      printf("]\n\n");
#endif

    }

    /* Add BNR pattern to token hash */
//    if (CTX->classification != DSR_NONE)
    {
      node_lht = c_lht_first (pfreq, &c_lht);
      while(node_lht != NULL) {
        lht_hit(freq, node_lht->key, node_lht->token_name);
        lht_setspamstat(freq, node_lht->key, &node_lht->s);
        lht_setfrequency(freq, node_lht->key, 1);
  
        LOGDEBUG("BNR Pattern: %s %01.5f %lds %ldi",
                 node_lht->token_name,
                 node_lht->s.probability,
                 node_lht->s.spam_hits,
                 node_lht->s.innocent_hits);
  
#ifdef BNR_DEBUG
        tbt_add (pindex, node_lht->s.probability, node_lht->key,
               node_lht->frequency, 1);
#endif
        node_lht = c_lht_next(pfreq, &c_lht);
      }
    }
  }

  if (CTX->flags & DSF_WHITELIST)
  {
    LOGDEBUG("Whitelist threshold: %d", CTX->wh_threshold);
  }

  /* Create a binary tree index sorted by a token's delta from .5 */
  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    if (!(CTX->flags & DSF_NOISE) || CTX->classification != DSR_NONE)
      _ds_calc_stat (CTX, node_lht->key, &node_lht->s, DTT_DEFAULT);

    if (CTX->flags & DSF_WHITELIST) {
      if (node_lht->key == whitelist_token              && 
          node_lht->s.spam_hits == 0                    && 
          node_lht->s.innocent_hits > CTX->wh_threshold && 
          CTX->classification == DSR_NONE)
      {
        do_whitelist = 1;
      }
    }

    if (CTX->flags & DSF_SBPH)
      node_lht->frequency = 1;

    if (node_lht->frequency > 0)
    {
      tbt_add (index, node_lht->s.probability, node_lht->key,
             node_lht->frequency, _ds_compute_complexity(node_lht->token_name));
    }

     
#ifdef VERBOSE
    LOGDEBUG ("Token: %s [%f]", node_lht->token_name,
              node_lht->s.probability);
#endif

    node_lht = c_lht_next (freq, &c_lht);
  }

  /* Take the 15 most interesting tokens and generate a score */

  if (index->items == 0)
  {
    LOGDEBUG ("no tokens found in message");
    errcode = EINVAL; 
    goto bail;
  }

  /* Initialize Non-SBPH signature, if requested */

//HERE
/*
  if (! (CTX->flags & DSF_SBPH) &&
       CTX->flags & DSF_SIGNATURE && 
       (CTX->operating_mode != DSM_CLASSIFY || ! CTX->_sig_provided))
  {
    CTX->signature = calloc (1, sizeof (struct _ds_spam_signature));
    if (CTX->signature == NULL)
    {
      LOG (LOG_CRIT, "memory allocation error");
      errcode = EUNKNOWN;
      goto bail;
    }

    CTX->signature->length =
      sizeof (struct _ds_signature_token) * freq->items;
    CTX->signature->data = malloc (CTX->signature->length);
    if (CTX->signature->data == NULL)
    {
      LOG (LOG_CRIT, "memory allocation error");
      free (CTX->signature);
      CTX->signature = NULL;
      errcode = EUNKNOWN;
      goto bail;
    }
  }
*/

#ifdef BNR_DEBUG
  LOGDEBUG("Calculating BNR Result");
  result = _ds_calc_result(CTX, pindex, pfreq);
  LOGDEBUG ("message result: %s", (result != DSR_ISSPAM) ? "NOT SPAM" : "SPAM");
  tbt_destroy(pindex);
#endif

  result = _ds_calc_result(CTX, index, freq);

  if (CTX->flags & DSF_WHITELIST && do_whitelist) {
    LOGDEBUG("auto-whitelisting this message");
    CTX->result = DSR_ISWHITELISTED;
  }

  /* Update Totals */

  /* SPAM */
  if (CTX->result == DSR_ISSPAM && CTX->operating_mode != DSM_CLASSIFY)
  {
    if (!(CTX->flags & DSF_UNLEARN)) {
      CTX->totals.spam_learned++;
      CTX->learned = 1;
    }

    if (CTX->classification == DSR_ISSPAM) 
    {
      if (CTX->flags & DSF_UNLEARN) {
        CTX->totals.spam_learned -= (CTX->totals.spam_learned > 0) ? 1 : 0;
      } else if (CTX->source == DSS_CORPUS || CTX->source == DSS_INOCULATION) {
        CTX->totals.spam_corpusfed++;
      }
      else if (SPAM_MISS(CTX))
      {
        CTX->totals.spam_misclassified++;
        CTX->totals.innocent_learned -=
          (CTX->totals.innocent_learned > 0) ? 1 : 0;
      }
    }

    /* INNOCENT */
  }
  else if ((CTX->result == DSR_ISINNOCENT || CTX->result == DSR_ISWHITELISTED) 
    && CTX->operating_mode != DSM_CLASSIFY)
  {
    if (!(CTX->flags & DSF_UNLEARN)) {
      CTX->totals.innocent_learned++;
      CTX->learned = 1;
    }

    if (CTX->source == DSS_CORPUS || CTX->source == DSS_INOCULATION)
    {
      CTX->totals.innocent_corpusfed++;
    }
    else if (FALSE_POSITIVE(CTX))
    {
      if (CTX->flags & DSF_UNLEARN) {
        CTX->totals.innocent_learned -= (CTX->totals.innocent_learned >0) ? 1:0;
      } else {
        CTX->totals.innocent_misclassified++;
        CTX->totals.spam_learned -= (CTX->totals.spam_learned > 0) ? 1 : 0;
      }
    }
  }

  /* TOE mode increments 'classified' totals */
  if (CTX->training_mode == DST_TOE && CTX->operating_mode == DSM_CLASSIFY) {
    if (CTX->result == DSR_ISSPAM) 
      CTX->totals.spam_classified++;
    else if (CTX->result == DSR_ISINNOCENT || CTX->result == DSR_ISWHITELISTED)
      CTX->totals.innocent_classified++;
  }

  /* Increment and Store Tokens */

  i = 0;
  node_lht = c_lht_first(freq, &c_lht);
  while (node_lht != NULL)
  {
    unsigned long long crc;

    crc = node_lht->key;

    /* Create a signature if we're processing a message */

// HERE
/*
    if ((!(CTX->flags & DSF_SBPH))  && 
        CTX->flags & DSF_SIGNATURE && 
       (CTX->operating_mode != DSM_CLASSIFY || !(CTX->_sig_provided)))
    {
      struct _ds_signature_token t;

      memset(&t, 0, sizeof(t));
      t.token = crc;
      t.frequency = lht_getfrequency (freq, t.token);
      memcpy ((char *) CTX->signature->data +
              (i * sizeof (struct _ds_signature_token)), &t,
              sizeof (struct _ds_signature_token));
    }
*/

    /* If classification was provided, force probabilities */
    if (CTX->classification == DSR_ISSPAM)
      node_lht->s.probability = 1.00;
    else if (CTX->classification == DSR_ISINNOCENT) 
      node_lht->s.probability = 0.00;

    if (CTX->training_mode != DST_TUM  || 
        CTX->source == DSS_ERROR       ||
        CTX->source == DSS_INOCULATION ||
        node_lht->s.spam_hits + node_lht->s.innocent_hits < 50 ||
        !strncmp(node_lht->token_name, "bnr.", 4))
    {
      node_lht->s.status |= TST_DIRTY;
    }

    /* SPAM */
    if (CTX->result == DSR_ISSPAM)
    {
      /* Inoculations increase token count considerably */
      if (CTX->source == DSS_INOCULATION)
      {
        if (node_lht->s.innocent_hits < 2 && node_lht->s.spam_hits < 5)
          node_lht->s.spam_hits += 5;
        else
          node_lht->s.spam_hits += 2;
      }

      /* Standard increase */
      else
      {
        if (CTX->flags & DSF_UNLEARN) {
          if (CTX->classification == DSR_ISSPAM)
            node_lht->s.spam_hits -= (node_lht->s.spam_hits>0) ? 1:0;
        } else {
          node_lht->s.spam_hits++;
        }
      }

//HERE
      if (SPAM_MISS(CTX) && !(CTX->flags & DSF_UNLEARN)) { 
        node_lht->s.innocent_hits-= 1;//(node_lht->s.innocent_hits>0) ? 1:0;
//        if (node_lht->s.innocent_hits < 0)
//          node_lht->s.innocent_hits = 0;

      }
    }


    /* INNOCENT */
    else
    {
      if (CTX->flags & DSF_UNLEARN) { 
        if (CTX->classification == DSR_ISINNOCENT)
          node_lht->s.innocent_hits-= (node_lht->s.innocent_hits>0) ? 1:0;
      } else {
        node_lht->s.innocent_hits++;
      }

      if (FALSE_POSITIVE(CTX) && !(CTX->flags & DSF_UNLEARN))
      {

        node_lht->s.spam_hits-= 1;//(node_lht->s.spam_hits>0) ? 1:0;
//        if (node_lht->s.spam_hits < 0)
//          node_lht->s.spam_hits = 0;
      }
    }

    node_lht = c_lht_next(freq, &c_lht);
    i++;
  }

  /* Store all tokens */
  if (_ds_setall_spamrecords (CTX, freq))
  {
    LOGDEBUG ("_ds_setall_spamrecords() failed");
    errcode = EUNKNOWN;
    goto bail;
  }

  tbt_destroy (index);
  lht_destroy (freq);
  lht_destroy (pfreq);

  /* One final sanity check */

  if (CTX->classification == DSR_ISINNOCENT)
  {
    CTX->probability = 0.0;
    CTX->result = DSR_ISINNOCENT;
  }
  else if (CTX->classification == DSR_ISSPAM)
  {
    CTX->probability = 1.0;
    CTX->result = DSR_ISSPAM;
  }

  return CTX->result;

bail:
  tbt_destroy (index);
  lht_destroy (freq);
  lht_destroy (pfreq);
  return errcode;
}

/*
 *  _ds_process_signature() - process an erroneously classified message 
 *    processing based on signature
 *
 *   parameters: DSPAM_CTX *CTX		pointer to classification context
 *                                      where CTX->signature is present
 */

int
_ds_process_signature (DSPAM_CTX * CTX)
{
  struct _ds_signature_token t;
  int num_tokens, i;
  struct lht *freq = lht_create (24593);
  struct lht_node *node_lht;
  struct lht_c c_lht;

  if (CTX->signature == NULL)
  {
    LOG(LOG_WARNING, "DSF_SIGNATURE specified, but no signature provided.");
    return EINVAL;
  }

  if (freq == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  LOGDEBUG ("processing signature.  length: %ld", CTX->signature->length);

  CTX->result = -1;

  if (!(CTX->flags & DSF_UNLEARN)) 
    CTX->learned = 1;

  /* INNOCENT */
  if (CTX->classification == DSR_ISINNOCENT && 
      CTX->operating_mode != DSM_CLASSIFY)
  {
    if (CTX->flags & DSF_UNLEARN) {
      CTX->totals.innocent_learned -= (CTX->totals.innocent_learned) > 0 ? 1:0;
    } else {
      if (CTX->source == DSS_ERROR) {
        CTX->totals.innocent_misclassified++;
        if ((CTX->training_mode != DST_TOE || CTX->totals.innocent_learned <= 100)
            && CTX->training_mode != DST_NOTRAIN)
          CTX->totals.spam_learned -= (CTX->totals.spam_learned > 0) ? 1:0;
      } else {
        CTX->totals.innocent_corpusfed++;
      }

      CTX->totals.innocent_learned++;
    }
  }

  /* SPAM */
  else if (CTX->classification == DSR_ISSPAM &&
           CTX->operating_mode != DSM_CLASSIFY)
  {
    if (CTX->flags & DSF_UNLEARN) {
      CTX->totals.spam_learned -= (CTX->totals.spam_learned > 0) ? 1 : 0;
    } else {
      if (CTX->source == DSS_ERROR) {
        CTX->totals.spam_misclassified++;
        if ((CTX->training_mode != DST_TOE || CTX->totals.innocent_learned <= 100)
          && CTX->training_mode != DST_NOTRAIN)
          CTX->totals.innocent_learned -= (CTX->totals.innocent_learned > 0) ? 1:0;
      } else {
        CTX->totals.spam_corpusfed++;
      }
      CTX->totals.spam_learned++;
    }
  }

  num_tokens = CTX->signature->length / sizeof (struct _ds_signature_token);

  LOGDEBUG ("reversing %d tokens", num_tokens);
  for (i = 0; i < num_tokens; i++)
  {
    memcpy (&t,
            (char *) CTX->signature->data +
            (i * sizeof (struct _ds_signature_token)),
            sizeof (struct _ds_signature_token));
    lht_hit (freq, t.token, "-");
    lht_setfrequency (freq, t.token, t.frequency);
  }

  if (_ds_getall_spamrecords (CTX, freq))
    return EUNKNOWN;

  node_lht = c_lht_first (freq, &c_lht);
  while (node_lht != NULL)
  {
    /* INNOCENT */
    if (CTX->classification == DSR_ISINNOCENT)
    {
      if (CTX->flags & DSF_UNLEARN) {
        node_lht->s.innocent_hits-= (node_lht->s.innocent_hits>0) ? 1:0;
      } else {
        node_lht->s.innocent_hits++;
        if (CTX->source == DSS_ERROR &&
          (CTX->training_mode != DST_TOE || CTX->totals.innocent_learned <= 100)
          && CTX->training_mode != DST_NOTRAIN)

          node_lht->s.spam_hits -= (node_lht->s.spam_hits > 0) ? 1 : 0;
      }
    }

    /* SPAM */
    else if (CTX->classification == DSR_ISSPAM)
    {
      if (CTX->flags & DSF_UNLEARN) {
        node_lht->s.spam_hits -= (node_lht->s.spam_hits>0) ? 1 :0;
      } else {
       if (CTX->source == DSS_ERROR &&
          (CTX->training_mode != DST_TOE || CTX->totals.innocent_learned <= 100)
          && CTX->training_mode != DST_NOTRAIN)

          node_lht->s.innocent_hits -= (node_lht->s.innocent_hits > 0) ? 1 : 0;

        if (CTX->source == DSS_INOCULATION)
        {
          if (node_lht->s.innocent_hits < 2 && node_lht->s.spam_hits < 5)
            node_lht->s.spam_hits += 5;
          else
            node_lht->s.spam_hits += 2;
        } else /* ERROR or CORPUS */
        {
          node_lht->s.spam_hits++;
        }
      }
    }
    node_lht->s.status |= TST_DIRTY;
    node_lht = c_lht_next (freq, &c_lht);
  }

  if (_ds_setall_spamrecords (CTX, freq))
    return EUNKNOWN;

  if (CTX->classification == DSR_ISSPAM)
  {
    CTX->probability = 1.0;
    CTX->result = DSR_ISSPAM;
  }
  else
  {
    CTX->probability = 0.0;
    CTX->result = DSR_ISINNOCENT;
  }

  lht_destroy(freq);
  return 0;
}

/*
 *  _ds_calc_stat() - Calculate the probability of a token
 *
 *  Calculates the probability of an individual token based on  the
 *  pvalue algorithm chosen. The resulting value largely depends on
 *  the total  amount of ham/spam in the user's corpus. The  result
 *  is written to s.
 */

int
_ds_calc_stat (DSPAM_CTX * CTX, unsigned long long token,
               struct _ds_spam_stat *s, int token_type)
{

  int min_hits;
  long ti, ts;

  min_hits = (token_type == DTT_BNR) ? 25 : 5;

  ti = CTX->totals.innocent_learned + CTX->totals.innocent_classified;
  ts = CTX->totals.spam_learned + CTX->totals.spam_classified;

  if (CTX->training_buffer) {

    if (ti < 1000 && ti < ts)
    {
      min_hits = min_hits+(CTX->training_buffer/2)+
                   (CTX->training_buffer*((ts-ti)/200));
    }

    if (ti < 2500 && ti >=1000 && ts > ti)
    {
      float spams = (ts * 1.0 / (ts * 1.0 + ti * 1.0)) * 100;
      min_hits = min_hits+(CTX->training_buffer/2)+
                   (CTX->training_buffer*(spams/20));
    }
  }

  if (min_hits < (token_type == DTT_BNR) ? 25 : 5)
    min_hits = (token_type == DTT_BNR) ? 25 : 5;

  if (CTX->training_mode == DST_TUM) {
    if (min_hits>20)
      min_hits = 20;
  }

  if (CTX->classification == DSR_ISSPAM)
    s->probability = .7;
  else
    s->probability = .4;

  if (CTX->totals.spam_learned > 0 && 
      CTX->totals.innocent_learned > 0 &&
        ((s->spam_hits * 1.0 / CTX->totals.spam_learned * 1.0) +
         (s->innocent_hits * 1.0 / CTX->totals.innocent_learned * 1.0)) > 0)
  {
#ifdef BIAS
    s->probability =
      (s->spam_hits * 1.0 / CTX->totals.spam_learned * 1.0) /
      ((s->spam_hits * 1.0 / CTX->totals.spam_learned * 1.0) +
       (s->innocent_hits * 2.0 / CTX->totals.innocent_learned * 1.0));
#else
    s->probability =
      (s->spam_hits * 1.0 / CTX->totals.spam_learned * 1.0) /
      ((s->spam_hits * 1.0 / CTX->totals.spam_learned * 1.0) +
       (s->innocent_hits * 1.0 / CTX->totals.innocent_learned * 1.0));
#endif
  }

//  if (s->innocent_hits < 0)
//    s->innocent_hits = 0;

//  if (s->spam_hits < 0)
//    s->spam_hits = 0;

  if (s->spam_hits == 0 && s->innocent_hits > 0)
  {
    if (s->innocent_hits > 50)
      s->probability = 0.0060;
    else if (s->innocent_hits > 10)
      s->probability = 0.0001;
    else
      s->probability = 0.0002;
  }
  else if (s->spam_hits > 0 && s->innocent_hits == 0)
  {
    if (s->spam_hits >= 10)
      s->probability = 0.9999;
    else
      s->probability = 0.9998;
  }

#ifdef BIAS
  if (s->spam_hits + (2 * s->innocent_hits) < min_hits
      || CTX->totals.innocent_learned < min_hits)
#else
  if (s->spam_hits + s->innocent_hits < min_hits
      || CTX->totals.innocent_learned < min_hits)
#endif
    s->probability = .4;

  if (s->probability < 0.0001)
    s->probability = 0.0001;

  if (s->probability > 0.9999)
    s->probability = 0.9999;

  if (CTX->algorithms & DSP_ROBINSON)
  {
    int n = s->spam_hits + s->innocent_hits;
    double fw = ((CHI_S * CHI_X) + (n * s->probability))/(CHI_S + n);
    s->probability = fw;
  }

  return 0;
}

/*
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
                          const char *previous_token, struct lht *freq,
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
    if (token[i] >= 127 || iscntrl ((unsigned char) token[i])) {
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

  if (!isalnum ((unsigned char) token[0]) && token[0] != '$' && token[0] != '#')
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
  lht_hit (freq, crc, combined_token); 

  if ((CTX->flags & DSF_CHAINED) && previous_token != NULL && !is_received)
  {
    char *tweaked_previous;

    tweaked_previous = _ds_truncate_token(previous_token);
    if (tweaked_previous == NULL)
      return EUNKNOWN;

    snprintf (combined_token, sizeof (combined_token),
              "%s*%s+%s", heading, tweaked_previous, tweaked_token);
    crc = _ds_getcrc64 (combined_token);

    lht_hit (freq, crc, combined_token);
    free(tweaked_previous);
  }

  free(tweaked_token);
  return 0;
}

int
_ds_process_body_token (DSPAM_CTX * CTX, char *token,
                        const char *previous_token, struct lht *freq)
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
    if (token[i] >= 127 || iscntrl ((unsigned char) token[i])) {
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

  if (!isalnum ((unsigned char) token[0]) && token[0] != '$' && token[0] != '#')
    all_num = 1;

  /* Ignore tokens that are all numbers, or contain high ASCII characters */
  if (all_num)
    return EINVAL;

  tweaked_token = _ds_truncate_token(token);
  if (tweaked_token == NULL)
    return EUNKNOWN;

  crc = _ds_getcrc64 (tweaked_token);

  lht_hit (freq, crc, tweaked_token);

  if ((CTX->flags & DSF_CHAINED) && previous_token != NULL)
  {
    char *tweaked_previous = _ds_truncate_token(previous_token);
    if (tweaked_previous == NULL)
      return EUNKNOWN;

    snprintf (combined_token, sizeof (combined_token), "%s+%s",
              tweaked_previous, tweaked_token);
    crc = _ds_getcrc64 (combined_token);

    lht_hit (freq, crc, combined_token);
    free(tweaked_previous);
  }
  free(tweaked_token);

  return 0;
}


int
_ds_map_header_token (DSPAM_CTX * CTX, char *token,
                      char **previous_tokens, struct lht *freq,
                      const char *heading)
{
  int all_num = 1, i, mask, t;
  int len;
  unsigned long long crc;
  char key[256];

  if (_ds_match_attribute(CTX->config->attributes, "IgnoreHeader", heading))
    return 0;

  for (i = 0; token[i] != 0; i++)
  {
    if (!isdigit ((unsigned char) token[i]))
      all_num = 0;
    if (token[i] >= 127 || iscntrl ((unsigned char) token[i])) {
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

  if (!isalnum ((unsigned char) token[0]) && token[0] != '$' && token[0] != '#')
    all_num = 1;

  /* Ignore tokens that are all numbers, or contain high ASCII characters */
  if (all_num)
    return EINVAL;

  /* Shift all previous tokens up */
  free(previous_tokens[0]);
  for(i=0;i<SBPH_SIZE-1;i++)
    previous_tokens[i] = previous_tokens[i+1];

  previous_tokens[SBPH_SIZE-1] = strdup (token);

  /* Iterate and generate all keys necessary */
  for(mask=0;mask < _ds_pow2(SBPH_SIZE);mask++) {
    snprintf(key, sizeof(key), "%s*", heading);
    t = 0;
                                                                                
    /* Each Bit */
    for(i=0;i<SBPH_SIZE;i++) {
      if (t) 
        strlcat(key, "+", sizeof(key));

      if (mask & (_ds_pow2(i+1)/2) && previous_tokens[i]) {
        strlcat(key, previous_tokens[i], sizeof(key));
        t++;
      }
    }

    /* If the bucket has at least 2 literals, hit it */
    if (t>=2) {
      crc = _ds_getcrc64(key);
      lht_hit (freq, crc, key);
    }
  }

  return 0;
}

int
_ds_map_body_token (DSPAM_CTX * CTX, char *token,
                        char **previous_tokens, struct lht *freq)
{
  int all_num = 1, i,  mask, t;
  int len;
  unsigned long long crc;
  char key[256];

  for (i = 0; token[i] != 0; i++)
  {
    if (!isdigit ((unsigned char) token[i]))
      all_num = 0;
    if (token[i] >= 127 || iscntrl ((unsigned char) token[i])) {
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

  if (!isalnum ((unsigned char) token[0]) && token[0] != '$' && token[0] != '#')
    all_num = 1;

  /* Ignore tokens that are all numbers, or contain high ASCII characters */
  if (all_num)
    return EINVAL;

  /* Shift all previous tokens up */
  free(previous_tokens[0]);
  for(i=0;i<SBPH_SIZE-1;i++)
    previous_tokens[i] = previous_tokens[i+1];
                                                                                
  previous_tokens[SBPH_SIZE-1] = strdup (token);
                                                                                
  /* Iterate and generate all keys necessary */
  for(mask=0;mask < _ds_pow2(SBPH_SIZE);mask++) {
    t = 0;

    key[0] = 0;
                                                                                
    /* Each Bit */
    for(i=0;i<SBPH_SIZE;i++) {
      if (t)
        strlcat(key, "+", sizeof(key));
                                                                                
      if (mask & (_ds_pow2(i+1)/2) && previous_tokens[i]) {
        strlcat(key, previous_tokens[i], sizeof(key));
        t++;
      }
    }

    /* If the bucket has at least 2 literals, hit it */
    if (t>=2) {
      while(key[strlen(key)-1] == '+')
        key[strlen(key)-1] = 0;

      crc = _ds_getcrc64(key);
      lht_hit (freq, crc, key);
    }
  }

  return 0;
}

/* 
 *  _ds_degenerate_message() - Degenerate the message into tokenizable pieces
 *
 *  This function is responsible for analyzing the actualized message and
 *  degenerating it into only the components which are tokenizable.  This 
 *  process  effectively eliminates much HTML noise, special symbols,  or
 *  other  non-tokenizable/non-desirable components. What is left  is the
 *  bulk of  the message  and only  desired tags,  URLs, and other  data.
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
            char hex[5];
            int conv;
                                                                                
            strcpy(hex, "0x00");
                                                                                
            x = strchr(decode2, '%');
            while(x != NULL) {
              if (isxdigit((unsigned char) x[1]) && isxdigit((unsigned char) x[2])) {
                hex[2] = x[1];
                hex[3] = x[2];
                                                                                
                conv = strtol(hex, NULL, 16);
                                                                                
                x[0] = conv;
                memmove(x+1, x+3, strlen(x+3));
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

/*
 *  _ds_calc_result() - Perform statistical combination of the token index
 *
 *  Passed in an index of tokens, this function is responsible for choosing
 *  and combining  the most  relevant characteristics  (based on the  algo-
 *  rithms  configured)  and calculating  libdspam's  decision  about  the 
 *  provided message sample.
 */

int _ds_calc_result(DSPAM_CTX *CTX, struct tbt *index, struct lht *freq) {

  struct _ds_spam_stat stat;
  struct tbt_node *node_tbt;

  /* Graham-Bayesian */
  float bay_top = 0.0; 
  float bay_bot = 0.0;
  float bay_result = -1;
  long bay_used = 0;            /* Total tokens used in bayes */
  struct nt *factor_bayes = nt_create(NT_PTR);

  /* Burton-Bayesian */
  double abay_top = 0.0; 
  double abay_bot = 0.0;
  double abay_result = -1;
  long abay_used = 0;           /* Total tokens used in altbayes */
  struct nt *factor_altbayes = nt_create(NT_PTR);

  /* Robinson's Geometric Mean, used to calculate confidence */
  float rob_top = 0.0;      		/* Robinson's Geometric Mean */
  float rob_bot = 0.0;
  float rob_result = -1;
  double p = 0.0, q = 0.0, s = 0.0;     /* Robinson PQS Calculations */
  long rob_used = 0;            	/* Total tokens used in Robinson's GM */
  struct nt *factor_rob = nt_create(NT_PTR);
                                                                                
  /* Fisher-Robinson's Chi-Square */
  float chi_result = -1;
  long chi_used  = 0, chi_sx = 0, chi_hx = 0;
  double chi_s = 1.0, chi_h = 1.0;
  struct nt *factor_chi = nt_create(NT_PTR);

  int i;

  node_tbt = tbt_first (index);

  for (i = 0; i <
    (  ( !(CTX->algorithms & DSA_CHI_SQUARE) && index->items > 27 ) ? 
       27 : (long)index->items); i++)
  {
    unsigned long long crc;
    char *token_name;

    crc = node_tbt->token;
    token_name = lht_gettoken (freq, crc);

    if (lht_getspamstat (freq, crc, &stat) || token_name == NULL)
    {
      node_tbt = tbt_next (node_tbt);
      continue;
    }

    /* Skip BNR patterns if still training */
    if (!strncmp(token_name, "bnr.", 4)) {
      if (CTX->classification != DSR_NONE ||
          CTX->totals.innocent_learned + CTX->totals.innocent_classified 
            <= 2500)
      {
        node_tbt = tbt_next (node_tbt);
        continue;
      }
    }

    /* Set the probability if we've provided a classification */
    if (CTX->classification == DSR_ISSPAM)
      stat.probability = 1.00;
    else if (CTX->classification == DSR_ISINNOCENT)
      stat.probability = 0.00;

    /* BEGIN Combine Token Values */

    /* Graham-Bayesian */
    if (CTX->algorithms & DSA_GRAHAM && bay_used < 15)
    {

        LOGDEBUG ("[graham] [%2.6f] %s (%dfrq, %lds, %ldi)",
                  stat.probability, token_name, lht_getfrequency (freq, crc),
                  stat.spam_hits, stat.innocent_hits);

      _ds_factor(factor_bayes, token_name, stat.probability);

      if (bay_used == 0)
      {
        bay_top = stat.probability;
        bay_bot = 1 - stat.probability;
      }
      else
      {
        bay_top *= stat.probability;
        bay_bot *= (1 - stat.probability);
      }

      bay_used++;
    }

    /* Burton Bayesian */
    if (CTX->algorithms & DSA_BURTON && abay_used < 27)
    {
        LOGDEBUG ("[burton] [%2.6f] %s (%dfrq, %lds, %ldi)",
                  stat.probability, token_name, lht_getfrequency (freq, crc),
                  stat.spam_hits, stat.innocent_hits);

      _ds_factor(factor_altbayes, token_name, stat.probability);

      if (abay_used == 0)
      {
        abay_top = stat.probability;
        abay_bot = (1 - stat.probability);
      }
      else
      {
        abay_top *= stat.probability;
        abay_bot *= (1 - stat.probability);
      }

      abay_used++;

      if (abay_used < 27 && lht_getfrequency (freq, crc) > 1 )
      {
          LOGDEBUG ("[burton] [%2.6f] %s (%dfrq, %lds, %ldi)",
                    stat.probability, token_name, lht_getfrequency (freq,
                                                                    crc),
                    stat.spam_hits, stat.innocent_hits);

        _ds_factor(factor_altbayes, token_name, stat.probability);

        abay_used++;
        abay_top *= stat.probability;
        abay_bot *= (1 - stat.probability);
      }

    }

    /* Robinson's Geometric Mean Definitions */

#define ROB_S	0.010           /* Sensitivity */
#define ROB_X	0.415           /* Value to use when N = 0 */
#define ROB_CUTOFF	0.54

    if (rob_used < 25)
    {
      float probability;
      long n = (index->items > 25) ? 25 : index->items;

      probability = ((ROB_S * ROB_X) + (n * stat.probability)) / (ROB_S + n);

#ifdef ROBINSON
#ifndef VERBOSE
      if (CTX->operating_mode != DSM_CLASSIFY)
      {
#endif
        LOGDEBUG ("[rob] [%2.6f] %s (%dfrq, %lds, %ldi)",
                  stat.probability, token_name, lht_getfrequency (freq, crc),
                  stat.spam_hits, stat.innocent_hits);
#ifndef VERBOSE
      }
#endif
#endif

      _ds_factor(factor_rob, token_name, stat.probability);

      if (probability < 0.3 || probability > 0.7)
      {

        if (rob_used == 0)
        {
          rob_top = probability;
          rob_bot = (1 - probability);
        }
        else
        {
          rob_top *= probability;
          rob_bot *= (1 - probability);
        }

        rob_used++;

        if (rob_used < 25 && lht_getfrequency (freq, crc) > 1)
        {
#ifdef ROBINSON
#ifndef VERBOSE
          if (CTX->operating_mode != DSM_CLASSIFY)
          {
#endif
            LOGDEBUG ("[rob] [%2.6f] %s (%dfrq, %lds, %ldi)",
                      stat.probability, token_name, lht_getfrequency (freq,
                                                                      crc),
                      stat.spam_hits, stat.innocent_hits);

#ifndef VERBOSE
          }
#endif
#endif

          _ds_factor(factor_rob, token_name, stat.probability);

          rob_used++;
          rob_top *= probability;
          rob_bot *= (1 - probability);
        }
      }
    }

    /* Fisher-Robinson's Inverse Chi-Square */
#define CHI_CUTOFF	0.5010	/* Ham/Spam Cutoff */
#define CHI_EXCR	0.4500	/* Exclusionary Radius */
#define LN2		0.69314718055994530942 /* log e2 */
    if (CTX->algorithms & DSA_CHI_SQUARE)
    {
      double fw;
      int n, exp;

      if (CTX->algorithms & DSP_ROBINSON) {
        fw = stat.probability;
      } else {
        n = stat.spam_hits + stat.innocent_hits;
        fw = ((CHI_S * CHI_X) + (n * stat.probability))/(CHI_S + n);
      }

      if (fabs(0.5-fw)>CHI_EXCR) {
        int iter = _ds_compute_complexity(token_name);

iter = 1;
        while(iter>0) {
          iter --;

#ifndef VERBOSE
          if (CTX->operating_mode != DSM_CLASSIFY)
          {
#endif
            LOGDEBUG ("[chi-sq] [%2.6f] %s (%dfrq, %lds, %ldi)",
                      fw, token_name, lht_getfrequency (freq, crc),
                      stat.spam_hits, stat.innocent_hits);
#ifndef VERBOSE
          }
#endif

          _ds_factor(factor_chi, token_name, stat.probability);

          chi_used++;
          chi_s *= (1.0 - fw);
          chi_h *= fw;
          if (chi_s < 1e-200) {
            chi_s = frexp(chi_s, &exp);
            chi_sx += exp;
          }
          if (chi_h < 1e-200) {
            chi_h = frexp(chi_h, &exp);
            chi_hx += exp; 
          }
        }
      }
    }


/* END Combine Token Values */

    node_tbt = tbt_next (node_tbt);
  }

/* BEGIN Calculate Individual Probabilities */

  if (CTX->algorithms & DSA_GRAHAM) {
    bay_result = (bay_top) / (bay_top + bay_bot);
    LOGDEBUG ("Graham-Bayesian Probability: %f Samples: %ld", bay_result,
              bay_used);
  }

  if (CTX->algorithms & DSA_BURTON) {
    abay_result = (abay_top) / (abay_top + abay_bot);
    LOGDEBUG ("Burton-Bayesian Probability: %f Samples: %ld", abay_result,
              abay_used);
  }

  /* Robinson's */
  if (rob_used == 0)
  {
    p = q = s = 0;
  }
  else
  {
    p = 1.0 - pow (rob_bot, 1.0 / rob_used);
    q = 1.0 - pow (rob_top, 1.0 / rob_used);
    s = (p - q) / (p + q);
    s = (s + 1.0) / 2.0;
  }

  rob_result = s;

  if (CTX->algorithms & DSA_ROBINSON) { 
    LOGDEBUG("Robinson's Geometric Confidence: %f (Spamminess: %f, "
      "Non-Spamminess: %f, Samples: %ld)", rob_result, p, q, rob_used);
  }

  if (CTX->algorithms & DSA_CHI_SQUARE) {
    chi_s = log(chi_s) + chi_sx * LN2;
    chi_h = log(chi_h) + chi_hx * LN2;

    if (chi_used) {
      chi_s = 1.0 - chi2Q(-2.0 * chi_s, 2 * chi_used);
      chi_h = 1.0 - chi2Q(-2.0 * chi_h, 2 * chi_used);

      chi_result = ((chi_s-chi_h)+1.0) / 2.0;
    } else {
      chi_result = (float)(CHI_CUTOFF-0.1);
    }  

    LOGDEBUG("Chi-Square Confidence: %f", chi_result); 
  }

/* END Calculate Individual Probabilities */

/* BEGIN Determine Result */

  if (CTX->classification == DSR_ISSPAM) {
    CTX->result = DSR_ISSPAM;
    CTX->probability = 1.0;
  } else if (CTX->classification == DSR_ISINNOCENT) {
    CTX->result = DSR_ISINNOCENT;
    CTX->probability = 0.0;
  } else {
    struct nt *factor = NULL;

    if (CTX->algorithms & DSA_GRAHAM) {
      factor = factor_bayes;
      if (bay_result >= 0.9)
      {
        CTX->result = DSR_ISSPAM;
        CTX->probability = bay_result;
        CTX->factors = factor;
      }
    }

    if (CTX->algorithms & DSA_BURTON) {
      factor = factor_altbayes;
      if (abay_result >= 0.9) 
      {
        CTX->result = DSR_ISSPAM;
        CTX->probability = abay_result;
        if (!CTX->factors)
          CTX->factors = factor;
      }
    }

    if (CTX->algorithms & DSA_ROBINSON) {
      factor = factor_rob;
      if (rob_result >= ROB_CUTOFF) 
      {
        CTX->result = DSR_ISSPAM;
        if (CTX->probability < 0)
          CTX->probability = rob_result;
        if (!CTX->factors)
          CTX->factors = factor;
      }
    }

    if (CTX->algorithms & DSA_CHI_SQUARE) {
     factor = factor_chi;
     if (chi_result >= CHI_CUTOFF)
     {
       CTX->result = DSR_ISSPAM;
       if (CTX->probability < 0)
         CTX->probability = chi_result;
       if (!CTX->factors)
         CTX->factors = factor;
       }
    }

    if (!CTX->factors)
      CTX->factors = factor;
  }

  if (CTX->factors != factor_bayes)
    _ds_factor_destroy(factor_bayes);
  if (CTX->factors != factor_altbayes)
    _ds_factor_destroy(factor_altbayes);
  if (CTX->factors != factor_rob)
    _ds_factor_destroy(factor_rob);
  if (CTX->factors != factor_chi)
    _ds_factor_destroy(factor_chi);

  /* If somehow we haven't yet assigned a probability, assign one */
  if (CTX->probability) 
  {
    if (CTX->algorithms & DSA_GRAHAM)
      CTX->probability = bay_result;

    if (CTX->probability < 0 && CTX->algorithms & DSA_BURTON)
      CTX->probability = abay_result;

    if (CTX->probability < 0 && CTX->algorithms & DSA_ROBINSON)
      CTX->probability = rob_result;

    if (CTX->probability < 0 && CTX->algorithms & DSA_CHI_SQUARE)
      CTX->probability = chi_result;
  }

#ifdef VERBOSE
  if (DO_DEBUG) {
    if (abay_result >= 0.9 && bay_result < 0.9)
    {
      LOGDEBUG ("CATCH: Burton Bayesian");
    }
    else if (abay_result < 0.9 && bay_result >= 0.9)
    {
      LOGDEBUG ("MISS: Burton Bayesian");
    }
                                                                                
    if (rob_result >= ROB_CUTOFF && bay_result < 0.9)
    {
      LOGDEBUG ("CATCH: Robinson's");
    }
    else if (rob_result < ROB_CUTOFF && bay_result >= 0.9)
    {
      LOGDEBUG ("MISS: Robinson's");
    }
                                                                                
    if (chi_result >= CHI_CUTOFF && bay_result < 0.9)
    {
      LOGDEBUG("CATCH: Chi-Square");
    }
    else if (chi_result < CHI_CUTOFF && bay_result >= 0.9)
    {
      LOGDEBUG("MISS: Chi-Square");
    }
  }
#endif

  /* Calculate Confidence */

  if (CTX->result == DSR_ISSPAM)
  {
    CTX->confidence = rob_result;
  }
  else
  {
    CTX->confidence = 1.0 - rob_result;
  }

  return CTX->result;
}

/*
 *  _ds_factor() - Factors a token/value into a set
 *
 *  Adds a token/value pair to a factor set. The factor set of the dominant
 *  calculation  is provided to the  client in order to explain  libdspam's
 *  final decision about the message's classification.
 */
 
int _ds_factor(struct nt *set, char *token_name, float value) {
  struct dspam_factor *f;
  f = calloc(1, sizeof(struct dspam_factor));
  if (!f)
    return EUNKNOWN;
  f->token_name = strdup(token_name);
  f->value = value;
  nt_add(set, (void *) f);
  return 0;
}

/*
 *  _ds_spbh_clear - Clears the SBPH stack
 *
 *  Clears and frees all of the tokens in the SBPH stack. Used when a 
 *  boundary has been crossed (such as a new message header) where
 *  tokens from the previous boundary are no longer useful.
 */
 
void _ds_sbph_clear(char **previous_tokens) {
  int i;
  for(i=0;i<SBPH_SIZE;i++) {
    free(previous_tokens[i]);
    previous_tokens[i] = NULL;
  }
  return;
}


/*
 *  _ds_factor_destroy - Destroys a factor tree
 *
 */

void _ds_factor_destroy(struct nt *factors) {
  struct dspam_factor *f;
  struct nt_node *node;
  struct nt_c c;

  if (factors == NULL)
    return;
  
  node = c_nt_first(factors, &c);
  while(node != NULL) {
    f = (struct dspam_factor *) node->ptr;
    free(f->token_name);
    node = c_nt_next(factors, &c);
  }
  nt_destroy(factors);

  return;
} 
