/* $Id: dspam.h,v 1.12 2004/12/03 01:30:32 jonz Exp $ */

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

int deliver_message	(const char *message, const char *mailer_args,
			 const char *username, FILE *out);

int process_message	(AGENT_CTX *ATX, AGENT_PREF PTX,
			 buffer *message, const char *username);

int inoculate_user	(const char *username, struct _ds_spam_signature *SIG,
			 const char *message, AGENT_CTX *ATX);

int user_classify	(const char *username, struct _ds_spam_signature *SIG,
			 const char *message, AGENT_CTX *ATX);

int send_notice		(const char *filename, const char *mailer_args,
			 const char *username);

int write_web_stats     (const char *username, const char *group, 
                         struct _ds_spam_totals *totals);

int ensure_confident_result	(DSPAM_CTX *CTX, AGENT_CTX *ATX, int result);
DSPAM_CTX *ctx_init	(AGENT_CTX *ATX, AGENT_PREF PTX, const char *username);
int log_events		(DSPAM_CTX *CTX);
int retrain_message	(DSPAM_CTX *CTX, AGENT_CTX *ATX);
int tag_message		(struct _ds_message_block *block, AGENT_PREF PTX);
int quarantine_message  (const char *message, const char *username);
int **process_users       (AGENT_CTX *ATX, buffer *message);
int find_signature	(DSPAM_CTX *CTX, AGENT_CTX *ATX, AGENT_PREF PTX);
int add_xdspam_headers	(DSPAM_CTX *CTX, AGENT_CTX *ATX,  AGENT_PREF PTX);
int embed_signature	(DSPAM_CTX *CTX, AGENT_CTX *ATX, AGENT_PREF PTX);
int tracksource		(DSPAM_CTX *CTX);
buffer *read_stdin	(AGENT_CTX *ATX);

#ifdef NEURAL
int process_neural_decision(DSPAM_CTX *CTX, struct _ds_neural_decision *DEC);
#endif

#define DSM_DAEMON	0xFE

#endif /* _DSPAM_H */

