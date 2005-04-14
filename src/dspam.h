/* $Id: dspam.h,v 1.24 2005/04/14 05:13:43 jonz Exp $ */

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

#include <sys/types.h>
#ifndef _WIN32
#include <pwd.h>
#endif
#include "libdspam.h"
#include "buffer.h"
#include "pref.h"
#include "read_config.h"
#include "daemon.h"

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "agent_shared.h"

#ifndef _DSPAM_H
#  define _DSPAM_H

/* Public agent functions */

double gettime(void);

int deliver_message	(AGENT_CTX *ATX, 
                         const char *message, 
                         const char *mailer_args,
                         const char *username, 
                         FILE *out,
                         int result);

agent_pref_t load_aggregated_prefs
			(AGENT_CTX *ATX, const char *username);

int process_message	(AGENT_CTX *ATX, buffer *message, const char *username);

int inoculate_user	(const char *username, struct _ds_spam_signature *SIG,
			 const char *message, AGENT_CTX *ATX);

int user_classify	(const char *username, struct _ds_spam_signature *SIG,
			 const char *message, AGENT_CTX *ATX);

int send_notice		(AGENT_CTX *ATX, const char *filename, const char *mailer_args,
			 const char *username);

int write_web_stats     (agent_pref_t PTX, const char *username, const char *group, 
                         struct _ds_spam_totals *totals);

int ensure_confident_result	(DSPAM_CTX *CTX, AGENT_CTX *ATX, int result);
DSPAM_CTX *ctx_init	(AGENT_CTX *ATX, const char *username);
int log_events		(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int retrain_message	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int tag_message		(struct _ds_message_block *block, agent_pref_t PTX);
int quarantine_message  (agent_pref_t PTX, const char *message, const char *username);
int process_users       (AGENT_CTX *ATX, buffer *message);
int find_signature	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int add_xdspam_headers	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int embed_signature	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int embed_signed	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int tracksource		(DSPAM_CTX *CTX);
int is_blacklisted	(const char *ip);
#ifdef DAEMON
int daemon_start	(AGENT_CTX *ATX);
#endif

buffer *read_stdin	(AGENT_CTX *ATX);

#ifdef NEURAL
int process_neural_decision(DSPAM_CTX *CTX, struct _ds_neural_decision *DEC);
#endif

#define DSM_DAEMON	0xFE

typedef struct agent_result {
  int exitcode;
  int classification;
} *agent_result_t; 

#define ERC_SUCCESS		0x00
#define ERC_PROCESS		-0x01
#define ERC_DELIVERY		-0x02

#endif /* _DSPAM_H */

