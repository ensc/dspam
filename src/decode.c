/* $Id: decode.c,v 1.395 2011/09/03 13:25:39 sbajic Exp $ */

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

/*
 * decode.c - message decoding and parsing
 *
 *  DESCRIPTION
 *    This set of functions performs parsing and decoding of a message and
 *    embeds its components into a ds_message_t structure, suitable for
 *    logical access.
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "decode.h"
#include "error.h"
#include "util.h"
#include "language.h"
#include "buffer.h"
#include "base64.h"
#include "libdspam.h"

/*
 * _ds_actualize_message (const char *message)
 *
 * DESCRIPTION
 *   primary message parser
 *
 *   this function performs all decoding and actualization of the message
 *   into the message structures defined in the .h
 *
 * INPUT ARGUMENTS
 *      message    message to decode
 *
 * RETURN VALUES
 *   pointer to an allocated message structure (ds_message_t), NULL on failure
 */

ds_message_t
_ds_actualize_message (const char *message)
{
  char *line = NULL;
  char *in = NULL;
  char *m_in = NULL;
  ds_message_part_t current_block;
  ds_header_t current_heading = NULL;
  struct nt *boundaries = NULL;
  ds_message_t out = NULL;
  int block_position = BP_HEADER;
  int in_content = 0;

  if (!message || !(*message))
    goto MEMFAIL;

  if (!(in = strdup(message)))
    goto MEMFAIL;

  m_in = in;

  boundaries = nt_create (NT_CHAR);
  if (!boundaries)
    goto MEMFAIL;

  out = (ds_message_t) calloc (1, sizeof (struct _ds_message));
  if (!out)
    goto MEMFAIL;

  out->components = nt_create (NT_PTR);
  if (!out->components)
    goto MEMFAIL;

  current_block = _ds_create_message_part ();
  if (!current_block)
    goto MEMFAIL;

  if (nt_add (out->components, (void *) current_block) == NULL)
    goto MEMFAIL;

  /* Read the message from memory */

  line = strsep (&in, "\n");
  while (line)
  {

    /* Header processing */

    if (block_position == BP_HEADER)
    {

      /* If we see two boundaries converged on top of one another */

      if (_ds_match_boundary (boundaries, line))
      {

        /* Add the boundary as the terminating boundary */

        current_block->terminating_boundary = strdup (line + 2);
        current_block->original_encoding = current_block->encoding;

        _ds_decode_headers(current_block);
        current_block = _ds_create_message_part ();

        if (!current_block)
          goto MEMFAIL;

        if (nt_add (out->components, (void *) current_block) == NULL)
          goto MEMFAIL;

        block_position = BP_HEADER;
      }

      /* Concatenate multiline headers to the original header field data */

      else if (line[0] == 32 || line[0] == '\t')
      {
        if (current_heading)
        {
          char *eow, *ptr;

          ptr = realloc (current_heading->data,
                         strlen (current_heading->data) + strlen (line) + 2);
          if (ptr)
          {
            current_heading->data = ptr;
            strcat (current_heading->data, "\n");
            strcat (current_heading->data, line);
          } else {
            goto MEMFAIL;
          }

          /* Our concatenated data doesn't have any whitespace between lines */
          for(eow=line;eow[0] && isspace((int) eow[0]);eow++) { }

          ptr =
            realloc (current_heading->concatenated_data,
              strlen (current_heading->concatenated_data) + strlen (eow) + 1);
          if (ptr)
          {
            current_heading->concatenated_data = ptr;
            strcat (current_heading->concatenated_data, eow);
          } else {
            goto MEMFAIL;
          }

          if (current_heading->original_data) {
            ptr =
              realloc (current_heading->original_data,
                       strlen (current_heading->original_data) +
                               strlen (line) + 2);
            if (ptr) {
              current_heading->original_data = ptr;
              strcat (current_heading->original_data, "\n");
              strcat (current_heading->original_data, line);
            } else {
              goto MEMFAIL;
            }
          }

          _ds_analyze_header (current_block, current_heading, boundaries);
        }
      }

      /* New header field when LF or CRLF is not found */

      else if (line[0] != 0  && line[0] != 13)
      {
        ds_header_t header = _ds_create_header_field (line);

        if (header != NULL)
        {
          _ds_analyze_header (current_block, header, boundaries);
          current_heading = header;
          nt_add (current_block->headers, header);
        }


      /* line[0] == 0 or line[0] == 13; LF or CRLF, switch to body */

      } else {
        block_position = BP_BODY;
      }
    }

    /* Body processing */

    else if (block_position == BP_BODY)
    {
      /* Look for a boundary in the header of a part */

      if (!strncasecmp (line, "Content-Type", 12)
            || ((line[0] == 32 || line[0] == 9) && in_content))
      {
        char boundary[128];
        in_content = 1;
        if (!_ds_extract_boundary(boundary, sizeof(boundary), line)) {
          if (!_ds_match_boundary (boundaries, boundary)) {
            _ds_push_boundary (boundaries, boundary);
            free(current_block->boundary);
            current_block->boundary = strdup (boundary);
          }
        } else {
          _ds_push_boundary (boundaries, "");
        }
      } else {
        in_content = 0;
      }

      /* Multipart boundary was reached; move onto next block */

      if (_ds_match_boundary (boundaries, line))
      {

        /* Add the boundary as the terminating boundary */

        current_block->terminating_boundary = strdup (line + 2);
        current_block->original_encoding = current_block->encoding;

        _ds_decode_headers(current_block);
        current_block = _ds_create_message_part ();

        if (!current_block)
          goto MEMFAIL;

        if (nt_add (out->components, (void *) current_block) == NULL)
          goto MEMFAIL;

        block_position = BP_HEADER;
      }

      /* Plain old message (or part) body */

      else {
        buffer_cat (current_block->body, line);

        /* Don't add extra \n at the end of message's body */

        if (in != NULL)
          buffer_cat (current_block->body, "\n");
      }
    }

    line = strsep (&in, "\n");
  } /* while (line) */

  _ds_decode_headers(current_block);

  free (m_in);
  nt_destroy (boundaries);
  return out;

MEMFAIL:
  if (m_in) free(m_in);
  if (boundaries) nt_destroy (boundaries);
  if (out) _ds_destroy_message(out);
  LOG (LOG_CRIT, ERR_MEM_ALLOC);
  return NULL;
}

/*
 * _ds_create_message_part
 *
 * DESCRIPTION
 *   create and initialize a new message block component
 *
 * RETURN VALUES
 *   pointer to an allocated message block (ds_message_part_t), NULL on failure
 *
 */

ds_message_part_t
_ds_create_message_part (void)
{
  ds_message_part_t block =
    (ds_message_part_t) calloc (1, sizeof (struct _ds_message_part));

  if (!block)
    goto MEMFAIL;

  block->headers = nt_create (NT_PTR);
  if (!block->headers)
    goto MEMFAIL;

  block->body = buffer_create (NULL);
  if (!block->body)
    goto MEMFAIL;

  block->encoding   = EN_UNKNOWN;
  block->media_type = MT_TEXT;
  block->media_subtype     = MST_PLAIN;
  block->original_encoding = EN_UNKNOWN;
  block->content_disposition = PCD_UNKNOWN;

  /* Not really necessary, but.. */

  block->boundary = NULL;
  block->terminating_boundary = NULL;
  block->original_signed_body = NULL;


  return block;

MEMFAIL:
  if (block) {
    buffer_destroy(block->body);
    nt_destroy(block->headers);
    free(block);
  }
  LOG (LOG_CRIT, ERR_MEM_ALLOC);
  return NULL;
}

/*
 * _ds_create_header_field(const char *heading)
 *
 * DESCRIPTION
 *   create and initialize a new header structure
 *
 * INPUT ARGUMENTS
 *      heading    plain text heading (e.g. "To: Mom")
 *
 * RETURN VALUES
 *   pointer to an allocated header structure (ds_header_t), NULL on failure
 */

ds_header_t
_ds_create_header_field (const char *heading)
{
  char *in = strdup(heading);
  char *ptr, *m = in, *data;
  ds_header_t header =
    (ds_header_t) calloc (1, sizeof (struct _ds_header_field));

  if (!header || !in)
    goto MEMFAIL;

  ptr = strsep (&in, ":");
  if (ptr) {
    header->heading = strdup (ptr);
    if (!header->heading)
      goto MEMFAIL;
    else
    {
      if (!in)
      {
        LOGDEBUG("%s:%u: unexpected data: header string '%s' doesn't "
                 "contains `:' character", __FILE__, __LINE__, header->heading);

        /* Use empty string as data as fallback for comtinue processing. */

        in = "";
      }
      else
      {
        /* Skip white space */
        while (*in == 32 || *in == 9)
          ++in;
      }

      data = strdup (in);
      if (!data)
        goto MEMFAIL;

      header->data = data;
      header->concatenated_data = strdup(data);
    }
  }

  free (m);
  return header;

MEMFAIL:
  free(header);
  free(m);
  LOG (LOG_CRIT, ERR_MEM_ALLOC);
  return NULL;
}

/*
 * _ds_decode_headers (ds_message_part_t block)
 *
 * DESCRIPTION
 *   decodes in-line encoded headers
 *
 * RETURN VALUES
 *   returns 0 on success
 */

int
_ds_decode_headers (ds_message_part_t block) {
#ifdef VERBOSE
  LOGDEBUG("decoding headers in message block");
#endif
  char *ptr, *dptr, *rest, *enc;
  ds_header_t header;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  long decoded_len;

  node_nt = c_nt_first(block->headers, &c_nt);
  while(node_nt != NULL) {
    long enc_offset;
    header = (ds_header_t) node_nt->ptr;

    for(enc_offset = 0; header->concatenated_data[enc_offset]; enc_offset++)
    {
      enc = header->concatenated_data + enc_offset;

      if (!strncmp(enc, "=?", 2)) {
        int was_null = 0;
        char *ptrptr, *decoded = NULL;
        long offset = (long) enc - (long) header->concatenated_data;

        if (header->original_data == NULL) {
          header->original_data = strdup(header->data);
          was_null = 1;
        }

        strtok_r (enc, "?", &ptrptr);
        strtok_r (NULL, "?", &ptrptr);
        ptr = strtok_r (NULL, "?", &ptrptr);
        dptr = strtok_r (NULL, "?", &ptrptr);
        if (!dptr) {
          if (was_null && header->original_data != NULL)
            free(header->original_data);
          if (was_null)
            header->original_data = NULL;
          continue;
        }

        rest = dptr + strlen (dptr);
        if (rest[0]!=0) {
          rest++;
          if (rest[0]!=0) rest++;
        }

        if (ptr != NULL && (ptr[0] == 'b' || ptr[0] == 'B'))
          decoded = _ds_decode_base64 (dptr);
        else if (ptr != NULL && (ptr[0] == 'q' || ptr[0] == 'Q'))
          decoded = _ds_decode_quoted (dptr);

        decoded_len = 0;

        /* Append the rest of the message */

        if (decoded)
        {
          char *new_alloc;

          decoded_len = strlen(decoded);
          new_alloc = calloc (1, offset + decoded_len + strlen (rest) + 2);
          if (new_alloc == NULL) {
            LOG (LOG_CRIT, ERR_MEM_ALLOC);
          }
          else
          {
            if (offset)
              strncpy(new_alloc, header->concatenated_data, offset);

            strcat(new_alloc, decoded);
            strcat(new_alloc, rest);
            free(decoded);
            decoded = new_alloc;
          }
        }

        if (decoded) {
          enc_offset += (decoded_len-1);
          free(header->concatenated_data);
          header->concatenated_data = decoded;
        }
        else if (was_null && header->original_data) {
          free(header->original_data);
          header->original_data = NULL;
        }
        else if (was_null) {
          header->original_data = NULL;
        }
      }
    }

    if (header->original_data != NULL) {
      free(header->data);
      header->data = strdup(header->concatenated_data);
    }

    node_nt = c_nt_next(block->headers, &c_nt);
  }

  return 0;
}

/*
 *  _ds_analyze_header (ds_message_part_t block, ds_header_t header,
 *                      struct nt *boundaries)
 *
 * DESCRIPTION
 *   analyzes the header passed in and performs various operations including:
 *     - setting media type and subtype
 *     - setting transfer encoding
 *     - adding newly discovered boundaries
 *
 *   based on the heading specified. essentially all headers should be
 *   analyzed for future expansion
 *
 * INPUT ARGUMENTS
 *      block		the message block to which the header belongs
 *      header		the header to analyze
 *      boundaries	a list of known boundaries found within the block
 */

void
_ds_analyze_header (
  ds_message_part_t block,
  ds_header_t header,
  struct nt *boundaries)
{
  if (!header || !block || !header->data)
    return;

  /* Content-Type header */

  if (!strcasecmp (header->heading, "Content-Type"))
  {
    int len = strlen(header->data);
    if (!strncasecmp (header->data, "text", 4)) {
      block->media_type = MT_TEXT;
      if (len >= 5 && !strncasecmp (header->data + 5, "plain", 5))
        block->media_subtype = MST_PLAIN;
      else if (len >= 5 && !strncasecmp (header->data + 5, "html", 4))
        block->media_subtype = MST_HTML;
      else
        block->media_subtype = MST_OTHER;
    }

    else if (!strncasecmp (header->data, "application", 11))
    {
      block->media_type = MT_APPLICATION;
      if (len >= 12 && !strncasecmp (header->data + 12, "dspam-signature", 15))
        block->media_subtype = MST_DSPAM_SIGNATURE;
      else
        block->media_subtype = MST_OTHER;
    }

    else if (!strncasecmp (header->data, "message", 7))
    {
      block->media_type = MT_MESSAGE;
      if (len >= 8 && !strncasecmp (header->data + 8, "rfc822", 6))
        block->media_subtype = MST_RFC822;
      else if (len >= 8 && !strncasecmp (header->data + 8, "inoculation", 11))
        block->media_subtype = MST_INOCULATION;
      else
        block->media_subtype = MST_OTHER;
    }

    else if (!strncasecmp (header->data, "multipart", 9))
    {
      char boundary[128];

      block->media_type = MT_MULTIPART;
      if (len >= 10 && !strncasecmp (header->data + 10, "mixed", 5))
        block->media_subtype = MST_MIXED;
      else if (len >= 10 && !strncasecmp (header->data + 10, "alternative", 11))
        block->media_subtype = MST_ALTERNATIVE;
      else if (len >= 10 && !strncasecmp (header->data + 10, "signed", 6))
        block->media_subtype = MST_SIGNED;
      else if (len >= 10 && !strncasecmp (header->data + 10, "encrypted", 9))
        block->media_subtype = MST_ENCRYPTED;
      else
        block->media_subtype = MST_OTHER;

      if (!_ds_extract_boundary(boundary, sizeof(boundary), header->data)) {
        if (!_ds_match_boundary (boundaries, boundary)) {
          _ds_push_boundary (boundaries, boundary);
          free(block->boundary);
          block->boundary = strdup (boundary);
        }
      } else {
        _ds_push_boundary (boundaries, "");
      }
    }
    else {
      block->media_type = MT_OTHER;
      block->media_subtype = MST_OTHER;
    }

  }

  /* Content-Transfer-Encoding */

  else if (!strcasecmp (header->heading, "Content-Transfer-Encoding"))
  {
    if (!strncasecmp (header->data, "7bit", 4))
      block->encoding = EN_7BIT;
    else if (!strncasecmp (header->data, "8bit", 4))
      block->encoding = EN_8BIT;
    else if (!strncasecmp (header->data, "quoted-printable", 16))
      block->encoding = EN_QUOTED_PRINTABLE;
    else if (!strncasecmp (header->data, "base64", 6))
      block->encoding = EN_BASE64;
    else if (!strncasecmp (header->data, "binary", 6))
      block->encoding = EN_BINARY;
    else
      block->encoding = EN_OTHER;
  }

  if (!strcasecmp (header->heading, "Content-Disposition"))
  {
    if (!strncasecmp (header->data, "inline", 6))
      block->content_disposition = PCD_INLINE;
    else if (!strncasecmp (header->data, "attachment", 10))
      block->content_disposition = PCD_ATTACHMENT;
    else
      block->content_disposition = PCD_OTHER;
  }

  return;
}

/*
 * _ds_destroy_message (ds_message_t message)
 *
 * DESCRIPTION
 *   destroys a message structure (ds_message_t)
 *
 * INPUT ARGUMENTS
 *      message    the message structure to be destroyed
 */

void
_ds_destroy_message (ds_message_t message)
{
  struct nt_node *node_nt;
  struct nt_c c;

  if (message == NULL)
    return;

  if (message->components) {
    node_nt = c_nt_first (message->components, &c);
    while (node_nt != NULL)
    {
      ds_message_part_t block = (ds_message_part_t) node_nt->ptr;
      _ds_destroy_block(block);
      node_nt = c_nt_next (message->components, &c);
    }
    nt_destroy (message->components);
  }
  free (message);
  return;
}

/*
 * _ds_destroy_headers (ds_message_part_t block)
 *
 * DESCRIPTION
 *   destroys a message block's header pairs
 *   does not free the structures themselves; these are freed at nt_destroy
 *
 * INPUT ARGUMENTS
 *      block    the message block containing the headers to destsroy
 */

void
_ds_destroy_headers (ds_message_part_t block)
{
  struct nt_node *node_nt;
  struct nt_c c;

  if (!block || !block->headers)
    return;

  node_nt = c_nt_first (block->headers, &c);
  while (node_nt != NULL)
  {
    ds_header_t field = (ds_header_t) node_nt->ptr;

    if (field)
    {
      free (field->original_data);
      free (field->heading);
      free (field->concatenated_data);
      free (field->data);
    }
    node_nt = c_nt_next (block->headers, &c);
  }

  return;
}

/*
 * _ds_destroy_block (ds_message_part_t block)
 *
 * DESCRIPTION
 *   destroys a message block
 *
 * INPUT ARGUMENTS
 *   block   the message block to destroy
 */

void
_ds_destroy_block (ds_message_part_t block)
{
  if (!block)
    return;

  if (block->headers)
  {
    _ds_destroy_headers (block);
    nt_destroy (block->headers);
  }
  buffer_destroy (block->body);
  buffer_destroy (block->original_signed_body);
  free (block->boundary);
  free (block->terminating_boundary);
//  free (block);
  return;
}

/*
 * _ds_decode_block (ds_message_part_t block)
 *
 * DESCRIPTION
 *   decodes a message block
 *
 * INPUT ARGUMENTS
 *   block   the message block to decode
 *
 * RETURN VALUES
 *   a pointer to the allocated character array containing the decoded message
 *   NULL on failure
 */

char *
_ds_decode_block (ds_message_part_t block)
{
  if (block->encoding == EN_BASE64)
    return _ds_decode_base64 (block->body->data);
  else if (block->encoding == EN_QUOTED_PRINTABLE)
    return _ds_decode_quoted (block->body->data);

  LOG (LOG_WARNING, "decoding of block encoding type %d not supported",
       block->encoding);
  return NULL;
}

/*
 * _ds_decode_{base64,quoted,hex8bit}
 *
 * DESCRIPTION
 *   supporting block decoder functions
 *   these function call (or perform) specific decoding functions
 *
 * INPUT ARGUMENTS
 *   body	encoded message body
 *
 * RETURN VALUES
 *   a pointer to the allocated character array containing the decoded body
 */

char *
_ds_decode_base64 (const char *body)
{
  if (body == NULL)
    return NULL;

  return base64decode (body);
}

char *
_ds_decode_quoted (const char *body)
{
#ifdef VERBOSE
  LOGDEBUG("decoding Quoted Printable encoded buffer");
#endif
  if (!body)
    return NULL;

  char *n, *out;
  const char *end, *p;

  n = out = malloc(strlen(body)+1);
  end = body + strlen(body);

  if (out == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  for (p = body; p < end; p++, n++) {
    if (*p == '=') {
      if (p[1] == '\r' && p[2] == '\n') {
        n -= 1;
        p += 2;
      } else if (p[1] == '\n') {
        n -= 1;
        p += 1;
      } else if (p[1] && p[2] && isxdigit((unsigned char) p[1]) && isxdigit((unsigned char) p[2])) {
        *n = ((_ds_hex2dec((unsigned char) p[1])) << 4) | (_ds_hex2dec((unsigned char) p[2]));
        p += 2;
      } else
        *n = *p;
    } else
      *n = *p;
  }

  *n = '\0';
  return (char *)out;
}

char *
_ds_decode_hex8bit (const char *body)
{
#ifdef VERBOSE
  LOGDEBUG("decoding hexadecimal 8-bit encodings in message block");
#endif
  if (!body)
    return NULL;

  char *n, *out;
  const char *end, *p;

  n = out = malloc(strlen(body)+1);
  end = body + strlen(body);

  if (out == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  for (p = body; p < end; p++, n++) {
    if (*p == '%')
      if (p[1] && p[2] && isxdigit((unsigned char) p[1]) && isxdigit((unsigned char) p[2])) {
        *n = ((_ds_hex2dec((unsigned char) p[1])) << 4) | (_ds_hex2dec((unsigned char) p[2]));
        p += 2;
      } else
        *n = *p;
    else
      *n = *p;
  }

  *n = '\0';
  return (char *)out;
}

/*
 * _ds_encode_block (ds_message_part_t block, int encoding)
 *
 * DESCRIPTION
 *   encodes a message block using the encoding specified and replaces the
 *   block's message body with the encoded data
 *
 * INPUT ARGUMENTS
 *      block       the message block to encode
 *      encoding    encoding to use (EN_)
 *
 * RETURN VALUES
 *    returns 0 on success
 */

int
_ds_encode_block (ds_message_part_t block, int encoding)
{
  /* we can't encode a block with the same encoding */

  if (block->encoding == encoding)
    return EINVAL;

  /* we can't encode a block that's already encoded */

  if (block->encoding == EN_BASE64 || block->encoding == EN_QUOTED_PRINTABLE)
    return EFAILURE;

  if (encoding == EN_BASE64) {
    char *encoded = _ds_encode_base64 (block->body->data);
    buffer_destroy (block->body);
    block->body = buffer_create (encoded);
    free (encoded);
    block->encoding = EN_BASE64;
  }
  else if (encoding == EN_QUOTED_PRINTABLE) {

    /* TODO */

    return 0;
  }

  LOGDEBUG("unsupported encoding: %d", encoding);
  return 0;
}

/*
 * _ds_encode_{base64,quoted}
 *
 * DESCRIPTION
 *   supporting block encoder functions
 *   these function call (or perform) specific encoding functions
 *
 * INPUT ARGUMENTS
 *   body        decoded message body
 *
 * RETURN VALUES
 *   a pointer to the allocated character array containing the encoded body
 */

char *
_ds_encode_base64 (const char *body)
{
  return base64encode (body);
}

/*
 * _ds_assemble_message (ds_message_t message)
 *
 * DESCRIPTION
 *   assembles a message structure into a flat text message
 *
 * INPUT ARGUMENTS
 *      message    the message structure (ds_message_t) to assemble
 *
 * RETURN VALUES
 *   a pointer to the allocated character array containing the text message
 */

char *
_ds_assemble_message (ds_message_t message, const char *newline)
{
  buffer *out = buffer_create (NULL);
  struct nt_node *node_nt, *node_header;
  struct nt_c c_nt, c_nt2;
  char *heading;
  char *copyback;
#ifdef VERBOSE
  int i = 0;
#endif

  if (!out) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  node_nt = c_nt_first (message->components, &c_nt);
  while (node_nt != NULL && node_nt->ptr != NULL)
  {
    ds_message_part_t block =
      (ds_message_part_t) node_nt->ptr;
#ifdef VERBOSE
    LOGDEBUG ("assembling component %d", i);
#endif

    /* Assemble headers */

    if (block->headers != NULL && block->headers->items > 0)
    {
      node_header = c_nt_first (block->headers, &c_nt2);
      while (node_header != NULL)
      {
        char *data;
        ds_header_t current_header =
          (ds_header_t) node_header->ptr;

        data = (current_header->original_data == NULL) ? current_header->data :
               current_header->original_data;

        heading = malloc(
            ((current_header->heading) ? strlen(current_header->heading) : 0)
          + ((data) ? strlen(data) : 0)
          + 3 + strlen(newline));

        if (current_header->heading != NULL &&
            (!strncmp (current_header->heading, "From ", 5) ||
             !strncmp (current_header->heading, "--", 2)))
          sprintf (heading, "%s:%s%s",
            (current_header->heading) ? current_header->heading : "",
            (data) ? data : "", newline);
        else
          sprintf (heading, "%s: %s%s",
            (current_header->heading) ? current_header->heading : "",
            (data) ? data : "", newline);

        buffer_cat (out, heading);
        free(heading);
        node_header = c_nt_next (block->headers, &c_nt2);
      }
    }

    buffer_cat (out, newline);

    /* Assemble bodies */

    if (block->original_signed_body != NULL && message->protect)
      buffer_cat (out, block->original_signed_body->data);
    else
      buffer_cat (out, block->body->data);

    if (block->terminating_boundary != NULL)
    {
      buffer_cat (out, "--");
      buffer_cat (out, block->terminating_boundary);
    }

    node_nt = c_nt_next (message->components, &c_nt);
#ifdef VERBOSE
    i++;
#endif

    if (node_nt != NULL && node_nt->ptr != NULL)
      buffer_cat (out, newline);
  }

  copyback = out->data;
  out->data = NULL;
  buffer_destroy (out);
  return copyback;
}

/*
 * _ds_{push,pop,match,extract}_boundary
 *
 * DESCRIPTION
 *   these functions maintain and service a boundary "stack" on the message
 */

int
_ds_push_boundary (struct nt *stack, const char *boundary)
{
  char *y;

  if (boundary == NULL || boundary[0] == 0)
    return EINVAL;

  y = malloc (strlen (boundary) + 3);
  if (y == NULL)
    return EUNKNOWN;

  sprintf (y, "--%s", boundary);
  nt_add (stack, (char *) y);
  free(y);

  return 0;
}

char *
_ds_pop_boundary (struct nt *stack)
{
  struct nt_node *node, *last_node = NULL, *parent_node = NULL;
  struct nt_c c;
  char *boundary = NULL;

  node = c_nt_first (stack, &c);
  while (node != NULL)
  {
    parent_node = last_node;
    last_node = node;
    node = c_nt_next (stack, &c);
  }
  if (parent_node != NULL)
    parent_node->next = NULL;
  else
    stack->first = NULL;

  if (last_node == NULL)
    return NULL;

  boundary = strdup (last_node->ptr);

  free (last_node->ptr);
  free (last_node);

  return boundary;
}

int
_ds_match_boundary (struct nt *stack, const char *buff)
{
  struct nt_node *node;
  struct nt_c c;

  node = c_nt_first (stack, &c);
  while (node != NULL)
  {
    if (!strncmp (buff, node->ptr, strlen (node->ptr)))
    {
      return 1;
    }
    node = c_nt_next (stack, &c);
  }
  return 0;
}

int
_ds_extract_boundary (char *buf, size_t size, char *mem)
{
  char *data, *ptr, *ptrptr;

  if (mem == NULL)
    return EINVAL;

  data = strdup(mem);
  if (data == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  for(ptr=data;ptr<(data+strlen(data));ptr++) {
    if (!strncasecmp(ptr, "boundary", 8)) {
      ptr = strchr(ptr, '=');
      if (ptr == NULL) {
        free(data);
        return EFAILURE;
      }
      ptr++;
      while(isspace((int) ptr[0]))
        ptr++;
      if (ptr[0] == '"')
        ptr++;
      strtok_r(ptr, " \";\n\t", &ptrptr);
      strlcpy(buf, ptr, size);
      free(data);
      return 0;
    }
  }

  free(data);
  return EFAILURE;
}

/*
 * _ds_find_header (ds_message_t message, consr char *heading) {
 *
 * DESCRIPTION
 *   finds a header and returns its value
 *
 * INPUT ARGUMENTS
 *   message     the message structure to search
 *   heading	the heading to search for
 *   flags	optional search flags
 *
 * RETURN VALUES
 *   a pointer to the header structure's value
 *
 */

char *
_ds_find_header (ds_message_t message, const char *heading) {
  ds_message_part_t block;
  ds_header_t head;
  struct nt_node *node_nt;

  if (message->components->first) {
    if ((block = message->components->first->ptr)==NULL)
      return NULL;
    if (block->headers == NULL)
      return NULL;
  } else {
    return NULL;
  }

  node_nt = block->headers->first;
  while(node_nt != NULL) {
    head = (ds_header_t) node_nt->ptr;
    if (head && !strcasecmp(head->heading, heading)) {
      return head->data;
    }
    node_nt = node_nt->next;
  }

  return NULL;
}

int _ds_hex2dec(unsigned char hex) {
  switch (hex) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return -1;
  }
}

/*
 * _ds_strip_html(const char *html)
 *
 * DESCRIPTION
 *    strip html tags from the supplied message
 *
 * INPUT ARGUMENTS
 *     html encoded message body
 *
 * RETURN VALUES
 *   a pointer to the allocated character array containing the
 *   stripped message
 *
 */

char *
_ds_strip_html (const char *html)
{
#ifdef VERBOSE
  LOGDEBUG("stripping HTML tags from message block");
#endif
  size_t j = 0, k = 0, i = 0;
  int visible = 1;
  int closing_td_tag = 0;
  char *html2;
  const char *cdata_close_tag = NULL;

  if(!html)
    return NULL;

  static struct {
    unsigned int id;
    char *entity;
  }
  charset[] = {
    {   32, "&nbsp;"    }, {  34, "&quot;"    }, {  34, "&quot;"    }, {  38, "&amp;"     },
    {   38, "&amp;"     }, {  39, "&apos;"    }, {  60, "&lt;"      }, {  60, "&lt;"      },
    {   62, "&gt;"      }, {  62, "&gt;"      }, { 160, "&nbsp;"    }, { 161, "&iexcl;"   },
    {  162, "&cent;"    }, { 163, "&pound;"   }, { 164, "&curren;"  }, { 165, "&yen;"     },
    {  166, "&brvbar;"  }, { 167, "&sect;"    }, { 168, "&uml;"     }, { 169, "&copy;"    },
    {  170, "&ordf;"    }, { 171, "&laquo;"   }, { 172, "&not;"     }, { 173, "&shy;"     },
    {  174, "&reg;"     }, { 175, "&macr;"    }, { 176, "&deg;"     }, { 177, "&plusmn;"  },
    {  178, "&sup2;"    }, { 179, "&sup3;"    }, { 180, "&acute;"   }, { 181, "&micro;"   },
    {  182, "&para;"    }, { 183, "&middot;"  }, { 184, "&cedil;"   }, { 185, "&sup1;"    },
    {  186, "&ordm;"    }, { 187, "&raquo;"   }, { 188, "&frac14;"  }, { 189, "&frac12;"  },
    {  190, "&frac34;"  }, { 191, "&iquest;"  }, { 192, "&Agrave;"  }, { 193, "&Aacute;"  },
    {  194, "&Acirc;"   }, { 195, "&Atilde;"  }, { 196, "&Auml;"    }, { 197, "&Aring;"   },
    {  198, "&AElig;"   }, { 199, "&Ccedil;"  }, { 200, "&Egrave;"  }, { 201, "&Eacute;"  },
    {  202, "&Ecirc;"   }, { 203, "&Euml;"    }, { 204, "&Igrave;"  }, { 205, "&Iacute;"  },
    {  206, "&Icirc;"   }, { 207, "&Iuml;"    }, { 208, "&ETH;"     }, { 209, "&Ntilde;"  },
    {  210, "&Ograve;"  }, { 211, "&Oacute;"  }, { 212, "&Ocirc;"   }, { 213, "&Otilde;"  },
    {  214, "&Ouml;"    }, { 215, "&times;"   }, { 216, "&Oslash;"  }, { 217, "&Ugrave;"  },
    {  218, "&Uacute;"  }, { 219, "&Ucirc;"   }, { 220, "&Uuml;"    }, { 221, "&Yacute;"  },
    {  222, "&THORN;"   }, { 223, "&szlig;"   }, { 224, "&agrave;"  }, { 225, "&aacute;"  },
    {  226, "&acirc;"   }, { 227, "&atilde;"  }, { 228, "&auml;"    }, { 229, "&aring;"   },
    {  230, "&aelig;"   }, { 231, "&ccedil;"  }, { 232, "&egrave;"  }, { 233, "&eacute;"  },
    {  234, "&ecirc;"   }, { 235, "&euml;"    }, { 236, "&igrave;"  }, { 237, "&iacute;"  },
    {  238, "&icirc;"   }, { 239, "&iuml;"    }, { 240, "&eth;"     }, { 241, "&ntilde;"  },
    {  242, "&ograve;"  }, { 243, "&oacute;"  }, { 244, "&ocirc;"   }, { 245, "&otilde;"  },
    {  246, "&ouml;"    }, { 247, "&divide;"  }, { 248, "&oslash;"  }, { 249, "&ugrave;"  },
    {  250, "&uacute;"  }, { 251, "&ucirc;"   }, { 252, "&uuml;"    }, { 253, "&yacute;"  },
    {  254, "&thorn;"   }, { 255, "&yuml;"    }, { 338, "&OElig;"   }, { 339, "&oelig;"   },
    {  352, "&Scaron;"  }, { 353, "&scaron;"  }, { 376, "&Yuml;"    }, { 402, "&fnof;"    },
    {  710, "&circ;"    }, { 732, "&tilde;"   }, { 913, "&Alpha;"   }, { 914, "&Beta;"    },
    {  915, "&Gamma;"   }, { 916, "&Delta;"   }, { 917, "&Epsilon;" }, { 918, "&Zeta;"    },
    {  919, "&Eta;"     }, { 920, "&Theta;"   }, { 921, "&Iota;"    }, { 922, "&Kappa;"   },
    {  923, "&Lambda;"  }, { 924, "&Mu;"      }, { 925, "&Nu;"      }, { 926, "&Xi;"      },
    {  927, "&Omicron;" }, { 928, "&Pi;"      }, { 929, "&Rho;"     }, { 931, "&Sigma;"   },
    {  932, "&Tau;"     }, { 933, "&Upsilon;" }, { 934, "&Phi;"     }, { 935, "&Chi;"     },
    {  936, "&Psi;"     }, { 937, "&Omega;"   }, { 945, "&alpha;"   }, { 946, "&beta;"    },
    {  947, "&gamma;"   }, { 948, "&delta;"   }, { 949, "&epsilon;" }, { 950, "&zeta;"    },
    {  951, "&eta;"     }, { 952, "&theta;"   }, { 953, "&iota;"    }, { 954, "&kappa;"   },
    {  955, "&lambda;"  }, { 956, "&mu;"      }, { 957, "&nu;"      }, { 958, "&xi;"      },
    {  959, "&omicron;" }, { 960, "&pi;"      }, { 961, "&rho;"     }, { 962, "&sigmaf;"  },
    {  963, "&sigma;"   }, { 964, "&tau;"     }, { 965, "&upsilon;" }, { 966, "&phi;"     },
    {  967, "&chi;"     }, { 968, "&psi;"     }, { 969, "&omega;"   }, { 977, "&thetasym" },
    {  978, "&upsih;"   }, { 982, "&piv;"     }, {8194, "&ensp;"    }, {8195, "&emsp;"    },
    { 8201, "&thinsp;"  }, {8204, "&zwnj;"    }, {8205, "&zwj;"     }, {8206, "&lrm;"     },
    { 8207, "&rlm;"     }, {8211, "&ndash;"   }, {8212, "&mdash;"   }, {8216, "&lsquo;"   },
    { 8217, "&rsquo;"   }, {8218, "&sbquo;"   }, {8220, "&ldquo;"   }, {8221, "&rdquo;"   },
    { 8222, "&bdquo;"   }, {8224, "&dagger;"  }, {8225, "&Dagger;"  }, {8226, "&bull;"    },
    { 8230, "&hellip;"  }, {8240, "&permil;"  }, {8242, "&prime;"   }, {8243, "&Prime;"   },
    { 8249, "&lsaquo;"  }, {8250, "&rsaquo;"  }, {8254, "&oline;"   }, {8260, "&frasl;"   },
    { 8364, "&euro;"    }, {8465, "&image;"   }, {8472, "&weierp;"  }, {8476, "&real;"    },
    { 8482, "&trade;"   }, {8501, "&alefsym;" }, {8592, "&larr;"    }, {8593, "&uarr;"    },
    { 8594, "&rarr;"    }, {8595, "&darr;"    }, {8596, "&harr;"    }, {8629, "&crarr;"   },
    { 8656, "&lArr;"    }, {8657, "&uArr;"    }, {8658, "&rArr;"    }, {8659, "&dArr;"    },
    { 8660, "&hArr;"    }, {8704, "&forall;"  }, {8706, "&part;"    }, {8707, "&exist;"   },
    { 8709, "&empty;"   }, {8711, "&nabla;"   }, {8712, "&isin;"    }, {8713, "&notin;"   },
    { 8715, "&ni;"      }, {8719, "&prod;"    }, {8721, "&sum;"     }, {8722, "&minus;"   },
    { 8727, "&lowast;"  }, {8730, "&radic;"   }, {8733, "&prop;"    }, {8734, "&infin;"   },
    { 8736, "&ang;"     }, {8743, "&and;"     }, {8744, "&or;"      }, {8745, "&cap;"     },
    { 8746, "&cup;"     }, {8747, "&int;"     }, {8756, "&there4;"  }, {8764, "&sim;"     },
    { 8773, "&cong;"    }, {8776, "&asymp;"   }, {8800, "&ne;"      }, {8801, "&equiv;"   },
    { 8804, "&le;"      }, {8805, "&ge;"      }, {8834, "&sub;"     }, {8835, "&sup;"     },
    { 8836, "&nsub;"    }, {8838, "&sube;"    }, {8839, "&supe;"    }, {8853, "&oplus;"   },
    { 8855, "&otimes;"  }, {8869, "&perp;"    }, {8901, "&sdot;"    }, {8968, "&lceil;"   },
    { 8969, "&rceil;"   }, {8970, "&lfloor;"  }, {8971, "&rfloor;"  }, {9001, "&lang;"    },
    { 9002, "&rang;"    }, {9674, "&loz;"     }, {9824, "&spades;"  }, {9827, "&clubs;"   },
    { 9829, "&hearts;"  }, {9830, "&diams;"   }
  };
  int num_chars = sizeof(charset) / sizeof(charset[0]);

  static struct {
    char *open_tag;
    char *uri_tag;
  }
  uritag[] = {
    {          "<a", "href"       }, {        "<img", "src"        }, {      "<input", "src"        },
    {     "<iframe", "src"        }, {      "<frame", "src"        }, {     "<script", "src"        },
    {       "<form", "action"     }, {      "<embed", "src"        }, {       "<area", "href"       },
    {       "<base", "href"       }, {       "<link", "href"       }, {     "<source", "src"        },
    {       "<body", "background" }, { "<blockquote", "cite"       }, {          "<q", "cite"       },
    {        "<ins", "cite"       }, {        "<del", "cite"       }
  };
  int num_uri = sizeof(uritag) / sizeof(uritag[0]);

  size_t len = strlen(html);
  html2 = malloc(len+1);

  if (html2 == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  for (i = 0; i < len; i++) {
    if (html[i] == '<') {
      if (cdata_close_tag) {
        if (strncasecmp(html + i, cdata_close_tag, strlen(cdata_close_tag)) == 0) {
          i += strlen(cdata_close_tag) - 1;
          cdata_close_tag = NULL;
        }
        continue;
      } else if (strncasecmp(html + i, "</td>", 5) == 0) {
        i += 4;
        closing_td_tag = 1;
        continue;
      } else if (strncasecmp(html + i, "<td", 3) == 0 && closing_td_tag) {
        if (j > 0 && !isspace(html2[j-1])) {
          html2[j++]=' ';
        }
        visible = 0;
      } else {
        closing_td_tag = 0;
        visible = 1;
      }
      k = i + 1;

      if ((k < len) && (!( (html[k] >= 65 && html[k] <= 90) ||
                           (html[k] >= 97 && html[k] <= 122) ||
                           (html[k] == 47) ||
                           (html[k] == 33) ))) {
        /* Not a HTML tag. HTML tags start with a letter, forwardslash or exclamation mark */
        visible = 1;
        html2[j++]=html[i];
        i = k;
        const char *w = &(html[k]);
        while (j < len && (size_t)(w - html) < len && *w != '<') {
          html2[j++]=*w;
          w++;
          i++;
        }
        continue;
      } else if (html[k]) {
        /* find the end of the tag */
        while (k < len && html[k] != '<' && html[k] != '>') {k++;}

        /* if we've got a tag with a uri, save the address to print later. */
        char *url_tag = " ";
        int tag_offset = 0, x = 0, y = 0;
        for (y = 0; y < num_uri; y++) {
          x = strlen(uritag[y].open_tag);
          if (strncasecmp(html+i,uritag[y].open_tag,x)==0 && (i+x < len && isspace(html[i+x]))) {
            url_tag = uritag[y].uri_tag;
            tag_offset = i + x + 1;
            break;
          }
        }
        /* tag with uri found */
        if (tag_offset > 0) {
          size_t url_start;         /* start of url tag inclusive [ */
          size_t url_tag_len = strlen(url_tag);
          char delim = ' ';
          /* find start of uri */
          for (url_start = tag_offset; url_start <= k; url_start++) {
            if (strncasecmp(html + url_start, url_tag, url_tag_len) == 0) {
              url_start += url_tag_len;
              while (html[url_start] && isspace(html[url_start])) {url_start++;}   /* remove spaces before = */
              if (html[url_start] == '=') {
                url_start++;
                while (html[url_start] && isspace(html[url_start])) {url_start++;} /* remove spaces after = */
                if (html[url_start] == '"') {
                  delim = '"';
                  url_start++;
                } else if (html[url_start] == '\'') {
                  delim = '\'';
                  url_start++;
                } else {
                  delim = '>';
                }
                break;
              } else {
                /* Start of uri tag found but no '=' after the tag.
                 * Skip the whole tag.
                 */
                break;
              }
            } else if ((url_start - tag_offset) >= 50) {
              /* The length of the html tag is over 50 characters long without
               * finding the start of the url/uri. Skip the whole tag.
               */
              break;
            }
          }
          /* find end of uri */
          if (delim != ' ') {
            if (url_start < len &&
                (strncasecmp(html + url_start, "http:", 5) == 0 ||
                 strncasecmp(html + url_start, "https:", 6) == 0 ||
                 strncasecmp(html + url_start, "ftp:", 4) == 0)) {
              html2[j++]=' ';
              const char *w = &(html[url_start]);
              /* html2 is a buffer of len + 1, where the +1 is for NULL
               * termination. This means we only want to loop to len
               * since we will replace html2[j] right after the loop.
               */
              while (j < len && (size_t)(w - html) < len && *w != delim) {
                html2[j++]=*w;
                w++;
              }
              html2[j++]=' ';
            }
          }
        } else if (strncasecmp(html + i, "<p>", 3) == 0
                || strncasecmp(html + i, "<p ", 3) == 0
                || strncasecmp(html + i, "<p\t", 3) == 0
                || strncasecmp(html + i, "<tr", 3) == 0
                || strncasecmp(html + i, "<option", 7) == 0
                || strncasecmp(html + i, "<br", 3) == 0
                || strncasecmp(html + i, "<li", 3) == 0
                || strncasecmp(html + i, "<div", 4) == 0
                || strncasecmp(html + i, "</select>", 9) == 0
                || strncasecmp(html + i, "</table>", 8) == 0) {
          if (j > 0 && html2[j-1] != '\n' && html2[j-1] != '\r') {
            html2[j++] = '\n';
          }
        } else if (strncasecmp(html + i, "<applet", 7) == 0) {
          cdata_close_tag = "</applet>";
        } else if (strncasecmp(html + i, "<embed", 6) == 0) {
          cdata_close_tag = "</embed>";
        } else if (strncasecmp(html + i, "<frameset", 9) == 0) {
          cdata_close_tag = "</frameset>";
        } else if (strncasecmp(html + i, "<frame", 6) == 0) {
          cdata_close_tag = "</frame>";
        } else if (strncasecmp(html + i, "<iframe", 7) == 0) {
          cdata_close_tag = "</iframe>";
        } else if (strncasecmp(html + i, "<noembed", 8) == 0) {
          cdata_close_tag = "</noembed>";
        } else if (strncasecmp(html + i, "<noscript", 9) == 0) {
          cdata_close_tag = "</noscript>";
        } else if (strncasecmp(html + i, "<object", 7) == 0) {
          cdata_close_tag = "</object>";
        } else if (strncasecmp(html + i, "<script", 7) == 0) {
          cdata_close_tag = "</script>";
        } else if (strncasecmp(html + i, "<style", 6) == 0) {
          cdata_close_tag = "</style>";
        }
        i = (html[k] == '<' || html[k] == '\0')? k - 1: k;
        continue;
      }
    } else if (cdata_close_tag) {
      continue;
    } else if (!isspace(html[i])) {
      visible = 1;
    }

    if (strncmp(html+i,"&#",2)==0) {
      int x = 0;
      const char *w = &(html[i+2]);
      while (*w == '0') {i++;w++;}
      char n[5];
      if (html[i+4] && html[i+4] == ';'
          && isdigit(html[i+2])
          && isdigit(html[i+3])) {
        n[0] = html[i+2];
        n[1] = html[i+3];
        n[2] = 0;
        x = atoi(n);
        if (x <= 255 && x >= 32)
          html2[j++] = x;
        i += 4;
      } else if (html[i+6]
                  && html[i+6] == ';'
                  && isdigit(html[i+2])
                  && isdigit(html[i+3])
                  && isdigit(html[i+4])
                  && isdigit(html[i+5])) {
        n[0] = html[i+2];
        n[1] = html[i+3];
        n[2] = html[i+4];
        n[3] = html[i+5];
        n[4] = 0;
        x = atoi(n);
        if (x <= 255 && x >= 32)
          html2[j++] = x;
        i += 6;
      } else {
        const char *w = &(html[i]);
        while (*w != ';' && *w != ' ' && *w != '\t' && *w != '\0') {i++;w++;}
      }
      visible = 0;
      continue;
    } else if (html[i] == '&') {
      int x = 0, y = 0;
      for (y = 0; y < num_chars; y++) {
        x = strlen(charset[y].entity);
        if (strncasecmp(html+i,charset[y].entity,x)==0) {
          if (charset[y].id <= 255)
            html2[j++] = charset[y].id;
          i += x-1;
          visible = 0;
          continue;
        }
      }
    }

    if (j < len && visible)
      html2[j++] = html[i];

    if (j >= len)
      i = j = len;
  }

  html2[j] = '\0';
  return (char *)html2;
}
