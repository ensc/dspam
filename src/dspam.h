/* $Id: dspam.h,v 1.3 2004/11/24 17:57:47 jonz Exp $ */

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

#ifndef _DSPAM_H
#  define _DSPAM_H

#define SYNTAX "Syntax: dspam --mode=[toe|tum|teft|notrain] --user [user1 user2 ... userN] [--feature=[ch,no,wh,tb=N,sbph]] [--class=[spam|innocent]] [--source=[error|corpus|inoculation]] [--profile=[PROFILE]] [--deliver=[spam,innocent]] [--process|--classify] [--stdout] [passthru-arguments]"

/* Signature identifiers; don't modify unless you understand the 
   subtle consequences */

#define         SIGNATURE_BEGIN		"!DSPAM:"
#define         SIGNATURE_END		"!"
#define         LOOSE_SIGNATURE_BEGIN	"X-DSPAM-Signature:"
#define         SIGNATURE_DELIMITER	": "

/* Agent Context: Agent Configuration */

typedef struct {
  int operating_mode;   /* DSM_ */
  int training_mode;    /* DST_ */
  int classification;   /* DSR_ */
  int source;           /* DSS_ */
  int spam_action;	/* DSA_ */
  int learned;
  int trusted;		/* Trusted User */
  int feature;		/* Features supplied on cmdline? 0/1 */
  u_int32_t flags;      /* DAF_ */
  int training_buffer;	/* 0-10 */
  char mailer_args[256];
  char spam_args[256];
  char managed_group[256];
  char profile[32];	/* Storage Profile */
  char signature[128];
  struct nt *users;	/* Destination Users */
#ifdef DEBUG
  char debug_args[1024];
#endif
#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  struct passwd *p;
#endif
#endif
} AGENT_CTX;

/* Public agent functions */

int deliver_message	(const char *message, 
			 const char *mailer_args,
			 const char *username);

int process_message	(AGENT_CTX *ATX, 
                         AGENT_PREF PTX,
			 buffer *message,
			 const char *username);

int inoculate_user	(const char *username,
			 struct _ds_spam_signature *SIG,
			 const char *message,
                         AGENT_CTX *ATX);

int user_classify	(const char *username,
			 struct _ds_spam_signature *SIG,
			 const char *message,
                         AGENT_CTX *ATX);

int send_notice		(const char *filename,
			 const char *mailer_args,
			 const char *username);

int write_web_stats     (const char *username, 
                         const char *group, 
                         struct _ds_spam_totals *totals);

int tag_message		(struct _ds_message_block *block, AGENT_PREF PTX);
int quarantine_message  (const char *message, const char *username);
int process_features	(AGENT_CTX *ATX, const char *features);
int process_mode	(AGENT_CTX *ATX, const char *mode);
int check_configuration (AGENT_CTX *ATX);
int apply_defaults      (AGENT_CTX *ATX);
int process_arguments   (AGENT_CTX *ATX, int argc, char **argv);
int process_users       (AGENT_CTX *ATX, buffer *message);
int initialize_atx      (AGENT_CTX *ATX);
buffer *read_stdin	(AGENT_CTX *ATX);

#ifndef MIN
#   define MAX(a,b)  ((a)>(b)?(a):(b))
#   define MIN(a,b)  ((a)<(b)?(a):(b))
#endif /* !MIN */

#ifdef EXPERIMENTAL

/* Inoculation Authentication Types */

#define IAT_NONE	0x00
#define IAT_MD5		0x01
#define IAT_SIGNED	0x02
#define IAT_UNKNOWN	0xFF
#endif

/*
 *  DSPAM Agent Context Flags (DAF)
    Do not confuse with libdspam's classification context flags (DSF)
 */

#define DAF_STDOUT		0x01
#define DAF_DELIVER_SPAM	0x02
#define DAF_DELIVER_INNOCENT	0x04
#define DAF_NEURAL		0x08 /* No Context */
#define DAF_WHITELIST		0x08 
#define DAF_GLOBAL		0x10 /* No Context */
#define DAF_INOCULATE		0x20 /* No Context */
#define DAF_CHAINED		0x40
#define DAF_NOISE		0x80
#define DAF_MERGED		0x100
#define DAF_SUMMARY		0x200
#define DAF_SBPH		0x400
#define DAF_UNLEARN		0x800

#endif /* _DSPAM_H */

