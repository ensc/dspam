/* $Id: decode.h,v 1.3 2004/12/14 00:25:45 jonz Exp $ */

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

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "nodetree.h"
#include "buffer.h"

#ifndef _DECODE_H
#define _DECODE_H

/*
  Message Actualization and Decoding

  The decode components of DSPAM are responsible for decoding and actualizing
  a message into its smaller components.  Each message part consists of
  a message block, headers array, and etecetera.

*/
  
/*
  _ds_header_field

  A single header from a message block

*/
struct _ds_header_field
{
  char *heading;
  char *data;
  char *original_data;
  char *concatenated_data;
};

/*
  _ds_message_block
 

  A single message block.  In a single-part message, there will be only one
  block.  In a multipart message, each part will be separated into a separte
  block.  The message block consists of:

   - A dynamic array of headers for the block
   - Body data (= NULL if there is no body)
   - Block encoding
   - Block media type information
   - Boundary and terminating boundary information
*/

struct _ds_message_block
{
  struct nt *	headers;
  buffer *	body;
  buffer *	original_signed_body;
  char *	boundary;
  char *	terminating_boundary;
  int		encoding;
  int 		original_encoding;
  int 		media_type;
  int 		media_subtype;
};

/*
  _ds_message

   The actual message structure.  Comprised of an array of message blocks.
   In a non-multipart email, there will only be one message block however in
   multipart emails, the first message block will represent the header 
   (with a NULL body_data), and each additional block within the email will
   be given its own message_block structure with its own headers, boundary,
   etc.

   Embedded multipart messages are not realized by the structure, but can
   be identified by examining the media type or headers.
*/

struct _ds_message
{
  struct nt *	components; 
};

/*
  Decoding and Actualization Functions

  _ds_actualize_message()
    The main actualization function; this is called with a plain text copy of
    the message to decode and actualize, and returns a _ds_message structure
    containing the actualized message.

  _ds_assemble_message()
    The opposite of _ds_actualize_message(), this function assembles a
    _ds_message structure back into a plain text message
    
*/

/* Adapter-dependent */
#ifndef NCORE
char *  _ds_decode_base64       (const char *body);
char *  _ds_decode_quoted       (const char *body);
#endif

/* Adapter-independent */

struct _ds_message *	_ds_actualize_message	(const char *message);
char *			_ds_assemble_message	(struct _ds_message *message);

/* Internal Decoding Functions */

struct _ds_message_block * _ds_create_message_block (void);
struct _ds_header_field *  _ds_create_header_field  (const char *heading);

void   _ds_analyze_header	(struct _ds_message_block *block,
				 struct _ds_header_field *header,
				 struct nt *boundaries);

int _ds_destroy_message	(struct _ds_message *m);
int _ds_destroy_headers	(struct _ds_message_block *block);
int _ds_destroy_block	(struct _ds_message_block *block);

char *	_ds_decode_block	(struct _ds_message_block *block);
int	_ds_encode_block	(struct _ds_message_block *block, int encoding);
char *	_ds_encode_base64	(const char *body);
char *	_ds_encode_quoted	(const char *body);
int     _ds_decode_headers      (struct _ds_message_block *block);

int	_ds_push_boundary	(struct nt *stack, const char *boundary);
int	_ds_match_boundary	(struct nt *stack, const char *buff);
int     _ds_extract_boundary    (char *buf, size_t size, char *data);
char *	_ds_pop_boundary	(struct nt *stack);

/* Encoding Values */

#define EN_7BIT			0x00
#define EN_8BIT 		0x01
#define EN_QUOTED_PRINTABLE	0x02
#define EN_BASE64		0x03
#define EN_BINARY		0x04
#define EN_UNKNOWN		0xFE
#define EN_OTHER		0xFF

/* Media Types which are relevant to DSPAM */

#define MT_TEXT			0x00
#define MT_MULTIPART		0x01
#define MT_MESSAGE		0x02
#define MT_APPLICATION		0x03
#define MT_UNKNOWN		0xFE
#define MT_OTHER		0xFF

/* Media Subtypes which are relevant to DSPAM */

#define MST_PLAIN		0x00
#define	MST_HTML		0x01
#define MST_MIXED		0x02
#define MST_ALTERNATIVE		0x03
#define MST_RFC822		0x04
#define MST_DSPAM_SIGNATURE	0x05
#define MST_SIGNED		0x06
#define MST_INOCULATION		0x07
#define MST_ENCRYPTED		0x08
#define MST_UNKNOWN		0xFE
#define MST_OTHER		0xFF

/* Block position; used when analyzing a message */

#define BP_HEADER		0x00
#define BP_BODY			0x01

#endif /* _DECODE_H */

