/* $Id: tokenizer.h,v 1.1 2005/09/10 18:27:47 jonz Exp $ */

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

#ifndef _TOKENIZER_H
#  define _TOKENIZER_H

#include "diction.h"
#include "nodetree.h"
#include "error.h"
#include "storage_driver.h"
#include "decode.h"

#define SBPH_SIZE       5

int _ds_tokenize (DSPAM_CTX * CTX, char *headers, char *body, ds_diction_t diction);
int _ds_tokenize_sbph (DSPAM_CTX * CTX, char *headers, char *body, ds_diction_t diction);
int _ds_tokenize_ngram (DSPAM_CTX * CTX, char *headers, char *body, ds_diction_t diction);

int _ds_process_header_token (DSPAM_CTX * CTX, char *joined_token,
    const char *previous_token, ds_diction_t diction, const char *heading);
int _ds_process_body_token (DSPAM_CTX * CTX, char *joined_token,
    const char *previous_token, ds_diction_t diction); 
int _ds_map_header_token (DSPAM_CTX * CTX, char *token, char **previous_tokens,
    ds_diction_t diction, const char *heading);
int _ds_map_body_token          (DSPAM_CTX * CTX, char *token,
    char **previous_tokens, ds_diction_t diction);
int  _ds_degenerate_message    (DSPAM_CTX *CTX, buffer *header, buffer *body);
int _ds_url_tokenize(ds_diction_t diction, char *body, const char *key);
void _ds_sbph_clear(char **previous_tokens);
char * _ds_truncate_token(const char *token);

#endif
