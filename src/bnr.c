/* $Id: bnr.c,v 1.4 2004/11/23 15:17:47 jonz Exp $ */

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
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "bnr.h"
#include "config.h"
#include "libdspam_objects.h"
#include "libdspam.h"
#include "nodetree.h"
#include "config.h"
#include "util.h"
#include "lht.h"
#include "tbt.h"
#include "error.h"

/*
   Bayesian Noise Reduction - Progressive Noise Logic
   http://www.nuclearelephant.com/projects/dspam/bnr.html 
*/


/*
  bnr_pattern_instantiate() - Instantiate BNR patterns found within the message
  
  bnr_pattern_instantiate() scans the order of tokens in the message, 
  calculates their probability, then creates a series of BNR patterns based
  on the bands each token falls into. 

  CTX (in)       The DSPAM Context
  pfreq (in/out) The long hash tree to write the patterns to
  order (in)     The ordered stream of tokens
*/
  
int bnr_pattern_instantiate(
  DSPAM_CTX *CTX, 
  struct lht *pfreq, 
  struct nt *order, 
  char type)
{
  float previous_bnr_probs[BNR_SIZE];
  struct lht_node *node_lht;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  unsigned long long crc;
  char bnr_token[64];
  int i;
 
  for(i=0;i<BNR_SIZE;i++) 
    previous_bnr_probs[i] = 0.00000;

  node_nt = c_nt_first(order, &c_nt);
  while(node_nt != NULL) {
    node_lht = (struct lht_node *) node_nt->ptr;

    _ds_calc_stat (CTX, node_lht->key, &node_lht->s, DTT_DEFAULT);

    previous_bnr_probs[BNR_SIZE-1] = _ds_round(node_lht->s.probability);
    sprintf(bnr_token, "bnr.%c", type);
    for(i=0;i<BNR_SIZE;i++) {
      char x[6];
      snprintf(x, 6, "%01.2f.", previous_bnr_probs[i]);
      strlcat(bnr_token, x, sizeof(bnr_token));
    }

    for(i=0;i<BNR_SIZE-1;i++) {
      previous_bnr_probs[i] = previous_bnr_probs[i+1];
    }

    crc = _ds_getcrc64 (bnr_token);
#ifdef VERBOSE
    LOGDEBUG ("BNR pattern instantiated: '%s'", bnr_token);
#endif
    lht_hit (pfreq, crc, bnr_token);
    node_nt = c_nt_next(order, &c_nt);
  }
  return 0;
}

/*
  bnr_filter_process() - Identify and "dub" pattern inconsistencies

  bnr_filter_process() analyzes tokens contained within interesting patterns 
  and dubs any tokens whose disposition is inconsistent with that of the 
  pattern (e.g. falls outside of the inclusionary radius of the pattern's 
  p-value). 

  CTX (in)     DSPAM Context
  BTX (in/out) BNR Context

*/

int bnr_filter_process(DSPAM_CTX *CTX, BNR_CTX *BTX) {
  struct lht_node * previous_bnr_tokens[BNR_SIZE];
  float previous_bnr_probs[BNR_SIZE];
  struct lht_node *node_lht;
  struct _ds_spam_stat s;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  unsigned long long crc;
  char bnr_token[64];
  int i;

  for(i=0;i<BNR_SIZE;i++) {
    previous_bnr_probs[i] = 0.00000;
    previous_bnr_tokens[i] = NULL;
  } 
  node_nt = c_nt_first(BTX->stream, &c_nt);
  while(node_nt != NULL) {
    node_lht = (struct lht_node *) node_nt->ptr;

    _ds_calc_stat (CTX, node_lht->key, &node_lht->s, DTT_DEFAULT);

    previous_bnr_probs[BNR_SIZE-1] = _ds_round(node_lht->s.probability);
    previous_bnr_tokens[BNR_SIZE-1] = node_lht;
    sprintf(bnr_token, "bnr.%c", BTX->type);
    for(i=0;i<BNR_SIZE;i++) {
      char x[6];
      snprintf(x, 6, "%01.2f.", previous_bnr_probs[i]);
      strlcat(bnr_token, x, sizeof(bnr_token));
    }

    for(i=0;i<BNR_SIZE-1;i++) {
      previous_bnr_probs[i] = previous_bnr_probs[i+1];
      previous_bnr_tokens[i] = previous_bnr_tokens[i+1];
    }

    crc = _ds_getcrc64 (bnr_token);

    /* Identify interesting patterns */
    if (!lht_getspamstat(BTX->patterns, crc, &s) && 
        fabs(0.5-s.probability) > EX_RADIUS) 
    {

#ifdef BNR_VERBOSE_DEBUG
      printf("SUSPECT PATTERN: %s (%1.2f)\n", bnr_token, s.probability);
#endif
      /* Eliminate inconsistent tokens */
      for(i=0;i<BNR_SIZE;i++) {
        if (previous_bnr_tokens[i]) {
          if (fabs(s.probability-previous_bnr_tokens[i]->s.probability)>IN_RADIUS) {
            BTX->total_eliminations++;
            previous_bnr_tokens[i]->frequency -= 2;
#ifdef BNR_VERBOSE_DEBUG
            printf("\tELIMINATING: %s (%1.2f) RADIS %1.2f\n", 
                   previous_bnr_tokens[i]->token_name,
                   previous_bnr_tokens[i]->s.probability, 
                   fabs(s.probability-previous_bnr_tokens[i]->s.probability));
#endif
          } else {
#ifdef BNR_VERBOSE_DEBUG
            printf("\tOK: %s (%1.2f) RADIS %1.2f\n",
                   previous_bnr_tokens[i]->token_name,
                   previous_bnr_tokens[i]->s.probability,
                   fabs(s.probability-previous_bnr_tokens[i]->s.probability));
#endif 
            BTX->total_clean++;
          }
        }
      }
    } 

    node_nt = c_nt_next(BTX->stream, &c_nt);
  }

  return 0;
}

