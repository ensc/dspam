/* $Id: bnr.c,v 1.1 2004/10/24 20:49:33 jonz Exp $ */

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

   DSPAM uses a slightly modified implementation of BNR, which also counts
   frequency within a message. When a token is found noisy, its frequency is
   reduced by 2. If the token appears >2 times in a message, it will remain
   valid unless it is found noisy N/2 times.
*/

#define DUB_MIN_THRESHHOLD      4
#define TH_LOW  	0.30
#define TH_HIGH 	0.90
#define TH_NL   	0.40
#define TH_NH   	0.60
#define MARK_LOW	0.25
#define MARK_HIGH	0.80
#define IS_NEUTRAL(A)	(A>=TH_NL && A<=TH_NH)

int bnr_filter_process(DSPAM_CTX *CTX, BNR_CTX *BTX) {
  struct lht_node *node_previous = NULL;
  struct lht_node *node_current;
  struct lht_node *node_next;
  struct nt_node *node_nt;
  struct nt_c c_nt;

  int dubbing = 0;			/* Current dub mode */
  int dub_length = 0;			/* Current dub length */
  struct nt_node *dub_start = NULL;	/* Start of current dub */
  struct nt_node *dub_end   = NULL;	/* End of current dub */
 
  BTX->total_eliminations = 0;
  BTX->total_clean = 0;

  /* Iterate through each token in order of appearance */

  node_nt = c_nt_first(BTX->stream, &c_nt);
  while(node_nt != NULL) {
    node_current = (struct lht_node *) node_nt->ptr;
    if (node_nt->next != NULL)
      node_next = (struct lht_node *) node_nt->next->ptr;
    else
      node_next = node_current;

    /* Calculate probability for current node, if necessary */
    if (node_previous == NULL)
      _ds_calc_stat (CTX, node_current->key, &node_current->s);

    /* Calculate probability for next node */
    _ds_calc_stat (CTX, node_next->key, &node_next->s);

    /* If dubbing is on, turn it off if we come to an end-of-noise marker */
    if (
         dubbing		&&			/* Dubbing on */
         (
            node_current->s.probability >= MARK_HIGH ||	/* Spammy token */ 

           (node_current->s.probability <= MARK_LOW &&	/* 2 consecutive     */ 
            node_next->s.probability    <= MARK_LOW)	/*   innocent tokens */
         )
       )
    {
      dubbing = 0;
      dub_end = node_nt;

      /* Undub if the dub length is too short */
      if (dub_length<DUB_MIN_THRESHHOLD && dub_length>1) {
        struct nt_node *undub_this;

        undub_this = dub_start->next;
        while(undub_this && undub_this != dub_end) {
          ((struct lht_node *) undub_this->ptr)->frequency++;
          undub_this = undub_this->next;
        }
      }
    }

    if (dubbing)
      dub_length++;

    /* Identify noise, turn on dubbing */
    if (
       dubbing || 					/* Dubbing is on OR */

       ( /* Not a header */
         (node_current->token_name &&
          node_current->token_name[0] &&
         !strchr(node_current->token_name, '*'))	&&

         /* Current token is not neutral */

         (node_current->s.probability <= TH_LOW   ||
          node_current->s.probability >= TH_HIGH)	&&

         /* Adjacent to a neutral token */

         (
            (node_previous &&
            IS_NEUTRAL(node_previous->s.probability)) ||
            (node_next &&
            IS_NEUTRAL(node_next->s.probability))
         ) 						//&&


         /* Not an end marker */
/*       ! (
           node_current->s.probability <= TH_LOW &&
             (node_previous->s.probability <= TH_LOW || 
              node_next->s.probability <= TH_LOW)
         )
*/
       )
     )
    {
      BTX->total_eliminations ++;
      node_current->frequency -= 2;

      /* Advance the dubbing start position to this mark, committing all
         previous dubs */
      if (node_current->s.probability <= TH_LOW) {
 
        if (dubbing)
        {
          dub_start = node_nt;
        }
        else {
          dub_length = 1;
          dub_start = node_nt;
          dubbing = 1;
        }
      }
    } else { 
      BTX->total_clean++;
    }

    node_previous = node_current;
    node_nt = c_nt_next(BTX->stream, &c_nt);
  }

  return 0;
}
