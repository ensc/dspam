/* $Id: tokenizer.h,v 1.10 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifndef _TOKENIZER_H
#  define _TOKENIZER_H

#include "diction.h"
#include "nodetree.h"
#include "error.h"
#include "storage_driver.h"
#include "decode.h"

#define SPARSE_WINDOW_SIZE       5

int _ds_tokenize(
  DSPAM_CTX * CTX,
   char *headers,
   char *body,
   ds_diction_t diction);

int _ds_tokenize_sparse(
  DSPAM_CTX * CTX, 
  char *headers, 
  char *body, 
  ds_diction_t diction);

int _ds_tokenize_ngram(
  DSPAM_CTX * CTX,
  char *headers,
  char *body,
  ds_diction_t diction);

/* _ds_process: ngram token generation routines */

int _ds_process_header_token(
  DSPAM_CTX * CTX,
  char *joined_token,
  const char *previous_token,
  ds_diction_t diction,
  const char *heading);

int _ds_process_body_token(
  DSPAM_CTX * CTX,
  char *joined_token,
  const char *previous_token,
  ds_diction_t diction); 

/* _ds_map: sparse token generation routines */

int _ds_map_header_token(
  DSPAM_CTX * CTX,
  char *token,
  char **previous_tokens,
  ds_diction_t diction,
  const char *heading,
  const char *bitpattern);

int _ds_map_body_token(
  DSPAM_CTX * CTX,
  char *token,
  char **previous_tokens,
  ds_diction_t diction,
  const char *bitpattern);

int _ds_degenerate_message(
  DSPAM_CTX *CTX,
  buffer *header,
  buffer *body);

int _ds_url_tokenize(
  ds_diction_t diction,
  char *body,
  const char *key);

void _ds_sparse_clear
  (char **previous_tokens);

char * _ds_truncate_token
  (const char *token);

char *_ds_generate_bitpattern
  (int breadth);

#endif
