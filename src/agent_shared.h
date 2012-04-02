/* $Id: agent_shared.h,v 1.35 2011/07/11 21:27:15 sbajic Exp $ */

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

#include <sys/types.h>
#ifndef _WIN32
#include <pwd.h>
#endif
#include "buffer.h"

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif
#include "libdspam.h"

#ifndef _AGENT_SHARED_H
#  define _AGENT_SHARED_H

extern char *__pw_name;
extern uid_t __pw_uid;

#define STATUS( ... )   snprintf(ATX->status, sizeof(ATX->status), __VA_ARGS__);

#define SYNTAX "Syntax: dspam [--help|--version|[--client|--daemon [--nofork]] [--debug] --mode=[toe|tum|teft|notrain|unlearn] --user [user1 user2 ... userN] [--feature=[no,wh,tb=N]] [--class=[spam|innocent]] [--source=[error|corpus|inoculation]] [--profile=[PROFILE]] [--deliver=[spam,[innocent|nonspam],summary,stdout]] [--process|--classify] [--stdout] [--mail-from=sender-address] [--rcpt-to recipient-address(es)] [--signature=DSPAM-Signature] [passthru-arguments]]"

#define         SIGNATURE_BEGIN		"!DSPAM:"
#define         SIGNATURE_END		"!"
#define         LOOSE_SIGNATURE_BEGIN	"X-DSPAM-Signature:"
#define         SIGNATURE_DELIMITER	": "

/* AGENT_CTX: Agent context. Defines the behavior of the agent */

typedef struct {
  int operating_mode;			/* Processing Mode       IN DSM_ */
  int fork;				/* Fork daemon:          IN 1:0  */
  int client_mode;			/* Client Mode:          IN 1:0  */
  int training_mode;			/* Training Mode         IN DST_ */
  int classification;			/* Classification        IN DSR_ */
  int source;				/* Classification Source IN DSS_ */
  int spam_action;			/* Action on Spam        IN DSA_ */
#ifdef TRUSTED_USER_SECURITY
  int trusted;				/* Trusted User?         IN      */
#endif
  int feature;				/* Feature Overridden?   IN      */
  int train_pristine;			/* Train Pristine?       IN      */
  int tokenizer;			/* Tokenizer             IN      */
  void *dbh;				/* Database Handle       IN      */
  u_int64_t flags;			/* Flags DAF_            IN      */
  int training_buffer;			/* Sedation Level 0-10   IN      */
  char *recipient;			/* Current Recipient             */
  char mailer_args[256];		/* Delivery Args         IN      */
  char spam_args[256];			/* Quarantine Args       IN      */
  char managed_group[256];		/* Managed Groupname     IN      */
  char profile[32];			/* Storage Profile       IN      */
  char signature[128];			/* Signature Serial      IN/OUT  */
  char mailfrom[256];			/* For LMTP or SMTP              */
  struct nt *users;			/* Destination Users     IN      */
  struct nt *inoc_users;		/* Inoculate list        OUT     */
  struct nt *classify_users;		/* Classify list         OUT     */
  struct nt *recipients;		/* Recipients            IN      */
  struct nt *results;			/* Process Results       OUT     */
  struct _ds_spam_signature SIG;	/* Signature object      OUT     */
  int learned;                  	/* Message learned?      OUT     */
  FILE *sockfd;				/* Socket FD if not STDOUT       */
  int sockfd_output;			/* Output sent to sockfd?        */
  char client_args[1024];		/* Args for client connection    */
  double timestart;
  agent_pref_t PTX;

  char status[256];
#ifdef DEBUG
  char debug_args[1024];
#endif
#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  struct passwd *p;
#if defined(_REENTRANT) && defined(HAVE_GETPWUID_R)
  struct passwd pwbuf;
#endif
#endif
#endif
} AGENT_CTX;

int process_features	(AGENT_CTX *ATX, const char *features);
int process_mode	(AGENT_CTX *ATX, const char *mode);
int check_configuration (AGENT_CTX *ATX);
int apply_defaults      (AGENT_CTX *ATX);
int process_arguments   (AGENT_CTX *ATX, int argc, char **argv);
int initialize_atx      (AGENT_CTX *ATX);
int process_parseto	(AGENT_CTX *ATX, const char *buf);
buffer *read_stdin	(AGENT_CTX *ATX);

int init_pwent_cache(void);

#ifndef MIN
#   define MAX(a,b)  ((a)>(b)?(a):(b))
#   define MIN(a,b)  ((a)<(b)?(a):(b))
#endif /* !MIN */

/*
 * Agent context flag (DAF)
 * Do not confuse with libdspam's classification context flags (DSF)
 *
 */

#define DAF_STDOUT		0x01
#define DAF_DELIVER_SPAM	0x02
#define DAF_DELIVER_INNOCENT	0x04
#define DAF_WHITELIST		0x08
#define DAF_GLOBAL		0x10
#define DAF_INOCULATE		0x20
#define DAF_NOISE		0x40
#define DAF_MERGED		0x80
#define DAF_SUMMARY		0x100
#define DAF_UNLEARN		0x200
#define DAF_FIXED_TR_MODE	0x400

#define DAZ_WORD		0x01
#define DAZ_CHAIN		0x02
#define DAZ_SBPH		0x03
#define DAZ_OSB			0x04

#endif /* _AGENT_SHARED_H */
