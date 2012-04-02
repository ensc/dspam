/* $Id: dspam.c,v 1.412 2011/11/10 00:26:00 tomhendr Exp $ */

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
 * dspam.c - primary dspam processing agent
 *
 * DESCRIPTION
 *   The agent provides a commandline interface to the libdspam core engine
 *   and also provides advanced functions such as a daemonized LMTP server,
 *   extended groups, and other agent features outlined in the documentation.
 *
 *   This codebase is the full client/processing engine. See dspamc.c for
 *     the lightweight client-only codebase.
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#include <pwd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sysexits.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#define WIDEXITED(x) 1
#define WEXITSTATUS(x) (x)
#include <windows.h>
#else
#include <sys/wait.h>
#include <sys/param.h>
#endif
#include "config.h"
#include "util.h"
#include "read_config.h"

#ifdef DAEMON
#include <pthread.h>
#include "daemon.h"
#include "client.h"
#endif

#ifdef TIME_WITH_SYS_TIME
#   include <sys/time.h>
#   include <time.h>
#else
#   ifdef HAVE_SYS_TIME_H
#       include <sys/time.h>
#   else
#       include <time.h>
#   endif
#endif

#ifdef EXT_LOOKUP
#include "external_lookup.h"
int verified_user = 0;
#endif

#include "dspam.h"
#include "agent_shared.h"
#include "pref.h"
#include "libdspam.h"
#include "language.h"
#include "buffer.h"
#include "base64.h"
#include "heap.h"
#include "pref.h"
#include "config_api.h"

#define USE_LMTP        (_ds_read_attribute(agent_config, "DeliveryProto") && !strcmp(_ds_read_attribute(agent_config, "DeliveryProto"), "LMTP"))
#define USE_SMTP        (_ds_read_attribute(agent_config, "DeliveryProto") && !strcmp(_ds_read_attribute(agent_config, "DeliveryProto"), "SMTP"))
#define LOOKUP(A, B)	((_ds_pref_val(A, "localStore")[0]) ? _ds_pref_val(A, "localStore") : B)


int
main (int argc, char *argv[])
{
  AGENT_CTX ATX;		/* agent configuration */
  buffer *message = NULL;	/* input message */
  int agent_init = 0;		/* agent is initialized */
  int driver_init = 0;		/* storage driver is initialized */
  int pwent_cache_init = 0;	/* cache for username and uid is initialized */
  int exitcode = EXIT_SUCCESS;
  struct nt_node *node_nt;
  struct nt_c c_nt;

  srand ((long) time(NULL) ^ (long) getpid ());
  umask (006);                  /* rw-rw---- */
  setbuf (stdout, NULL);	/* unbuffered output */
#ifdef DEBUG
  DO_DEBUG = 0;
#endif

#ifdef DAEMON
  pthread_mutex_init(&__syslog_lock, NULL);
#endif

  /* Cache my username and uid for trusted user security */

  if (!init_pwent_cache()) {
    LOG(LOG_ERR, ERR_AGENT_RUNTIME_USER);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  } else
    pwent_cache_init = 1;

  /* Read dspam.conf into global config structure (ds_config_t) */

  agent_config = read_config(NULL);
  if (!agent_config) {
    LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  if (!_ds_read_attribute(agent_config, "Home")) {
    LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  /* Set up an agent context to define the behavior of the processor */

  if (initialize_atx(&ATX)) {
    LOG(LOG_ERR, ERR_AGENT_INIT_ATX);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  } else {
    agent_init = 1;
  }

  if (process_arguments(&ATX, argc, argv)) {
    LOG(LOG_ERR, ERR_AGENT_INIT_ATX);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  /* Switch into daemon mode if --daemon was specified on the commandline */

#ifdef DAEMON
#ifdef TRUSTED_USER_SECURITY
  if (ATX.operating_mode == DSM_DAEMON && ATX.trusted)
#else
  if (ATX.operating_mode == DSM_DAEMON)
#endif
  {
    exitcode = daemon_start(&ATX);

    if (agent_init) {
      nt_destroy(ATX.users);
      nt_destroy(ATX.recipients);
    }

    if (agent_config)
      _ds_destroy_config(agent_config);

    pthread_mutex_destroy(&__syslog_lock);
    if (pwent_cache_init)
      free(__pw_name);
    exit(exitcode);
  }
#endif

  if (apply_defaults(&ATX)) {
    LOG(LOG_ERR, ERR_AGENT_INIT_ATX);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  if (check_configuration(&ATX)) {
    LOG(LOG_ERR, ERR_AGENT_MISCONFIGURED);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  /* Read the message in and apply ParseTo services */

  message = read_stdin(&ATX);
  if (message == NULL) {
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  if (ATX.users->items == 0)
  {
    LOG(LOG_ERR, ERR_AGENT_USER_UNDEFINED);
    fprintf (stderr, "%s\n", SYNTAX);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  /* Perform client-based processing of message if --client was specified */

#ifdef DAEMON
  if (ATX.client_mode &&
      _ds_read_attribute(agent_config, "ClientIdent") &&
      (_ds_read_attribute(agent_config, "ClientHost") ||
       _ds_read_attribute(agent_config, "ServerDomainSocketPath")))
  {
    exitcode = client_process(&ATX, message);
    if (exitcode<0) {
      LOG(LOG_ERR, ERR_CLIENT_EXIT, exitcode);
    }
  } else {
#endif

  /* Primary (non-client) processing procedure */

  if (libdspam_init(_ds_read_attribute(agent_config, "StorageDriver"))) {
    LOG(LOG_CRIT, ERR_DRV_INIT);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  }

  if (dspam_init_driver (NULL))
  {
    LOG (LOG_WARNING, ERR_DRV_INIT);
    exitcode = EXIT_FAILURE;
    goto BAIL;
  } else {
    driver_init = 1;
  }

  ATX.results = nt_create(NT_PTR);
  if (ATX.results == NULL) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    exitcode = EUNKNOWN;
    goto BAIL;
  }

  exitcode = process_users(&ATX, message);
  if (exitcode) {
    LOGDEBUG("process_users() failed on error %d", exitcode);
  } else {
    exitcode = 0;
    node_nt = c_nt_first(ATX.results, &c_nt);
    while(node_nt) {
      agent_result_t result = (agent_result_t) node_nt->ptr;
      if (result->exitcode)
        exitcode--;
      node_nt = c_nt_next(ATX.results, &c_nt);
    }
  }
  nt_destroy(ATX.results);
  node_nt = NULL;

#ifdef DAEMON
  }
#endif

BAIL:

  if (message)
    buffer_destroy(message);

  if (agent_init) {
    nt_destroy(ATX.users);
    nt_destroy(ATX.recipients);
  }

#ifdef DAEMON
  if (agent_init) {
    if (!ATX.client_mode) {
#endif
  if (driver_init)
    dspam_shutdown_driver(NULL);
  libdspam_shutdown();
#ifdef DAEMON
    }
  }
#endif

  if (agent_config)
    _ds_destroy_config(agent_config);

  if (pwent_cache_init)
    free(__pw_name);
#ifdef DAEMON
  pthread_mutex_destroy(&__syslog_lock);
  // pthread_exit(0);
#endif
  exit(exitcode);
}

/*
 * process_message(AGENT_CTX *ATX, buffer *message, const char *username)
 *
 * DESCRIPTION
 *   Core message processing / interface to libdspam
 *   This function should be called once for each destination user
 *
 * INPUT ARGUMENTS
 *   ATX	Agent context defining processing behavior
 *   message	Buffer structure containing the message
 *   username   Destination user
 *
 * RETURN VALUES
 *   The processing result is returned:
 *
 *   DSR_ISINNOCENT	Message is innocent
 *   DSR_ISSPAM		Message is spam
 *   (other)		Error code (see libdspam.h)
 */

int
process_message (
  AGENT_CTX *ATX,
  buffer * message,
  const char *username,
  char **result_string)
{
  DSPAM_CTX *CTX = NULL;		/* (lib)dspam context */
  ds_message_t components;
  char *copyback;
  int have_signature = 0;
  int result, i;
  int internally_canned = 0;

  ATX->timestart = _ds_gettime();	/* set tick count to get run time */

  if (message->data == NULL) {
    LOGDEBUG("empty message provided");
    return EINVAL;
  }

  /* Create a dspam context based on the agent context */

  CTX = ctx_init(ATX, username);
  if (CTX == NULL) {
    LOG (LOG_WARNING, ERR_CORE_INIT);
    result = EUNKNOWN;
    goto RETURN;
  }

  /* Configure libdspam's storage properties, then attach storage */

  set_libdspam_attributes(CTX);
  if (ATX->sockfd && ATX->dbh == NULL)
    ATX->dbh = _ds_connect(CTX);

  /* Re-Establish database connection (if failed) */

  if (attach_context(CTX, ATX->dbh)) {
    if (ATX->sockfd) {
      ATX->dbh = _ds_connect(CTX);
      LOG(LOG_ERR, ERR_CORE_REATTACH);

      if (attach_context(CTX, ATX->dbh)) {
        LOG(LOG_ERR, ERR_CORE_ATTACH);
        result = EUNKNOWN;
        goto RETURN;
      }
    } else {
      LOG(LOG_ERR, ERR_CORE_ATTACH);
      result = EUNKNOWN;
      goto RETURN;
    }
  }

  /* Parse and decode the message into our message structure (ds_message_t) */

  components = _ds_actualize_message (message->data);
  if (components == NULL) {
    LOG (LOG_ERR, ERR_AGENT_PARSER_FAILED);
    result = EUNKNOWN;
    goto RETURN;
  }

  CTX->message = components;

#ifdef CLAMAV
  /* Check for viruses */

  if (_ds_read_attribute(agent_config, "ClamAVPort") &&
      _ds_read_attribute(agent_config, "ClamAVHost") &&
      CTX->source != DSS_ERROR                       &&
      strcmp(_ds_pref_val(ATX->PTX, "optOutClamAV"), "on"))
  {
    if (has_virus(message)) {
      char ip[32];
      CTX->result = DSR_ISSPAM;
      CTX->probability = 1.0;
      CTX->confidence = 1.0;
      STATUS("A virus was detected in the message contents");
      result = DSR_ISSPAM;
      strcpy(CTX->class, LANG_CLASS_VIRUS);
      internally_canned = 1;
      if(!_ds_match_attribute(agent_config, "TrackSources", "virus")) {
        if (!dspam_getsource (CTX, ip, sizeof (ip)))
        {
          LOG(LOG_WARNING, "virus warning: infected message from %s", ip);
        }
      }
    }
  }
#endif

  /* Check for a domain blocklist (user-based setting) */

  if (is_blocklisted(CTX, ATX)) {
    CTX->result = DSR_ISSPAM;
    CTX->probability = 1.0;
    CTX->confidence = 1.0;
    strcpy(CTX->class, LANG_CLASS_BLOCKLISTED);
    internally_canned = 1;
  }

  /* Check for an RBL blacklist (system-based setting) */

  if (CTX->classification == DSR_NONE &&
      _ds_read_attribute(agent_config, "Lookup"))
  {
    if(strcasecmp(_ds_pref_val(ATX->PTX, "ignoreRBLLookups"), "on")) {
      int bad = is_blacklisted(CTX, ATX);
      if (bad) {
        if ((_ds_match_attribute(agent_config, "RBLInoculate", "on") ||
             !strcasecmp(_ds_pref_val(ATX->PTX, "RBLInoculate"), "on")) &&
             strcasecmp(_ds_pref_val(ATX->PTX, "RBLInoculate"), "off")) {
          LOGDEBUG("source address is blacklisted. learning as spam.");
          CTX->classification = DSR_ISSPAM;
          CTX->source = DSS_INOCULATION;
        } else {
          CTX->result = DSR_ISSPAM;
          CTX->probability = 1.0;
          CTX->confidence = 1.0;
          strcpy(CTX->class, LANG_CLASS_BLACKLISTED);
          internally_canned = 1;
        }
      }
    }
  }

  /* Process a signature if one was provided */

  have_signature = find_signature(CTX, ATX);
  if (ATX->source == DSS_CORPUS || ATX->source == DSS_NONE)
    have_signature = 0; /* ignore sigs from corpusfed and inbound email */

  char *original_username = strdup(CTX->username);

  if (have_signature)
  {

    if (_ds_get_signature (CTX, &ATX->SIG, ATX->signature))
    {
      LOG(LOG_WARNING, ERR_AGENT_SIG_RET_FAILED, ATX->signature);
      have_signature = 0;
    }
    else {

      /* uid-based signatures will change the active username, so reload
         preferences if it has changed */

      CTX->signature = &ATX->SIG;
      if (!strcasecmp(CTX->username, original_username)) {
        if (ATX->PTX)
          _ds_pref_free(ATX->PTX);
        free(ATX->PTX);
        ATX->PTX = NULL;

        ATX->PTX = load_aggregated_prefs(ATX, CTX->username);

        ATX->train_pristine = 0;
        if ((_ds_match_attribute(agent_config, "TrainPristine", "on") ||
            !strcmp(_ds_pref_val(ATX->PTX, "trainPristine"), "on")) &&
            strcmp(_ds_pref_val(ATX->PTX, "trainPristine"), "off")) {
                ATX->train_pristine = 1;
        }

        /* Change also the mail recipient */
        ATX->recipient = CTX->username;

      }
    }
  } else if (CTX->operating_mode == DSM_CLASSIFY ||
             CTX->classification != DSR_NONE)
  {
    CTX->flags = CTX->flags ^ DSF_SIGNATURE;
    CTX->signature = NULL;
  }

  if (have_signature && CTX->classification != DSR_NONE) {

    /*
     * Reclassify (or retrain) message by signature
     */

    if (retrain_message(CTX, ATX) != 0) {
      have_signature = 0;
      result = EFAILURE;
      goto RETURN;
    }
  } else {
    CTX->signature = NULL;
    if (! ATX->train_pristine) {
      if (CTX->classification != DSR_NONE && CTX->source == DSS_ERROR) {
        LOG(LOG_WARNING, ERR_AGENT_NO_VALID_SIG);
        result = EFAILURE;
        goto RETURN;
      }
    }

    /*
     * Call libdspam to process the environment we've configured
     */

    if (!internally_canned) {
      result = dspam_process (CTX, message->data);
      if (result != 0) {
        result = EFAILURE;
        goto RETURN;
      }
    }
  }

  result = CTX->result;

  if (result == DSR_ISINNOCENT && !strcmp(CTX->class, LANG_CLASS_WHITELISTED)) {
    STATUS("Auto-Whitelisted");
  }

  /*
   * Send any relevant notifications to the user (first spam, etc)
   * Only if the process was successful
   */

  if (result == DSR_ISINNOCENT || result == DSR_ISSPAM)
  {
    do_notifications(CTX, ATX);
  }

  /* Consult global group or classification network */
  result = ensure_confident_result(CTX, ATX, result);
  if (result < 0)
    goto RETURN;

  /* Inoculate other users (signature) */

  if (have_signature                   &&
     CTX->classification == DSR_ISSPAM &&
     CTX->source != DSS_CORPUS         &&
     ATX->inoc_users->items > 0)
  {
    struct nt_node *node_int;
    struct nt_c c_i;

    node_int = c_nt_first (ATX->inoc_users, &c_i);
    while (node_int != NULL)
    {
      inoculate_user (ATX, (const char *) node_int->ptr, &ATX->SIG, NULL);
      node_int = c_nt_next (ATX->inoc_users, &c_i);
    }
  }

  /* Inoculate other users (message) */

  if (!have_signature                   &&
      CTX->classification == DSR_ISSPAM &&
      CTX->source != DSS_CORPUS         &&
      ATX->inoc_users->items > 0)
  {
    struct nt_node *node_int;
    struct nt_c c_i;
    node_int = c_nt_first (ATX->inoc_users, &c_i);
    while (node_int != NULL)
    {
      inoculate_user (ATX, (const char *) node_int->ptr, NULL, message->data);
      node_int = c_nt_next (ATX->inoc_users, &c_i);
    }
    inoculate_user (ATX, CTX->username, NULL, message->data);
    result = DSR_ISSPAM;
    CTX->result = DSR_ISSPAM;

    goto RETURN;
  }

  /* Generate a signature id for the message and store */

  if (internally_canned) {
    if (CTX->signature) {
      free(CTX->signature->data);
      free(CTX->signature);
      CTX->signature = NULL;
    }
    CTX->signature = calloc(1, sizeof(struct _ds_spam_signature));
    if (CTX->signature) {
      CTX->signature->length = 8;
      CTX->signature->data = calloc(1, (CTX->signature->length));
    }
  }

  if (internally_canned || (CTX->operating_mode == DSM_PROCESS &&
      CTX->classification == DSR_NONE    &&
      CTX->signature != NULL))
  {
    int valid = 0;

    while (!valid)
    {
      _ds_create_signature_id (CTX, ATX->signature, sizeof (ATX->signature));
      if (_ds_verify_signature (CTX, ATX->signature))
          valid = 1;
    }
    LOGDEBUG ("saving signature as %s", ATX->signature);

    if (CTX->classification == DSR_NONE && CTX->training_mode != DST_NOTRAIN)
    {
      if (!ATX->train_pristine) {
        int x = _ds_set_signature (CTX, CTX->signature, ATX->signature);
        if (x) {
          LOG(LOG_WARNING, "_ds_set_signature() failed with error %d", x);
        }
      }
    }
  }

  /* Restore original username if necessary */

  if (CTX->group != NULL && strcasecmp(CTX->username, original_username) != 0)
  {
    LOGDEBUG ("restoring original username %s after group processing as %s", original_username, CTX->username);
    CTX->username = original_username;
  }

  /* Write .stats file for web interface */

  if (CTX->training_mode != DST_NOTRAIN && _ds_match_attribute(agent_config, "WebStats", "on")) {
    write_web_stats (
      ATX,
      (CTX->group == NULL || CTX->flags & DSF_MERGED) ?
        CTX->username : CTX->group,
      (CTX->group != NULL && CTX->flags & DSF_MERGED) ?
        CTX->group: NULL,
      &CTX->totals);
  }

  LOGDEBUG ("libdspam returned probability of %f", CTX->probability);
  LOGDEBUG ("message result: %s", (result != DSR_ISSPAM) ? "NOT SPAM" : "SPAM");

  /* System and User logging */

  if (CTX->operating_mode != DSM_CLASSIFY &&
     (_ds_match_attribute(agent_config, "SystemLog", "on") ||
      _ds_match_attribute(agent_config, "UserLog", "on")))
  {
    log_events(CTX, ATX);
  }

  /*  Fragment Store - Store 1k fragments of each message for web users who
   *  want to be able to see them from history. This requires some type of
   *  find recipe for purging
   */

  if (ATX->PTX != NULL
      && !strcmp(_ds_pref_val(ATX->PTX, "storeFragments"), "on")
      && CTX->source != DSS_ERROR)
  {
    char dirname[MAX_FILENAME_LENGTH];
    char corpusfile[MAX_FILENAME_LENGTH];
    char output[1024];
    FILE *file;

    _ds_userdir_path(dirname, _ds_read_attribute(agent_config, "Home"),
                 LOOKUP(ATX->PTX, (ATX->managed_group[0]) ? ATX->managed_group :
                                   CTX->username), "frag");
    snprintf(corpusfile, MAX_FILENAME_LENGTH, "%s/%s.frag",
      dirname, ATX->signature);

    LOGDEBUG("writing to frag file %s", corpusfile);

    _ds_prepare_path_for(corpusfile);
    file = fopen(corpusfile, "w");
    if (file != NULL) {
      char *body = strstr(message->data, "\n\n");
      if (!body)
        body = message->data;
      strlcpy(output, body, sizeof(output));
      fputs(output, file);
      fputs("\n", file);
      fclose(file);
    }
  }

  /* Corpus Maker - Build a corpus in DSPAM_HOME/data/USERPATH/USER.corpus */

  if (ATX->PTX != NULL && !strcmp(_ds_pref_val(ATX->PTX, "makeCorpus"), "on")) {
    if (ATX->source != DSS_ERROR) {
      char dirname[MAX_FILENAME_LENGTH];
      char corpusfile[MAX_FILENAME_LENGTH];
      FILE *file;

      _ds_userdir_path(dirname, _ds_read_attribute(agent_config, "Home"),
                   LOOKUP(ATX->PTX, (ATX->managed_group[0]) ? ATX->managed_group
                                     : CTX->username), "corpus");
      snprintf(corpusfile, MAX_FILENAME_LENGTH, "%s/%s/%s.msg",
        dirname, (result == DSR_ISSPAM) ? "spam" : "nonspam",
        ATX->signature);

      LOGDEBUG("writing to corpus file %s", corpusfile);

      _ds_prepare_path_for(corpusfile);
      file = fopen(corpusfile, "w");
      if (file != NULL) {
        fputs(message->data, file);
        fclose(file);
      }
    } else {
      char dirname[MAX_FILENAME_LENGTH];
      char corpusfile[MAX_FILENAME_LENGTH];
      char corpusdest[MAX_FILENAME_LENGTH];

      _ds_userdir_path(dirname, _ds_read_attribute(agent_config, "Home"),
                   LOOKUP(ATX->PTX, (ATX->managed_group[0]) ? ATX->managed_group
                                     : CTX->username), "corpus");
      snprintf(corpusdest, MAX_FILENAME_LENGTH, "%s/%s/%s.msg",
        dirname, (result == DSR_ISSPAM) ? "spam" : "nonspam",
        ATX->signature);
      snprintf(corpusfile, MAX_FILENAME_LENGTH, "%s/%s/%s.msg",
        dirname, (result == DSR_ISSPAM) ? "nonspam" : "spam",
        ATX->signature);
      LOGDEBUG("moving corpusfile %s -> %s", corpusfile, corpusdest);
      _ds_prepare_path_for(corpusdest);
      rename(corpusfile, corpusdest);
    }
  }

  /* False positives and spam misses should return here */

  if (CTX->message == NULL)
    goto RETURN;

  /* Add headers, tag, and deliver if necessary */

  {
    add_xdspam_headers(CTX, ATX);
  }

  if (!strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "tag") &&
      result == DSR_ISSPAM)
  {
    tag_message(ATX, CTX->message);
  }


  if (
         (!strcmp(_ds_pref_val(ATX->PTX, "tagSpam"), "on")
          && CTX->result == DSR_ISSPAM)
         ||
         (!strcmp(_ds_pref_val(ATX->PTX, "tagNonspam"), "on")
          && CTX->result == DSR_ISINNOCENT)
     )
  {
     i = embed_msgtag(CTX, ATX);
     if (i<0) {
         result = i;
         goto RETURN;
     }
  }

  if (strcmp(_ds_pref_val(ATX->PTX, "signatureLocation"), "headers") &&
      !ATX->train_pristine &&
       (CTX->classification == DSR_NONE || internally_canned))
  {
    i = embed_signature(CTX, ATX);
    if (i<0) {
      result = i;
      goto RETURN;
    }
  }

  /* Reassemble message from components */

  copyback = _ds_assemble_message (CTX->message, (USE_LMTP || USE_SMTP) ? "\r\n" : "\n");
  buffer_clear (message);
  buffer_cat (message, copyback);
  free (copyback);

  /* Track source address and report to syslog, RABL */

  if ( _ds_read_attribute(agent_config, "TrackSources") &&
       CTX->operating_mode == DSM_PROCESS               &&
       CTX->source != DSS_CORPUS &&
       CTX->source != DSS_ERROR)
  {
    tracksource(CTX);
  }

  /* Print --classify output */

  if (CTX->operating_mode == DSM_CLASSIFY || ATX->flags & DAF_SUMMARY)
  {
    char data[128];
    FILE *fout;

    switch (CTX->result) {
      case DSR_ISSPAM:
        strcpy(data, "Spam");
        break;
      default:
        strcpy(data, "Innocent");
        break;
    }

    if (ATX->sockfd) {
      fout = ATX->sockfd;
      ATX->sockfd_output = 1;
    }
    else {
      fout = stdout;
    }

    fprintf(fout, "X-DSPAM-Result: %s; result=\"%s\"; class=\"%s\"; "
                  "probability=%01.4f; confidence=%02.2f; signature=%s\n",
           CTX->username,
           data,
           CTX->class,
           CTX->probability,
           CTX->confidence,
           (ATX->signature[0]) ? ATX->signature : "N/A");
  }

  ATX->learned = CTX->learned;
  if (result_string)
    *result_string = strdup(CTX->class);

RETURN:
  if (have_signature) {
    if (ATX->SIG.data != NULL) {
      free(ATX->SIG.data);
      ATX->SIG.data = NULL;
    }
  }
  ATX->signature[0] = 0;
  nt_destroy (ATX->inoc_users);
  nt_destroy (ATX->classify_users);
  if (CTX) {
    if (CTX->signature == &ATX->SIG) {
      CTX->signature = NULL;
    } else if (CTX->signature != NULL) {
      if (CTX->signature->data != NULL) {
        free (CTX->signature->data);
      }
      free (CTX->signature);
      CTX->signature = NULL;
    }
    dspam_destroy (CTX);
  }
  return result;
}

/*
 * deliver_message(AGENT_CTX *ATX, const char *message,
 *    const char *mailer_args, const char *username, FILE *stream,
 *    int result)
 *
 * DESCRIPTION
 *   Deliver message to the appropriate destination. This could be one of:
 *     - Trusted/Untrusted Delivery Agent
 *     - Delivery Host (SMTP/LMTP)
 *     - Quarantine Agent
 *     - File stream (--stdout)
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   message      Message to be delivered
 *   mailer_args  Arguments to pass to local agents via pipe()
 *   username     Destination username
 *   stream       File stream (if any) for stdout delivery
 *   result       Message classification result (DSR_)
 *
 * RETURN VALUES
 *   returns 0 on success
 *   EINVAL on permanent failure
 *   EFAILURE on temporary failure
 *   EFILE local agent failure
 */

int
deliver_message (
  AGENT_CTX *ATX,
  const char *message,
  const char *mailer_args,
  const char *username,
  FILE *stream,
  int result)
{
  char args[1024];
  char *margs, *mmargs, *arg;
  FILE *file;
  int rc;

#ifdef DAEMON

  /* If QuarantineMailbox defined and delivering a spam, get
   * name of recipient, truncate possible "+detail", and
   * add the QuarantineMailbox name (that must include the "+")
   */

  if ((_ds_read_attribute(agent_config, "QuarantineMailbox")) &&
      (result == DSR_ISSPAM)) {
    strlcpy(args, ATX->recipient, sizeof(args));

    /* strip trailing @domain, if present: */
    arg=index(args, '@');
    if (arg) *arg = '\0';

    arg=index(args,'+');
    if (arg != NULL) *arg='\0';
    strlcat(args,_ds_read_attribute(agent_config, "QuarantineMailbox"),
            sizeof(args));

    /* append trailing @domain again, if it was present: */
    arg=index(ATX->recipient, '@');
    if (arg) strlcat (args, arg, sizeof(args));

    ATX->recipient=args;
  }

  /* If (using LMTP or SMTP) and (not delivering to stdout) and
   * (we shouldn't be delivering this to a quarantine agent)
   * then call deliver_socket to deliver to DeliveryHost
   */

  if (
    (USE_LMTP || USE_SMTP) && ! (ATX->flags & DAF_STDOUT) &&
    (!(result == DSR_ISSPAM &&
       _ds_read_attribute(agent_config, "QuarantineAgent") &&
       ATX->PTX && !strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "quarantine")))
  )
  {
    return deliver_socket(ATX, message, (USE_LMTP) ? DDP_LMTP : DDP_SMTP);
  }
#endif

  if (message == NULL)
    return EINVAL;

  /* If we're delivering to stdout, we need to provide a classification for
   * use by client/daemon setups where the client needs to know the result
   * in order to support broken returnCodes.
   */

  if (ATX->sockfd && ATX->flags & DAF_STDOUT)
    fprintf(stream, "X-Daemon-Classification: %s\n",
                     (result == DSR_ISSPAM) ? "SPAM" : "INNOCENT");

  if (mailer_args == NULL) {
    fputs (message, stream);
    return 0;
  }

  /* Prepare local mailer args and interpolate all special symbols */

  args[0] = 0;
  margs = strdup (mailer_args);
  mmargs = margs;
  arg = strsep (&margs, " ");
  while (arg != NULL)
  {
    char a[256], b[256];

    /* Destination user */

    if (!strcmp (arg, "$u") || !strcmp (arg, "\\$u") ||
        !strcmp (arg, "%u") || !strcmp(arg, "\\%u"))
    {
      strlcpy(a, username, sizeof(a));
    }

    /* Recipient (from RCPT TO)*/

    else if (!strcmp (arg, "%r") || !strcmp (arg, "\\%r"))
    {
      if (ATX->recipient)
        strlcpy(a, ATX->recipient, sizeof(a));
      else
        strlcpy(a, username, sizeof(a));
    }

    /* Sender (from MAIL FROM) */

    else if (!strcmp (arg, "%s") || !strcmp (arg, "\\%s"))
      strlcpy(a, ATX->mailfrom, sizeof(a));

    else
      strlcpy(a, arg, sizeof(a));

    /* Escape special characters */

    if (strcmp(a, "\"\"")) {
      size_t i;
      for(i=0;i<strlen(a);i++) {
        if (!(isalnum((unsigned char) a[i]) || a[i] == '+' || a[i] == '_' ||
            a[i] == '-' || a[i] == '.' || a[i] == '/' || a[i] == '@')) {
          strlcpy(b, a+i, sizeof(b));
          a[i] = '\\';
          a[i+1] = 0;
          strlcat(a, b, sizeof(a));
          i++;
        }
      }
    }

    if (arg != NULL)
      strlcat (args, a, sizeof(args));

    arg = strsep(&margs, " ");

    if (arg) {
      strlcat (args, " ", sizeof (args));
    }
  }
  free (mmargs);

  LOGDEBUG ("Opening pipe to LDA: %s", args);
  file = popen (args, "w");
  if (file == NULL)
  {
    LOG(LOG_ERR, ERR_LDA_OPEN, args, strerror (errno));
    return EFILE;
  }

  /* Manage local delivery agent failures */

  fputs (message, file);
  rc = pclose (file);
  if (rc == -1) {
    LOG(LOG_WARNING, ERR_LDA_STATUS, args, strerror (errno));
    return EFILE;
  } else if (WIFEXITED (rc)) {
    int lda_exit_code;
    lda_exit_code = WEXITSTATUS (rc);
    if (lda_exit_code == 0) {
      LOGDEBUG ("LDA returned success");
    } else {
      LOG(LOG_ERR, ERR_LDA_EXIT, lda_exit_code, args);
      if (_ds_match_attribute(agent_config, "LMTPLDAErrorsPermanent", "on"))
        return EINVAL;
      else
        return lda_exit_code;
    }
  }
#ifndef _WIN32
  else if (WIFSIGNALED (rc))
  {
    int sig;
    sig = WTERMSIG (rc);
    LOG(LOG_ERR, ERR_LDA_SIGNAL, sig, args);
    return sig;
  }
  else
  {
    LOG(LOG_ERR, ERR_LDA_CLOSE, rc);
    return rc;
  }
#endif

  return 0;
}

/*
 * tag_message(AGENT_CTX *ATX, ds_message_t message)
 *
 * DESCRIPTION
 *   Tags a message's subject line as spam using spamSubject
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   message      Message structure (ds_message_t) to tag
 *
 * RETURN VALUES
 *   returns 0 on success
 *   EINVAL on permanent failure
 *   EFAILURE on temporary failure
 *   EFILE local agent failure
 */

int tag_message(AGENT_CTX *ATX, ds_message_t message)
{
  ds_message_part_t block = message->components->first->ptr;
  struct nt_node *node_header = block->headers->first;
  int tagged = 0;
  char spam_subject[16];

  strcpy(spam_subject, "[SPAM]");
  if (_ds_pref_val(ATX->PTX, "spamSubject")[0] != '\n' &&
      _ds_pref_val(ATX->PTX, "spamSubject")[0] != 0)
  {
    strlcpy(spam_subject, _ds_pref_val(ATX->PTX, "spamSubject"),
            sizeof(spam_subject));
  }

  /* Only scan the first (primary) header of the message. */

  while (node_header != NULL)
  {
    ds_header_t head;

    head = (ds_header_t) node_header->ptr;
    if (head->heading && !strcasecmp(head->heading, "Subject"))
    {

      /* CURRENT HEADER: Is this header already tagged? */

      if (strncmp(head->data, spam_subject, strlen(spam_subject)))
      {
        /* Not tagged, so tag it */
        long subject_length = strlen(head->data)+strlen(spam_subject)+2;
        char *subject = malloc(subject_length);
        if (subject != NULL) {
          snprintf(subject,
                   subject_length, "%s %s",
                   spam_subject,
                   head->data);
          free(head->data);
          head->data = subject;
        }
      }

      /* ORIGINAL HEADER: Is this header already tagged? */

      if (head->original_data != NULL &&
          strncmp(head->original_data, spam_subject, strlen(spam_subject)))
      {
        /* Not tagged => tag it. */
        long subject_length = strlen(head->original_data)+strlen(spam_subject)+2;
        char *subject = malloc(subject_length);
        if (subject != NULL) {
          snprintf(subject,
                   subject_length, "%s %s",
                   spam_subject,
                   head->original_data);
          free(head->original_data);
          head->original_data = subject;
        }
      }

      tagged = 1;
    }
    node_header = node_header->next;
  }

  /* There doesn't seem to be a subject field, so make one */

  if (!tagged)
  {
    char text[80];
    ds_header_t header;
    snprintf(text, sizeof(text), "Subject: %s", spam_subject);
    header = _ds_create_header_field(text);
    if (header != NULL)
    {
#ifdef VERBOSE
      LOGDEBUG("appending header %s: %s", header->heading, header->data);
#endif
      nt_add(block->headers, (void *) header);
    }
  }

  return 0;
}

/*
 * quarantine_message(AGENT_CTX *ATX, const char *message,
 *                    const char *username)
 *
 * DESCRIPTION
 *   Quarantine a message using DSPAM's internal quarantine function
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   message      Text message to quarantine
 *   username     Destination user
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int
quarantine_message (AGENT_CTX *ATX, const char *message, const char *username)
{
  char filename[MAX_FILENAME_LENGTH];
  char *x, *msg;
  int line = 1, i;
  FILE *file;

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"),
                   LOOKUP(ATX->PTX, username), "mbox");
  _ds_prepare_path_for(filename);
  file = fopen (filename, "a");
  if (file == NULL)
  {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror (errno));
    return EFILE;
  }

  i = _ds_get_fcntl_lock(fileno(file));
  if (i) {
    LOG(LOG_WARNING, ERR_IO_LOCK, filename, i, strerror(errno));
    return EFILE;
  }

  /* Write our own "From " header if the MTA didn't give us one. This
   * allows for the viewing of a mailbox from elm or some other local
   * client that would otherwise believe the mailbox is corrupt.
   */

  if (strncmp (message, "From ", 5))
  {
    char head[128];
    time_t tm = time (NULL);

    snprintf (head, sizeof (head), "From QUARANTINE %s", ctime (&tm));
    fputs (head, file);
  }

  msg = strdup(message);

  if (msg == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return EUNKNOWN;
  }

  /* TODO: Is there a way to do this without a strdup/strsep ? */

  x = strsep (&msg, "\n");
  while (x)
  {
    /* Quote any lines beginning with 'From ' to keep mbox from breaking */

    if (!strncmp (x, "From ", 5) && line != 1)
      fputs (">", file);
    fputs (x, file);
    fputs ("\n", file);
    line++;
    x = strsep (&msg, "\n");
  }
  free (msg);
  fputs ("\n\n", file);

  _ds_free_fcntl_lock(fileno(file));
  fclose (file);

  return 0;
}

/*
 * write_web_stats(AGENT_CTX *ATX, const char *username, const char *group,
 *                 struct _ds_spam_totals *totals)
 *
 * DESCRIPTION
 *   Writes a .stats file in the user's data directory for use with web UI
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   username     Destination user
 *   group        Group membership
 *   totals       Pointer to processing totals
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int
write_web_stats (
  AGENT_CTX *ATX,
  const char *username,
  const char *group,
  struct _ds_spam_totals *totals)
{
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;

  if (!totals)
    return EINVAL;

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"),
                   LOOKUP(ATX->PTX, username), "stats");
  _ds_prepare_path_for (filename);
  file = fopen (filename, "w");
  if (file == NULL) {
    LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror (errno));
    return EFILE;
  }

  fprintf (file, "%ld,%ld,%ld,%ld,%ld,%ld\n",
           MAX(0, (totals->spam_learned + totals->spam_classified) -
             (totals->spam_misclassified + totals->spam_corpusfed)),
           MAX(0, (totals->innocent_learned + totals->innocent_classified) -
             (totals->innocent_misclassified + totals->innocent_corpusfed)),
           totals->spam_misclassified, totals->innocent_misclassified,
           totals->spam_corpusfed, totals->innocent_corpusfed);

  if (group)
    fprintf(file, "%s\n", group);

  fclose (file);
  return 0;
}

/*
 * inoculate_user(AGENT_CTX *ATX, const char *username,
 *                struct _ds_spam_signature *SIG, const char *message)
 *
 * DESCRIPTION
 *   Provide a vaccination for the spam processed to the target user
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   username     Target user
 *   SIG          Signature (if providing signature-based inoculation)
 *   message      Text Message (if providing message-based inoculation)
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int
inoculate_user (
  AGENT_CTX *ATX,
  const char *username,
  struct _ds_spam_signature *SIG,
  const char *message)
{
  DSPAM_CTX *INOC;
  int do_inoc = 1, result = 0;
  int f_all = 0;

  LOGDEBUG ("checking if user %s requires this inoculation", username);
  if (user_classify(ATX, username, SIG, message) == DSR_ISSPAM) {
    do_inoc = 0;
  }

  if (!do_inoc)
  {
    LOGDEBUG ("skipping user %s: doesn't require inoculation", username);
    return EFAILURE;
  }
  else
  {
    LOGDEBUG ("inoculating user %s", username);

    if (ATX->flags & DAF_NOISE)
      f_all |= DSF_NOISE;

    if (ATX->PTX != NULL &&
        strcmp(_ds_pref_val(ATX->PTX, "processorBias"), ""))
    {
      if (!strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "on"))
        f_all |= DSF_BIAS;
    } else {
      if (_ds_match_attribute(agent_config, "ProcessorBias", "on"))
        f_all |= DSF_BIAS;
    }

    INOC = dspam_create (username,
                       NULL,
                       _ds_read_attribute(agent_config, "Home"),
                       DSM_PROCESS,
                       f_all);
    if (INOC)
    {
      set_libdspam_attributes(INOC);
      if (attach_context(INOC, ATX->dbh)) {
        LOG (LOG_WARNING, ERR_CORE_ATTACH);
        dspam_destroy(INOC);
        return EUNKNOWN;
      }

      INOC->classification = DSR_ISSPAM;
      INOC->source = DSS_INOCULATION;
      if (SIG)
      {
        INOC->flags |= DSF_SIGNATURE;
        INOC->signature = SIG;
        result = dspam_process (INOC, NULL);
      }
      else
      {
        result = dspam_process (INOC, message);
      }

      if (SIG)
        INOC->signature = NULL;
      dspam_destroy (INOC);
    }
  }

  return result;
}

/*
 * user_classify(AGENT_CTX *ATX, const char *username,
 *               struct _ds_spam_signature *SIG, const char *message)
 *
 * DESCRIPTION
 *   Determine the classification of a message for another user
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   username     Target user
 *   SIG          Signature (if performing signature-based classification)
 *   message      Text Message (if performing message-based ciassification)
 *
 * RETURN VALUES
 *   returns DSR_ value, standard errors on failure
 */

int
user_classify (
  AGENT_CTX *ATX,
  const char *username,
  struct _ds_spam_signature *SIG,
  const char *message)
{
  DSPAM_CTX *CLX;
  int result = 0;
  int f_all = 0;

  if (SIG == NULL && message == NULL) {
    LOG(LOG_WARNING, "user_classify(): SIG == NULL, message == NULL");
    return EINVAL;
  }

  if (ATX->flags & DAF_NOISE)
    f_all |= DSF_NOISE;

  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "")) {
    if (!strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "on"))
      f_all |= DSF_BIAS;
  } else {
    if (_ds_match_attribute(agent_config, "ProcessorBias", "on"))
      f_all |= DSF_BIAS;
  }

  /* First see if the user needs to be inoculated */
  CLX = dspam_create (username,
                    NULL,
                    _ds_read_attribute(agent_config, "Home"),
                    DSM_CLASSIFY,
                    f_all);
  if (CLX)
  {
    set_libdspam_attributes(CLX);
    if (attach_context(CLX, ATX->dbh)) {
      LOG (LOG_WARNING, ERR_CORE_ATTACH);
      dspam_destroy(CLX);
      return EUNKNOWN;
    }

    if (SIG)
    {
      CLX->flags |= DSF_SIGNATURE;
      CLX->signature = SIG;
      result = dspam_process (CLX, NULL);
    }
    else
    {
      if (message == NULL) {
        LOG(LOG_WARNING, "user_classify: SIG = %ld, message = NULL\n", (unsigned long) SIG);
        if (SIG) CLX->signature = NULL;
        dspam_destroy (CLX);
        return EFAILURE;
      }
      result = dspam_process (CLX, message);
    }

    if (SIG)
      CLX->signature = NULL;

    if (result)
    {
      LOGDEBUG ("user_classify() returned error %d", result);
      result = EFAILURE;
    }
    else
      result = CLX->result;

    dspam_destroy (CLX);
  }

  return result;
}

/*
 * send_notice(AGENT_CTX *ATX, const char *filename, const char *mailer_args,
 *             const char *username)
 *
 * DESCRIPTION
 *   Sends a canned notice to the destination user
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   filename     Filename of canned notice
 *   mailer_args  Local agent arguments
 *   username     Destination user
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int send_notice(
  AGENT_CTX *ATX,
  const char *filename,
  const char *mailer_args,
  const char *username)
{
  FILE *f;
  char msgfile[MAX_FILENAME_LENGTH];
  buffer *b;
  char buf[1024];
  time_t now;
  int ret;

  time(&now);

  if (_ds_read_attribute(agent_config, "TxtDirectory")) {
    snprintf(msgfile, sizeof(msgfile), "%s/%s", _ds_read_attribute(agent_config, "TxtDirectory"), filename);
  } else {
    snprintf(msgfile, sizeof(msgfile), "%s/txt/%s", _ds_read_attribute(agent_config, "Home"), filename);
  }
  f = fopen(msgfile, "r");
  if (!f) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, filename, strerror(errno));
    return EFILE;
  }

  b = buffer_create(NULL);
  if (!b) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    fclose(f);
    return EUNKNOWN;
  }

  strftime(buf,sizeof(buf), "Date: %a, %d %b %Y %H:%M:%S %z\n",
     localtime(&now));
  buffer_cat(b, buf);

  while(fgets(buf, sizeof(buf), f)!=NULL) {
    char *s = buf;
    char *w = strstr(buf, "$u");
    while(w != NULL) {
      w[0] = 0;
      buffer_cat(b, s);
      buffer_cat(b, username);
        s = w+2;
        w = strstr(s, "$u");
    }
    buffer_cat(b, s);
  }
  fclose(f);
  ret = deliver_message(ATX, b->data, mailer_args, username,
                        stdout, DSR_ISINNOCENT);

  buffer_destroy(b);

  return ret;
}

/*
 * process_users(AGENT_CTX *ATX, buffer *message)
 *
 * DESCRIPTION
 *   Primary processing loop: cycle through all destination users and process
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   message      Buffer structure containing text message
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int process_users(AGENT_CTX *ATX, buffer *message) {
  int i = 0, have_rcpts = 0, return_code = 0, retcode = 0;
  struct nt_node *node_nt;
  struct nt_node *node_rcpt = NULL;
  struct nt_c c_nt, c_rcpt;
  buffer *parse_message;
  agent_result_t presult = NULL;
  char *plus, *atsign;
  char mailbox[256];
  FILE *fout;

  if (ATX->sockfd) {
    fout = ATX->sockfd;
  } else {
    fout = stdout;
  }

  node_nt = c_nt_first (ATX->users, &c_nt);
  if (ATX->recipients) {
    node_rcpt = c_nt_first (ATX->recipients, &c_rcpt);
    have_rcpts = ATX->recipients->items;
  }

  /* Keep going as long as we have destination users */

  while (node_nt || node_rcpt)
  {
    struct stat s;
    char filename[MAX_FILENAME_LENGTH];
    int optin, optout;
    char *username = NULL;

    /* If ServerParameters specifies a --user, there will only be one
     * instance on the stack, but possible multiple recipients. So we
     * need to recycle.
     */

    if (node_nt == NULL)
      node_nt = ATX->users->first;

    /* Set the "current recipient" to either the next item on the rcpt stack
     * or the current user if not present.
     */


#ifdef EXT_LOOKUP
	verified_user = 0;
	if (_ds_match_attribute(agent_config, "ExtLookup", "on")) {
		LOGDEBUG ("looking up user %s using %s driver.", node_nt->ptr, _ds_read_attribute(agent_config, "ExtLookupDriver"));
		username = external_lookup(agent_config, node_nt->ptr, username);
		if (username != NULL) {
			LOGDEBUG ("external lookup verified user %s", node_nt->ptr);
			verified_user = 1;
			if (_ds_match_attribute(agent_config, "ExtLookupMode", "map") ||
					_ds_match_attribute(agent_config, "ExtLookupMode", "strict")) {
						LOGDEBUG ("mapping address %s to uid %s", node_nt->ptr, username);
						node_nt->ptr = username;
			}
		} else if (_ds_match_attribute(agent_config, "ExtLookupMode", "map")) {
				LOGDEBUG ("no match for user %s but mode is %s. continuing...", node_nt->ptr, _ds_read_attribute(agent_config, "ExtLookupMode"));
				verified_user = 1;
		}
	} else {
		verified_user = 1;
	}
#endif
	username = node_nt->ptr;

    if (node_rcpt) {
      ATX->recipient = node_rcpt->ptr;
      node_rcpt = c_nt_next (ATX->recipients, &c_rcpt);
    } else {

      /* We started out using the recipients list and it's exhausted, so quit */
      if (have_rcpts)
        break;

      ATX->recipient = node_nt->ptr;
    }

      /* If support for "+detail" is enabled, save full mailbox name for
         delivery and strip detail for processing */

    if (_ds_match_attribute(agent_config, "EnablePlusedDetail", "on")) {
      char plused_char = '+';
      if (_ds_read_attribute(agent_config, "PlusedCharacter"))
        plused_char = _ds_read_attribute(agent_config, "PlusedCharacter")[0];
      strlcpy(mailbox, username, sizeof(mailbox));
      ATX->recipient = mailbox;
      if (_ds_match_attribute(agent_config, "PlusedUserLowercase", "on"))
        lc (username, username);
      plus = index(username, plused_char);
      if (plus) {
        atsign = index(plus, '@');
        if (atsign)
          strcpy(plus, atsign);
        else
          *plus='\0';
      }
    }

    presult = calloc(1, sizeof(struct agent_result));
    parse_message = buffer_create(message->data);
    if (parse_message == NULL) {
      LOG(LOG_CRIT, ERR_MEM_ALLOC);
      presult->exitcode = ERC_PROCESS;
      strcpy(presult->text, ERR_MEM_ALLOC);

      if (ATX->results) {
        nt_add(ATX->results, presult);
        if (ATX->results->nodetype == NT_CHAR)
          free(presult);
      } else
        free(presult);

      presult = NULL;

      continue;
    }

    /* Determine whether to activate debug. If we're running in daemon mode,
     * debug is either on or off (it's a global variable), so this only
     * applies to running in client or local processing mode.
     */

#ifdef DEBUG
    if (!DO_DEBUG &&
        (_ds_match_attribute(agent_config, "Debug", "*")           ||
         _ds_match_attribute(agent_config, "Debug", node_nt->ptr)))
    {
      // No DebugOpt specified; turn it on for everything
      if (!_ds_read_attribute(agent_config, "DebugOpt"))
      {
        DO_DEBUG = 1;
      }
      else {
        if (_ds_match_attribute(agent_config, "DebugOpt", "process") &&
            ATX->source == DSS_NONE &&
            ATX->operating_mode == DSM_PROCESS)
        {
          DO_DEBUG = 1;
        }

        if (_ds_match_attribute(agent_config, "DebugOpt", "classify") &&
            ATX->operating_mode == DSM_CLASSIFY)
        {
          DO_DEBUG = 1;
        }

        if (_ds_match_attribute(agent_config, "DebugOpt", "spam") &&
            ATX->classification == DSR_ISSPAM &&
            ATX->source == DSS_ERROR)
        {
          DO_DEBUG = 1;
        }

        if (_ds_match_attribute(agent_config, "DebugOpt", "fp") &&
            ATX->classification == DSR_ISINNOCENT &&
            ATX->source == DSS_ERROR)
        {
          DO_DEBUG = 1;
        }

        if (_ds_match_attribute(agent_config, "DebugOpt", "inoculation") &&
            ATX->source == DSS_INOCULATION)
        {
          DO_DEBUG = 1;
        }

        if (_ds_match_attribute(agent_config, "DebugOpt", "corpus") &&
            ATX->source == DSS_CORPUS)
        {
          DO_DEBUG = 1;
        }
      }
    }

    ATX->status[0] = 0;

    if (DO_DEBUG) {
      LOGDEBUG ("DSPAM Instance Startup");
      LOGDEBUG ("input args: %s", ATX->debug_args);
      LOGDEBUG ("pass-thru args: %s", ATX->mailer_args);
      LOGDEBUG ("processing user %s", (const char *) node_nt->ptr);
      LOGDEBUG ("uid = %d, euid = %d, gid = %d, egid = %d",
                getuid(), geteuid(), getgid(), getegid());

      /* Write message to dspam.messags */
      {
        FILE *f;
        char m[MAX_FILENAME_LENGTH];
        snprintf (m, sizeof (m), "%s/dspam.messages", LOGDIR);
        f = fopen (m, "a");
        if (f != NULL)
        {
          fprintf (f, "%s\n", parse_message->data);
          fclose (f);
        }
      }
    }
#endif

    /*
     * Determine if the user is opted in or out
     */

    ATX->PTX = load_aggregated_prefs(ATX, username);
    if (!strcmp(_ds_pref_val(ATX->PTX, "fallbackDomain"), "on")) {
      if (username != NULL && strchr(username, '@')) {
        char *domain = strchr(username, '@');
        username = domain;
      } else {
        LOG(LOG_ERR, "process_users(): Can not fallback to domains for username '%s' without @domain part.", username);
      }
    }

    ATX->train_pristine = 0;
    if ((_ds_match_attribute(agent_config, "TrainPristine", "on") ||
        !strcmp(_ds_pref_val(ATX->PTX, "trainPristine"), "on")) &&
        strcmp(_ds_pref_val(ATX->PTX, "trainPristine"), "off")) {
            ATX->train_pristine = 1;
    }

    _ds_userdir_path(filename,
                     _ds_read_attribute(agent_config, "Home"),
                     LOOKUP(ATX->PTX, username), "dspam");
    optin = stat(filename, &s);

#ifdef HOMEDIR
    if (!optin && (!S_ISDIR(s.st_mode))) {
      optin = -1;
      LOG(LOG_WARNING, ERR_AGENT_OPTIN_DIR, filename);
    }
#endif

    _ds_userdir_path(filename,
                     _ds_read_attribute(agent_config, "Home"),
                     LOOKUP(ATX->PTX, username), "nodspam");
    optout = stat(filename, &s);

    /* If the message is too big to process, just deliver it */

    if (_ds_read_attribute(agent_config, "MaxMessageSize")) {
      if (parse_message->used >
          atoi(_ds_read_attribute(agent_config, "MaxMessageSize")))
      {
        LOG (LOG_INFO, "message too big, delivering");
        optout = 0;
      }
    }

    /* Deliver the message if the user has opted not to be filtered */

    optout = (optout) ? 0 : 1;
    optin = (optin) ? 0 : 1;

    if    /* opted out implicitly */
          (optout || !strcmp(_ds_pref_val(ATX->PTX, "optOut"), "on") ||

          /* not opted in (in an opt-in system) */
          (_ds_match_attribute(agent_config, "Opt", "in") &&
          !optin && strcmp(_ds_pref_val(ATX->PTX, "optIn"), "on")))
    {
      if (ATX->flags & DAF_DELIVER_INNOCENT)
      {
        retcode =
          deliver_message (ATX, parse_message->data,
                           (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
                            node_nt->ptr, fout, DSR_ISINNOCENT);
        if (retcode)
          presult->exitcode = ERC_DELIVERY;
	if (retcode == EINVAL)
          presult->exitcode = ERC_PERMANENT_DELIVERY;
        strlcpy(presult->text, ATX->status, sizeof(presult->text));

        if (ATX->sockfd && ATX->flags & DAF_STDOUT)
          ATX->sockfd_output = 1;
      }
    }

    /* Call process_message(), then handle result appropriately */

    else
    {
      char *result_string = NULL;
      int result;
      result = process_message (ATX, parse_message, username, &result_string);
      presult->classification = result;

#ifdef CLAMAV
      if (result_string && !strcmp(result_string, LANG_CLASS_VIRUS)) {
        if (_ds_match_attribute(agent_config, "ClamAVResponse", "reject")) {
          presult->classification = DSR_ISSPAM;
          presult->exitcode = ERC_PERMANENT_DELIVERY;
          strlcpy(presult->text, ATX->status, sizeof(presult->text));
          free(result_string);
          result_string = NULL;
          goto RSET;
        }
        else if (_ds_match_attribute(agent_config, "ClamAVResponse", "spam"))
        {
          presult->classification = DSR_ISSPAM;
          presult->exitcode = ERC_SUCCESS;
          result = DSR_ISSPAM;
          strlcpy(presult->text, ATX->status, sizeof(presult->text));
        } else {
          presult->classification = DSR_ISINNOCENT;
          presult->exitcode = ERC_SUCCESS;
          free(result_string);
          result_string = NULL;
          goto RSET;
        }
      }
#endif
      free(result_string);
      result_string = NULL;

      /* Exit code 99 for spam (when using broken return codes) */

      if (_ds_match_attribute(agent_config, "Broken", "returnCodes")) {
        if (result == DSR_ISSPAM)
          return_code = 99;
      }

      /*
       * Classify Only
       */

      if (ATX->operating_mode == DSM_CLASSIFY)
      {
        node_nt = c_nt_next (ATX->users, &c_nt);
        _ds_pref_free(ATX->PTX);
        free(ATX->PTX);
        ATX->PTX = NULL;
        buffer_destroy(parse_message);
        free(presult);
        presult = NULL;
        i++;
        continue;
      }

      /*
       * Classify and Process
       */

      /* Innocent */

      if (result != DSR_ISSPAM)
      {
        int deliver = 1;

        /* Processing Error */

        if (result != DSR_ISINNOCENT) {
          if (ATX->classification != DSR_NONE) {
            deliver = 0;
            LOG (LOG_WARNING,
                 "process_message returned error %d.  dropping message.", result);
          } else if (ATX->classification == DSR_NONE) {
            LOG (LOG_WARNING,
                 "process_message returned error %d.  delivering.", result);
          }
        }

        /* Deliver */

        if (deliver && ATX->flags & DAF_DELIVER_INNOCENT) {
          LOGDEBUG ("delivering message");
          retcode = deliver_message
            (ATX, parse_message->data,
             (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
             node_nt->ptr, fout, DSR_ISINNOCENT);

          if (ATX->sockfd && ATX->flags & DAF_STDOUT)
            ATX->sockfd_output = 1;
          if (retcode) {
            presult->exitcode = ERC_DELIVERY;
          if (retcode == EINVAL)
            presult->exitcode = ERC_PERMANENT_DELIVERY;
          strlcpy(presult->text, ATX->status, sizeof(presult->text));

            if (result == DSR_ISINNOCENT &&
                _ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                ATX->learned)
            {
              ATX->classification = result;
              ATX->source = DSS_ERROR;
              ATX->flags |= DAF_UNLEARN;
              process_message (ATX, parse_message, username, NULL);
            }

          }
        }
      }

      /* Spam */

      else
      {
        /* Do not Deliver Spam */

        if (! (ATX->flags & DAF_DELIVER_SPAM))
        {
          retcode = 0;

          /* If a specific quarantine has been configured, use it */

          if (ATX->source != DSS_CORPUS) {
            if (ATX->spam_args[0] != 0 ||
                 (ATX->PTX != NULL &&
                   ( !strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "tag") ||
                     !strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "deliver") )
                 )
               )
            {
              if (ATX->classification == DSR_NONE) {
                if (ATX->spam_args[0] != 0) {
                  retcode = deliver_message
                    (ATX, parse_message->data,
                     (ATX->flags & DAF_STDOUT) ? NULL : ATX->spam_args,
                     node_nt->ptr, fout, DSR_ISSPAM);
                  if (ATX->sockfd && ATX->flags & DAF_STDOUT)
                    ATX->sockfd_output = 1;
                } else {
                  retcode = deliver_message
                    (ATX, parse_message->data,
                     (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
                     node_nt->ptr, fout, DSR_ISSPAM);
                  if (ATX->sockfd && ATX->flags & DAF_STDOUT)
                    ATX->sockfd_output = 1;
                }

                if (retcode)
                  presult->exitcode = ERC_DELIVERY;
                if (retcode == EINVAL)
                  presult->exitcode = ERC_PERMANENT_DELIVERY;
                strlcpy(presult->text, ATX->status, sizeof(presult->text));
              }
            }
            else
            {
              /* Use standard quarantine procedure */
              if (ATX->source == DSS_INOCULATION ||
                  ATX->classification == DSR_NONE)
              {
                if (ATX->flags & DAF_SUMMARY) {
                  retcode = 0;
                } else {
                  if (ATX->managed_group[0] == 0)
                    retcode =
                      quarantine_message (ATX, parse_message->data, username);
                  else
                    retcode =
                      quarantine_message (ATX, parse_message->data,
                                          ATX->managed_group);
                }
              }
            }

            if (retcode) {
              presult->exitcode = ERC_DELIVERY;
            if (retcode == EINVAL)
              presult->exitcode = ERC_PERMANENT_DELIVERY;
            strlcpy(presult->text, ATX->status, sizeof(presult->text));


              /* Unlearn the message on a local delivery failure */
              if (_ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                  ATX->learned) {
                ATX->classification = result;
                ATX->source = DSS_ERROR;
                ATX->flags |= DAF_UNLEARN;
                process_message (ATX, parse_message, username, NULL);
              }
            }
          }
        }

        /* Deliver Spam */

        else
        {
          if (ATX->sockfd && ATX->flags & DAF_STDOUT)
            ATX->sockfd_output = 1;
          retcode = deliver_message
            (ATX, parse_message->data,
             (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
             node_nt->ptr, fout, DSR_ISSPAM);

          if (retcode) {
            presult->exitcode = ERC_DELIVERY;
          if (retcode == EINVAL)
            presult->exitcode = ERC_PERMANENT_DELIVERY;
          strlcpy(presult->text, ATX->status, sizeof(presult->text));


            if (_ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                ATX->learned) {
              ATX->classification = result;
              ATX->source = DSS_ERROR;
              ATX->flags |= DAF_UNLEARN;
              process_message (ATX, parse_message, username, NULL);
            }
          }
        }
      }
    }

#ifdef CLAMAV
RSET:
#endif
    _ds_pref_free(ATX->PTX);
    free(ATX->PTX);
    ATX->PTX = NULL;
    node_nt = c_nt_next (ATX->users, &c_nt);

    if (ATX->results) {
      nt_add(ATX->results, presult);
      if (ATX->results->nodetype == NT_CHAR)
        free(presult);
    } else
      free(presult);
    presult = NULL;
    LOGDEBUG ("DSPAM Instance Shutdown.  Exit Code: %d", return_code);
    buffer_destroy(parse_message);
  }

  if (presult) free(presult);
  return return_code;
}
// break
// load_agg
// continue
// return

/*
 * find_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Find and parse DSPAM signature
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing message and parameters
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 1 (and sets CTX->signature) if found
 *
 */

int find_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  struct nt_node *node_nt;
  struct nt_c c, c2;
  ds_message_part_t block = NULL;
  char first_boundary[512];
  int is_signed = 0, i = 0;
  char *signature_begin = NULL, *signature_end, *erase_begin;
  int signature_length, have_signature = 0;
  struct nt_node *node_header;

  first_boundary[0] = 0;

  if (ATX->signature[0] != 0)
    return 1;

  /* Iterate through each message component in search of a signature
   * and decode components as necessary
   */

  node_nt = c_nt_first (CTX->message->components, &c);
  while (node_nt != NULL)
  {
    block = (ds_message_part_t) node_nt->ptr;

    if (block->media_type == MT_MULTIPART && block->media_subtype == MST_SIGNED)
      is_signed = 1;

    if (!strcmp(_ds_pref_val(ATX->PTX, "signatureLocation"), "headers"))
      is_signed = 2;

#ifdef VERBOSE
      LOGDEBUG ("scanning component %d for a DSPAM signature", i);
#endif

    if (block->media_type == MT_TEXT
        || block->media_type == MT_MESSAGE
        || block->media_type == MT_UNKNOWN
        || (!i && block->media_type == MT_MULTIPART))
    {
      char *body;

      /* Verbose output of each message component */

#ifdef VERBOSE
      if (DO_DEBUG) {
        if (block->boundary != NULL)
        {
          LOGDEBUG ("  : Boundary     : %s", block->boundary);
        }
        if (block->terminating_boundary != NULL)
          LOGDEBUG ("  : Term Boundary: %s", block->terminating_boundary);
        LOGDEBUG ("  : Encoding     : %d", block->encoding);
        LOGDEBUG ("  : Media Type   : %d", block->media_type);
        LOGDEBUG ("  : Media Subtype: %d", block->media_subtype);
        LOGDEBUG ("  : Headers:");
        node_header = c_nt_first (block->headers, &c2);
        while (node_header != NULL)
        {
          ds_header_t header =
            (ds_header_t) node_header->ptr;
          LOGDEBUG ("    %-32s  %s", header->heading, header->data);
          node_header = c_nt_next (block->headers, &c2);
        }
      }
#endif

      body = block->body->data;
      if (block->encoding == EN_BASE64
          || block->encoding == EN_QUOTED_PRINTABLE)
      {
        if (block->content_disposition != PCD_ATTACHMENT)
        {
#ifdef VERBOSE
          LOGDEBUG ("decoding message block from encoding type %d",
                    block->encoding);
#endif

          body = _ds_decode_block (block);

          if (is_signed)
          {
            LOGDEBUG
              ("message is signed.  retaining original text for reassembly");
            block->original_signed_body = block->body;
          }
          else
          {
            block->encoding = EN_8BIT;

            node_header = c_nt_first (block->headers, &c2);
            while (node_header != NULL)
            {
              ds_header_t header =
                (ds_header_t) node_header->ptr;
              if (!strcasecmp
                  (header->heading, "Content-Transfer-Encoding"))
              {
                free (header->data);
                header->data = strdup ("8bit");
              }
              node_header = c_nt_next (block->headers, &c2);
            }

            buffer_destroy (block->body);
          }
          block->body = buffer_create (body);
          free (body);

          body = block->body->data;
        }
      }

      if (!strcmp(_ds_pref_val(ATX->PTX, "signatureLocation"), "headers")) {
        if (block->headers != NULL && !have_signature)
        {
          struct nt_node *node_header;
          ds_header_t head;

          node_header = block->headers->first;
          while(node_header != NULL) {
            head = (ds_header_t) node_header->ptr;
            if (head->heading &&
                !strcasecmp(head->heading, "X-DSPAM-Signature")) {
              if (!strncmp(head->data, SIGNATURE_BEGIN,
                           strlen(SIGNATURE_BEGIN)))
              {
                body = head->data;
              }
              else
              {
                strlcpy(ATX->signature, head->data, sizeof(ATX->signature));
                have_signature = 1;
              }
              break;
            }
            node_header = node_header->next;
          }
        }
      }

      if (!ATX->train_pristine &&

        /* Don't keep searching if we've already found the signature in the
         * headers, and we're using signatureLocation=headers
         */
        (!have_signature ||
         strcmp(_ds_pref_val(ATX->PTX, "signatureLocation"), "headers")))
      {
        /* Look for signature */
        if (body != NULL)
        {
          int tight = 1;
          signature_begin = strstr (body, SIGNATURE_BEGIN);
          if (signature_begin == NULL) {
            signature_begin = strstr (body, LOOSE_SIGNATURE_BEGIN);
            tight = 0;
          }

          if (signature_begin)
          {
            erase_begin = signature_begin;
            if (tight)
              signature_begin += strlen(SIGNATURE_BEGIN);
            else {
              char *loose = strstr (signature_begin, SIGNATURE_DELIMITER);
              if (!loose) {
                LOGDEBUG("found loose signature begin, but no delimiter");
                goto NEXT;
              }
              signature_begin = loose + strlen(SIGNATURE_DELIMITER);
            }

            signature_end = signature_begin;

            /* Find the signature's end character */
            while (signature_end != NULL
              && signature_end[0] != 0
              && (isalnum ((int) signature_end[0]) || signature_end[0] == 32 ||
                  signature_end[0] == ','))
            {
              signature_end++;
            }

            if (signature_end != NULL)
            {
              signature_length = signature_end - signature_begin;

              if (signature_length < 128)
              {
                memcpy (ATX->signature, signature_begin, signature_length);
                ATX->signature[signature_length] = 0;

                while(isspace( (int) ATX->signature[0]))
                {
                  memmove(ATX->signature, ATX->signature+1, strlen(ATX->signature));
                }

                if (strcmp(_ds_pref_val(ATX->PTX, "signatureLocation"),
                    "headers")) {

                  if (!is_signed && ATX->classification == DSR_NONE) {
                    memmove(erase_begin, signature_end+1, strlen(signature_end+1)+1);
                    block->body->used = (long) strlen(body);
                  }
                }
                have_signature = 1;
                LOGDEBUG ("found signature '%s'", ATX->signature);
              }
            }
          }
        }
      } /* TrainPristine */
    }
NEXT:
    node_nt = c_nt_next (CTX->message->components, &c);
    i++;
  }

  CTX->message->protect = is_signed;

  return have_signature;
}

/*
 * ctx_init(AGENT_CTX *ATX, const char *username)
 *
 * DESCRIPTION
 *   Initialize a DSPAM context from an agent context
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   username     Destination user
 *
 * RETURN VALUES
 *   pointer to newly allocated DSPAM context, NULL on failure
 *
 */

DSPAM_CTX *ctx_init(AGENT_CTX *ATX, const char *username) {
  /* We NEED a username. Without it we can't do much */
  if (username == NULL) {
    LOG (LOG_CRIT, ERR_AGENT_USER_UNDEFINED);
    return NULL;
  }
  DSPAM_CTX *CTX;
  char filename[MAX_FILENAME_LENGTH];
  char ctx_group[128] = { 0 };
  int f_all = 0, f_mode = DSM_PROCESS;
  FILE *file;

  ATX->inoc_users = nt_create (NT_CHAR);
  if (ATX->inoc_users == NULL) {
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  ATX->classify_users = nt_create (NT_CHAR);
  if (ATX->classify_users == NULL)
  {
    nt_destroy(ATX->inoc_users);
    LOG (LOG_CRIT, ERR_MEM_ALLOC);
    return NULL;
  }

  /* Set Group Membership */

  if (ATX->operating_mode == DSM_CLASSIFY) {
    LOGDEBUG ("Group support disabled in classify mode");
  } else if (!strcmp(_ds_pref_val(ATX->PTX, "ignoreGroups"), "on")) {
    LOGDEBUG ("Ignoring groups due preference ignoreGroups on");
  } else if (ATX->operating_mode == DSM_PROCESS) {
    snprintf (filename, sizeof (filename), "%s",
              _ds_read_attribute(agent_config, "GroupConfig"));
    file = fopen (filename, "r");
    if (file != NULL)
    {
      int is_group_member_inoculation = 0;
      int is_group_member_classification = 0;
      int is_group_member_global = 0;
      int is_group_member_shared = 0;
      int is_group_member_merged = 0;
      char *group;
      char buffer[10240];

      while (fgets (buffer, sizeof (buffer), file) != NULL)
      {
        int do_inocgroups = 0;
        int do_classgroups = 0;
        char *type, *list, *listentry;
        chomp (buffer);

        if (buffer[0] == 0 || buffer[0] == '#' || buffer[0] == ';')
          continue;

        list = strdup (buffer);
        listentry = strdup (buffer);
        group = strtok (buffer, ":");

        if (group != NULL)
        {
          type = strtok (NULL, ":");
          if (!type)
            continue;

          /* Check if user is member of inoculation group */
          if (strcasecmp (type, "INOCULATION") == 0 &&
              ATX->classification == DSR_ISSPAM &&
              ATX->source != DSS_CORPUS)
          {
            if (is_group_member_shared == 1) {
              LOGDEBUG ("skipping innoculation group %s: user %s is already in a shared group", group, username);
              /* Process next entry in group file */
              continue;
            } else {
              char *l = list, *u;
              strsep (&l, ":");
              strsep (&l, ":");
              u = strsep (&l, ",");
              while (u != NULL) {
                if (strcasecmp(u,username) == 0) {
                  LOGDEBUG ("user %s is member of inoculation group %s", username, group);
                  is_group_member_inoculation = 1;
                  do_inocgroups = 1;
                  break;
                }
                u = strsep (&l, ",");
              }
            }
          }
          /* Check if user is member of classification group */
          else if (strcasecmp (type, "CLASSIFICATION") == 0)
          {
            if (is_group_member_shared == 1) {
              LOGDEBUG ("skipping classification or global group %s: user %s is already in a shared group", group, username);
              /* Process next entry in group file */
              continue;
            } else {
              char *l = list, *u;
              strsep (&l, ":");
              strsep (&l, ":");
              u = strsep (&l, ",");
              while (u != NULL) {
                if (u[0] == '*' && strcmp(u,"*") != 0) {
                  if (is_group_member_classification == 1) {
                    LOGDEBUG ("skipping global group %s: user %s is already in a classification group", group, username);
                    break;
                  }
                  LOGDEBUG ("user %s is member of global group %s", username, group);
                  is_group_member_global = 1;
                  do_classgroups = 1;
                  break;
                } else if (strcasecmp(u,username) == 0) {
                  if (is_group_member_global == 1) {
                    LOGDEBUG ("skipping classification group %s: user %s is already in a global group", group, username);
                    break;
                  }
                  LOGDEBUG ("user %s is member of classification group %s", username, group);
                  is_group_member_classification = 1;
                  do_classgroups = 1;
                  break;
                }
                u = strsep (&l, ",");
              }
            }
          }
          /* Process shared and shared,managed group */
          else if (strncasecmp (type, "SHARED", 6) == 0)
          {
            if (is_group_member_shared == 1) {
              LOGDEBUG ("skipping shared group %s: user %s is already in a shared group", group, username);
              /* Process next entry in group file */
              continue;
            } else if (is_group_member_merged == 1) {
              LOGDEBUG ("skipping shared group %s: user %s is already in a merged group", group, username);
              /* Process next entry in group file */
              continue;
            } else if (is_group_member_inoculation == 1) {
              LOGDEBUG ("skipping shared group %s: user %s is already in a inoculation group", group, username);
              /* Process next entry in group file */
              continue;
            } else if (is_group_member_classification == 1) {
              LOGDEBUG ("skipping shared group %s: user %s is already in a classification group", group, username);
              /* Process next entry in group file */
              continue;
            } else if (is_group_member_global == 1) {
              LOGDEBUG ("skipping shared group %s: user %s is already in a global group", group, username);
              /* Process next entry in group file */
              continue;
            } else {
              char *l = list, *u;
              strsep (&l, ":");
              strsep (&l, ":");
              u = strsep (&l, ",");
              while (u != NULL) {
                if (strcasecmp(u,username) == 0 ||
                    strcmp(u,"*") == 0 ||
                    (strncmp(u,"*@",2) == 0 && strchr(username,'@') != NULL && strcasecmp(u+1,strchr(username,'@')) == 0))
                {
                  LOGDEBUG ("assigning user %s to shared group %s", username, group);
                  strlcpy (ctx_group, group, sizeof (ctx_group));
                  if (strncasecmp (type + 6, ",MANAGED", 8) == 0) {
                    LOGDEBUG ("shared group is managed by %s", group);
                    strlcpy (ATX->managed_group, ctx_group, sizeof(ATX->managed_group));
                  }
                  is_group_member_shared = 1;
                  break;
                }
                u = strsep (&l, ",");
              }
            }
            /* Process next entry in group file */
            continue;
          }
          /* Process merged group */
          else if (strcasecmp (type, "MERGED") == 0 && strcasecmp(group, username) != 0)
          {
            if (is_group_member_merged == 1) {
              LOGDEBUG ("skipping merged group %s: user %s is already in merged group %s", group, username, ctx_group);
              /* Process next entry in group file */
              continue;
            } else if (is_group_member_shared == 1) {
              LOGDEBUG ("skipping merged group %s: user %s is already in a shared group", group, username);
              /* Process next entry in group file */
              continue;
            } else if (ATX->flags & DAF_MERGED) {
              LOGDEBUG ("BUG in DSPAM. Please report this bug:");
              LOGDEBUG (" --> Skipping merged group %s: user %s is already in merged group %s", group, username, ctx_group);
              /* Process next entry in group file */
              continue;
            } else {
              char *l = list, *u;
              strsep (&l, ":");
              strsep (&l, ":");
              u = strsep (&l, ",");
              while (u != NULL) {
                if (strcasecmp(u,username) == 0 || strcmp(u,"*") == 0 ||
                    (strncmp(u,"*@",2) == 0 && strchr(username,'@') != NULL && strcasecmp(u+1,strchr(username,'@')) == 0))
                {
                  if (is_group_member_merged == 1) {
                    LOGDEBUG ("skipping entry %s for merged group %s. User is already in merged group.", u, group);
                    continue;
                  } else {
                    LOGDEBUG ("adding user to merged group %s", group);
                    ATX->flags |= DAF_MERGED;
                    strlcpy(ctx_group, group, sizeof(ctx_group));
                    is_group_member_merged = 1;
                  }
                } else if ((strncmp(u,"-",1) == 0 && strcasecmp(u+1,username) == 0) ||
                            (strncmp(u,"-*@",3) == 0 && strchr(username,'@') != NULL && strcasecmp(u+2,strchr(username,'@')) == 0))
                {
                  if (is_group_member_merged == 0) {
                    LOGDEBUG ("skipping entry %s for merged group %s. User is already not in merged group.", u, group);
                    continue;
                  } else {
                    LOGDEBUG ("removing user from merged group %s", group);
                    ATX->flags ^= DAF_MERGED;
                    ctx_group[0] = 0;
                    is_group_member_merged = 0;
                  }
                } else {
                  LOGDEBUG ("unhandled entry %s in merged group %s", u, group);
                }
                u = strsep (&l, ",");
              }
            }
            /* Process next entry in group file */
            continue;
          }

          /*
           * If we are reporting a spam, report it as a spam to all other
           * users in the inoculation group
           */
          if (do_inocgroups)
          {
            char *l = listentry, *u;
            strsep (&l, ":");
            strsep (&l, ":");
            u = strsep (&l, ",");
            while (u != NULL) {
              if (strcasecmp(u,username) != 0) {
                LOGDEBUG ("adding user %s as target for inoculation", u);
                nt_add (ATX->inoc_users, u);
              }
              u = strsep (&l, ",");
            }
          }
          /*
           * When user is member of a global group or classification network
           * then consult all other users in the group
           */
          else if (do_classgroups)
          {
            char *l = listentry, *u;
            strsep (&l, ":");
            strsep (&l, ":");
            u = strsep (&l, ",");
            while (u != NULL) {
              if (u[0] == '*' && strcmp(u,"*") != 0) {
                /* global classification group */
                if (is_group_member_classification == 1) {
                  LOGDEBUG ("skipping global group (%s) entry %s: user %s is already in a classification group", group, u, username);
                  continue;
                } else if (strcasecmp(u+1,username) != 0 && is_group_member_global == 1) {
                  LOGDEBUG ("adding %s as classification peer for %s", u+1, username);
                  ATX->flags |= DAF_GLOBAL;
                  nt_add (ATX->classify_users, u+1);
                } else {
                  LOGDEBUG ("skipping global group entry %s for user %s", u+1, username);
                }
              } else if (strcasecmp(u,username) != 0) {
                /* classification network group */
                if (is_group_member_global == 1) {
                  LOGDEBUG ("skipping classification group (%s) entry %s: user %s is already in a global group", group, u, username);
                  continue;
                } else if (is_group_member_classification == 1) {
                  LOGDEBUG ("adding user %s to classification network group %", u, group);
                  nt_add (ATX->classify_users, u);
                } else {
                  LOGDEBUG ("skipping classification group entry %s for user %s", u, username);
                }
              }
              u = strsep (&l, ",");
            }
          }
        }
        free (list);
        free (listentry);
      }
      fclose (file);
    }
  }

  /* Crunch our agent context into a DSPAM context */

  f_mode = ATX->operating_mode;
  f_all  = DSF_SIGNATURE;

  if (ATX->flags & DAF_UNLEARN)
    f_all |= DSF_UNLEARN;

  /* If there is no preference, defer to commandline */
  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "enableBNR"), "")) {
    if (!strcmp(_ds_pref_val(ATX->PTX, "enableBNR"), "on"))
      f_all |= DSF_NOISE;
  } else {
    if (ATX->flags & DAF_NOISE)
     f_all |= DSF_NOISE;
  }

  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "")) {
    if (!strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "on"))
      f_all |= DSF_BIAS;
  } else {
    if (_ds_match_attribute(agent_config, "ProcessorBias", "on"))
      f_all |= DSF_BIAS;
  }

  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "enableWhitelist"), "")) {
    if (!strcmp(_ds_pref_val(ATX->PTX, "enableWhitelist"), "on"))
      f_all |= DSF_WHITELIST;
  } else {
    if (ATX->flags & DAF_WHITELIST)
      f_all |= DSF_WHITELIST;
  }

  if (ATX->flags & DAF_MERGED)
    f_all |= DSF_MERGED;

  CTX = dspam_create (username,
                    ctx_group,
                    _ds_read_attribute(agent_config, "Home"),
                    f_mode,
                    f_all);

  if (CTX == NULL)
    return NULL;

  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "statisticalSedation"), "")) {
    CTX->training_buffer = atoi(_ds_pref_val(ATX->PTX, "statisticalSedation"));
    LOGDEBUG("sedation level set to: %d", CTX->training_buffer);
  } else if (ATX->training_buffer>=0) {
    CTX->training_buffer = ATX->training_buffer;
    LOGDEBUG("sedation level set to: %d", CTX->training_buffer);
  }

  if (ATX->PTX != NULL && strcmp(_ds_pref_val(ATX->PTX, "whitelistThreshold"), ""))
    CTX->wh_threshold = atoi(_ds_pref_val(ATX->PTX, "whitelistThreshold"));

  if (ATX->classification != DSR_NONE) {
    CTX->classification  = ATX->classification;
    CTX->source          = ATX->source;
  }

  if (!( ATX->flags & DAF_FIXED_TR_MODE)
      && ATX->PTX != NULL
      && strcmp(_ds_pref_val(ATX->PTX, "trainingMode"), "")) {
    if (!strcasecmp(_ds_pref_val(ATX->PTX, "trainingMode"), "TEFT"))
      CTX->training_mode = DST_TEFT;
    else if (!strcasecmp(_ds_pref_val(ATX->PTX, "trainingMode"), "TOE"))
      CTX->training_mode = DST_TOE;
    else if (!strcasecmp(_ds_pref_val(ATX->PTX, "trainingMode"), "TUM"))
      CTX->training_mode = DST_TUM;
    else if (!strcasecmp(_ds_pref_val(ATX->PTX, "trainingMode"), "NOTRAIN"))
      CTX->training_mode = DST_NOTRAIN;
    else
      CTX->training_mode = ATX->training_mode;
  } else {
    CTX->training_mode = ATX->training_mode;
  }

  return CTX;
}

/*
 * retrain_message(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Retrain a message and perform iterative training
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing the classification results
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int retrain_message(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  int do_train = 1, iter = 0, ck_result = 0, t_mode = CTX->source;

  /* Train until test conditions are met, 5 iterations max */

  if (!_ds_match_attribute(agent_config, "TestConditionalTraining", "on")) {
    ck_result = dspam_process (CTX, NULL);
    if (ck_result != 0)
      return EFAILURE;
  } else {
    while (do_train && iter < 5)
    {
      DSPAM_CTX *CLX;
      int match;

      match = (CTX->classification == DSR_ISSPAM) ?
        DSR_ISSPAM : DSR_ISINNOCENT;
      iter++;

      ck_result = dspam_process (CTX, NULL);
      if (ck_result != 0)
        return EFAILURE;

      /* Only subtract innocent values once */
      CTX->source = DSS_CORPUS;

      LOGDEBUG ("reclassifying iteration %d result: %d", iter, ck_result);

      if (t_mode == DSS_CORPUS)
        do_train = 0;

      /* Only attempt test-conditional training on a mature corpus */

      if (CTX->totals.innocent_learned+CTX->totals.innocent_classified<1000 &&
          CTX->classification == DSR_ISSPAM)
      {
        do_train = 0;
      }
      else
      {
        int f_all =  DSF_SIGNATURE;

        /* CLX = Classify Context */
        if (ATX->flags & DAF_NOISE)
          f_all |= DSF_NOISE;

        if (ATX->PTX != NULL &&
            strcmp(_ds_pref_val(ATX->PTX, "processorBias"), ""))
        {
          if (!strcmp(_ds_pref_val(ATX->PTX, "processorBias"), "on"))
            f_all |= DSF_BIAS;
        } else {
          if (_ds_match_attribute(agent_config, "ProcessorBias", "on"))
            f_all |= DSF_BIAS;
        }

        CLX = dspam_create (CTX->username,
                          CTX->group,
                          _ds_read_attribute(agent_config, "Home"),
                          DSM_CLASSIFY,
                          f_all);
        if (!CLX)
          break;

        CLX->training_mode = CTX->training_mode;

        set_libdspam_attributes(CLX);
        if (attach_context(CLX, ATX->dbh)) {
          dspam_destroy(CLX);
          break;
        }

        CLX->signature = &ATX->SIG;
        ck_result = dspam_process (CLX, NULL);
        if (ck_result < 0) {
          CLX->signature = NULL;
          dspam_destroy(CLX);
          return EFAILURE;
        }
        if (ck_result == 0 || CLX->result == match)
          do_train = 0;
        CLX->signature = NULL;
        dspam_destroy (CLX);
      }
    }

    CTX->source = DSS_ERROR;
  }

  return 0;
}

/*
 * ensure_confident_result(DSPAM_CTX *CTX, AGENT_CTX *ATX, int result)
 *
 * DESCRIPTION
 *   Consult a global group or classification network if
 *   the user's filter instance isn't confident in its result
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing classification results
 *   ATX          Agent context defining processing behavior
 *   result       DSR_ processing result
 *
 * RETURN VALUES
 *   returns result networks believe the message should be
 */

/* ensure_confident_result: consult global group or
   clasification network if the user isn't confident in their result */

int ensure_confident_result(DSPAM_CTX *CTX, AGENT_CTX *ATX, int result) {
  int was_spam = 0;

  /* Exit if no users available for global group or classification network */
  if (ATX->classify_users && ATX->classify_users->items == 0)
    return result;

  /* global groups or classification network only operates on SPAM or INNOCENT */
  if (strcmp(CTX->class, LANG_CLASS_WHITELISTED) ==0 ||
      strcmp(CTX->class, LANG_CLASS_VIRUS) == 0 ||
      strcmp(CTX->class, LANG_CLASS_BLOCKLISTED) == 0 ||
      strcmp(CTX->class, LANG_CLASS_BLACKLISTED) == 0)
  {
    LOGDEBUG ("Not consulting %s group: message class is %s",
              (ATX->flags & DAF_GLOBAL) ? "global" : "classification",
              CTX->class);
    return result;
  }

  /* Defer to global group */
  if (ATX->flags & DAF_GLOBAL &&
      ((CTX->totals.innocent_learned + CTX->totals.innocent_corpusfed < 1000 ||
        CTX->totals.spam_learned + CTX->totals.spam_corpusfed < 250)         ||
      (CTX->training_mode == DST_NOTRAIN))
     )
  {
    if (result == DSR_ISSPAM) {
      was_spam = 1;
      CTX->result = DSR_ISINNOCENT;
      result = DSR_ISINNOCENT;
    }
    CTX->confidence = 0.60f;
  }

  if (result != DSR_ISSPAM               &&
      CTX->operating_mode == DSM_PROCESS &&
      CTX->classification == DSR_NONE    &&
      CTX->confidence < 0.65)
  {
    LOGDEBUG ("consulting %s group member list", (ATX->flags & DAF_GLOBAL) ? "global" : "classification");

    struct nt_node *node_int;
    struct nt_c c_i;

    node_int = c_nt_first (ATX->classify_users, &c_i);
    while (node_int != NULL && result != DSR_ISSPAM) {
      LOGDEBUG ("checking result for user %s", (const char *) node_int->ptr);
      result = user_classify (ATX, (const char *) node_int->ptr, CTX->signature, NULL);
      if (result == DSR_ISSPAM) {
        LOGDEBUG ("CLASSIFY CATCH: %s", (const char *) node_int->ptr);
        CTX->result = result;
      }
      node_int = c_nt_next (ATX->classify_users, &c_i);
    }

    /* If the global user thinks it's spam, and the user thought it was
     * innocent, retrain the user as a false negative.
     */
    if (result == DSR_ISSPAM && !was_spam) {
      LOGDEBUG ("re-adding as %s", LANG_CLASS_SPAM);
      DSPAM_CTX *CTC = malloc(sizeof(DSPAM_CTX));
      if (CTC == NULL) {
        LOG(LOG_CRIT, ERR_MEM_ALLOC);
        return EUNKNOWN;
      }
      memcpy(CTC, CTX, sizeof(DSPAM_CTX));
      CTC->operating_mode = DSM_PROCESS;
      CTC->classification = DSR_ISSPAM;
      CTC->source         = DSS_ERROR;
      CTC->flags         |= DSF_SIGNATURE;
      dspam_process (CTC, NULL);
      memcpy(&CTX->totals, &CTC->totals, sizeof(struct _ds_spam_totals));
      free(CTC);
      CTC = NULL;
      CTX->totals.spam_misclassified--;
      strncpy(CTX->class, LANG_CLASS_SPAM, sizeof(CTX->class));
      /* should we be resetting CTX->probability and CTX->confidence here as well? */
      CTX->result = result;
    /* If the global user thinks it's innocent, and the user thought it was
     * spam, retrain the user as a false positive
     */
    } else if (result == DSR_ISINNOCENT && was_spam) {
      LOGDEBUG ("re-adding as %s", LANG_CLASS_INNOCENT);
      DSPAM_CTX *CTC = malloc(sizeof(DSPAM_CTX));
      if (CTC == NULL) {
        LOG(LOG_CRIT, ERR_MEM_ALLOC);
        return EUNKNOWN;
      }
      memcpy(CTC, CTX, sizeof(DSPAM_CTX));
      CTC->operating_mode = DSM_PROCESS;
      CTC->classification = DSR_ISINNOCENT;
      CTC->source         = DSS_ERROR;
      CTC->flags         |= DSF_SIGNATURE;
      dspam_process (CTC, NULL);
      memcpy(&CTX->totals, &CTC->totals, sizeof(struct _ds_spam_totals));
      free(CTC);
      CTC = NULL;
      CTX->totals.innocent_misclassified--;
      strncpy(CTX->class, LANG_CLASS_INNOCENT, sizeof(CTX->class));
      /* should we be resetting CTX->probability and CTX->confidence here as well? */
      CTX->result = result;
    }
  }

  return result;
}

/*
 * log_prepare(char *buffer, char *value)
 *
 * DESCRIPTION
 *   Prepares a value for logging by copying it to the buffer and removing
 *   all potentially dangerous characters.
 *
 * INPUT ARGUMENTS
 *   buffer	  A 256-byte buffer to store the result into
 *   value	  Value to be logged or NULL
 *
 * RETURN VALUES
 *   None
 */

static void log_prepare(char *buffer, char *value) {
  char *p;

  if (!value)
    value = "<None Specified>";
  strncpy(buffer, value, 255);
  buffer[255] = 0;
  for (p=buffer; *p; p++)
    if (*p >= 0 && *p < 32)
      *p = ' ';
}

/*
 * log_events(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Log events to system and user logs
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int log_events(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  char filename[MAX_FILENAME_LENGTH];
  char *subject = NULL, *from = NULL;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  FILE *file;
  char class;
  char x[1024], subject_buf[256], from_buf[256];
  char *messageid = NULL;

  if (CTX->message)
    messageid = _ds_find_header(CTX->message, "Message-Id");

  if (ATX->status[0] == 0 && CTX->source == DSS_ERROR &&
     (!(ATX->flags & DAF_UNLEARN)))
  {
    STATUS("Retrained");
  }

  if (ATX->status[0] == 0 && CTX->classification == DSR_NONE
                          && CTX->result == DSR_ISSPAM
                          && ATX->status[0] == 0)
  {
    if (_ds_pref_val(ATX->PTX, "spamAction")[0] == 0 ||
        !strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "quarantine"))
    {
      STATUS("Quarantined");
    } else if (!strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "tag")) {
      STATUS("Tagged");
    } else if (!strcmp(_ds_pref_val(ATX->PTX, "spamAction"), "deliver")) {
      STATUS("Delivered");
    }
  }

  if (ATX->status[0] == 0             &&
      CTX->classification == DSR_NONE &&
      CTX->result == DSR_ISINNOCENT)
  {
    STATUS("Delivered");
  }

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"), LOOKUP(ATX->PTX, (ATX->managed_group[0]) ? ATX->managed_group : CTX->username), "log");

  if (CTX->message)
  {
    node_nt = c_nt_first (CTX->message->components, &c_nt);
    if (node_nt != NULL)
    {
      ds_message_part_t block;
      block = node_nt->ptr;
      if (block->headers != NULL)
      {
        ds_header_t head;
        struct nt_node *node_header;
        node_header = block->headers->first;
        while(node_header != NULL) {
          head = (ds_header_t) node_header->ptr;
          if (head) {
            if (!strcasecmp(head->heading, "Subject")) {
              subject = head->data;
              if (from != NULL) break;
            } else if (!strcasecmp(head->heading, "From")) {
              from = head->data;
              if (subject != NULL) break;
            }
          }
          node_header = node_header->next;
        }
      }
    }
  }

  if (!strcmp(CTX->class, LANG_CLASS_WHITELISTED))
    class = 'W';
  else if (!strcmp(CTX->class, LANG_CLASS_VIRUS))
    class = 'V';
  else if (!strcmp(CTX->class, LANG_CLASS_BLACKLISTED))
    class = 'A';
  else if (!strcmp(CTX->class, LANG_CLASS_BLOCKLISTED))
    class = 'O';
  else if (CTX->result == DSR_ISSPAM)
    class = 'S';
  else if (CTX->result == DSR_ISINNOCENT)
    class = 'I';
  else
    class = 'U';

  if (CTX->source == DSS_ERROR) {
    if (CTX->classification == DSR_ISSPAM)
      class = 'M';
    else if (CTX->classification == DSR_ISINNOCENT)
      class = 'F';
  } else if (CTX->source == DSS_INOCULATION)
    class = 'N';
  else if (CTX->source == DSS_CORPUS)
    class = 'C';

  if (ATX->flags & DAF_UNLEARN) {
    char stat[256];
    snprintf(stat, sizeof(stat), "Delivery Failed (%s)",
             (ATX->status[0]) ? ATX->status : "No error provided");
    STATUS("%s", stat);
    class = 'E';
  }

  log_prepare(from_buf, from);
  log_prepare(subject_buf, subject);

  /* Write USER.log */

  if (_ds_match_attribute(agent_config, "UserLog", "on")) {

    snprintf(x, sizeof(x), "%ld\t%c\t%s\t%s\t%s\t%s\t%s\n",
            (long) time(NULL),
            class,
            from_buf,
            ATX->signature,
            subject_buf,
            ATX->status,
            (messageid) ? messageid : "");


    _ds_prepare_path_for(filename);
    file = fopen(filename, "a");
    if (file != NULL) {
      int i = _ds_get_fcntl_lock(fileno(file));
      if (!i) {
          fputs(x, file);
          fputs("\n", file);
          _ds_free_fcntl_lock(fileno(file));
      } else {
        LOG(LOG_WARNING, ERR_IO_LOCK, filename, i, strerror(errno));
      }
      fclose(file);
    }
  }

  /* Write system.log */

  if (_ds_match_attribute(agent_config, "SystemLog", "on")) {
    snprintf(filename, sizeof(filename), "%s/system.log",
             _ds_read_attribute(agent_config, "Home"));
    file = fopen(filename, "a");
    if (file != NULL) {
      int i = _ds_get_fcntl_lock(fileno(file));
      if (!i) {

        snprintf(x, sizeof(x), "%ld\t%c\t%s\t%s\t%s\t%f\t%s\t%s\t%s\n",
            (long) time(NULL),
            class,
            from_buf,
            ATX->signature,
            subject_buf,
            _ds_gettime()-ATX->timestart,
	    (CTX->username) ? CTX->username: "",
	    (ATX->status) ? ATX->status : "",
            (messageid) ? messageid : "");
        fputs(x, file);
        _ds_free_fcntl_lock(fileno(file));
      } else {
        LOG(LOG_WARNING, ERR_IO_LOCK, filename, i, strerror(errno));
      }
      fclose(file);
    }
  }
  return 0;
}

/*
 * add_xdspam_headers(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Add X-DSPAM headers to the message being processed
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing message and results
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */


int add_xdspam_headers(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  struct nt_node *node_nt;
  struct nt_c c_nt;

  node_nt = c_nt_first (CTX->message->components, &c_nt);
  if (node_nt != NULL)
  {
    ds_message_part_t block = node_nt->ptr;
    struct nt_node *node_ft;
    struct nt_c c_ft;
    if (block != NULL && block->headers != NULL)
    {
      ds_header_t head;
      char data[10240];
      char scratch[128];

      snprintf(data, sizeof(data), "%s: %s",
        (CTX->source == DSS_ERROR) ? "X-DSPAM-Reclassified" : "X-DSPAM-Result",
        CTX->class);

      head = _ds_create_header_field(data);
      if (head != NULL)
      {
#ifdef VERBOSE
        LOGDEBUG ("appending header %s: %s", head->heading, head->data);
#endif
        nt_add (block->headers, (void *) head);
      }
      else {
        LOG (LOG_CRIT, ERR_MEM_ALLOC);
      }

      if (CTX->source == DSS_NONE) {
        char buf[27];
        time_t t = time(NULL);
        ctime_r(&t, buf);
        chomp(buf);
        snprintf(data, sizeof(data), "X-DSPAM-Processed: %s", buf);
        head = _ds_create_header_field(data);
        if (head != NULL)
        {
#ifdef VERBOSE
          LOGDEBUG("appending header %s: %s", head->heading, head->data);
#endif
          nt_add(block->headers, (void *) head);
        }
        else
          LOG (LOG_CRIT, ERR_MEM_ALLOC);
      }

      if (CTX->source != DSS_ERROR) {
        snprintf(data, sizeof(data), "X-DSPAM-Confidence: %01.4f",
                 CTX->confidence);
        head = _ds_create_header_field(data);
        if (head != NULL)
        {
#ifdef VERBOSE
          LOGDEBUG("appending header %s: %s", head->heading, head->data);
#endif
          nt_add(block->headers, (void *) head);
        }
        else
          LOG (LOG_CRIT, ERR_MEM_ALLOC);

        if (_ds_match_attribute(agent_config, "ImprobabilityDrive", "on"))
        {
          float probability = CTX->confidence;
          char *as;
          if (probability > 0.999999)
            probability = 0.999999;
          if (CTX->result == DSR_ISINNOCENT) {
            as = "spam";
          } else {
            as = "ham";
          }
          snprintf(data, sizeof(data), "X-DSPAM-Improbability: 1 in %.0f "
            "chance of being %s",
            1.0+(100*(probability / (1-probability))), as);
          head = _ds_create_header_field(data);
          if (head != NULL)
          {
#ifdef VERBOSE
            LOGDEBUG("appending header %s: %s", head->heading, head->data);
#endif
            nt_add(block->headers, (void *) head);
          }
          else
            LOG (LOG_CRIT, ERR_MEM_ALLOC);
        }


        snprintf(data, sizeof(data), "X-DSPAM-Probability: %01.4f",
                 CTX->probability);

        head = _ds_create_header_field(data);
        if (head != NULL)
        {
#ifdef VERBOSE
          LOGDEBUG ("appending header %s: %s", head->heading, head->data);
#endif
            nt_add (block->headers, (void *) head);
        }
        else
          LOG (LOG_CRIT, ERR_MEM_ALLOC);

        if (CTX->training_mode != DST_NOTRAIN && ATX->signature[0] != 0) {
          snprintf(data, sizeof(data), "X-DSPAM-Signature: %s", ATX->signature);

          head = _ds_create_header_field(data);
          if (head != NULL)
          {
            if (strlen(ATX->signature)<5)
            {
              LOGDEBUG("WARNING: Signature not generated, or invalid");
            }
#ifdef VERBOSE
            LOGDEBUG ("appending header %s: %s", head->heading, head->data);
#endif
            nt_add (block->headers, (void *) head);
          }
          else
            LOG (LOG_CRIT, ERR_MEM_ALLOC);
        }

        if (CTX->result == DSR_ISSPAM && (ATX->managed_group[0] || (_ds_pref_val(ATX->PTX, "localStore")[0])))
        {
          snprintf(data, sizeof(data), "X-DSPAM-User: %s", CTX->username);
          head = _ds_create_header_field(data);
          if (head != NULL)
          {
#ifdef VERBOSE
            LOGDEBUG ("appending header %s: %s", head->heading, head->data);
#endif
              nt_add (block->headers, (void *) head);
          }
          else
            LOG (LOG_CRIT, ERR_MEM_ALLOC);
        }

        if (!strcmp(_ds_pref_val(ATX->PTX, "showFactors"), "on")) {

          if (CTX->factors != NULL) {
            snprintf(data, sizeof(data), "X-DSPAM-Factors: %d",
                     CTX->factors->items);
            node_ft = c_nt_first(CTX->factors, &c_ft);
            while(node_ft != NULL) {
              struct dspam_factor *f = (struct dspam_factor *) node_ft->ptr;
              if (f) {
	        char *s, *t;
                strlcat(data, ",\n\t", sizeof(data));
		s = f->token_name;
		t = scratch;
		while (*s && t < scratch + sizeof(scratch) - 16)
		  if (*s >= ' ' && *s < 0x7f && *s != '%')
		    *t++ = *s++;
		  else
		    t += sprintf(t, "%%%02x", (unsigned char) *s++);
		snprintf(t, 15, ", %2.5f", f->value);
                strlcat(data, scratch, sizeof(data));
              }
              node_ft = c_nt_next(CTX->factors, &c_ft);
            }
            head = _ds_create_header_field(data);
            if (head != NULL)
            {
#ifdef VERBOSE
              LOGDEBUG("appending header %s: %s", head->heading, head->data);
#endif
              nt_add(block->headers, (void *) head);
            }
          }
        }

      } /* CTX->source != DSS_ERROR */
    }
  }
  return 0;
}

/*
 * embed_msgtag(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Embed a message tag
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing the message
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int embed_msgtag(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  char toplevel_boundary[128] = { 0 };
  ds_message_part_t block;
  int i = 0;
  FILE *f;
  char buff[1024], msgfile[MAX_FILENAME_LENGTH];
  buffer *b;
  ATX = ATX; /* Keep compiler happy */

  if (CTX->result != DSR_ISSPAM && CTX->result != DSR_ISINNOCENT)
      return EINVAL;

  node_nt = c_nt_first (CTX->message->components, &c_nt);
  if (node_nt == NULL || node_nt->ptr == NULL)
    return EFAILURE;

  block = node_nt->ptr;

  /* Signed messages cannot be tagged */

  if (block->media_subtype == MST_SIGNED)
    return EINVAL;

  /* Load the message tag */
  if (_ds_read_attribute(agent_config, "TxtDirectory")) {
    snprintf(msgfile, sizeof(msgfile), "%s/msgtag.%s",
           _ds_read_attribute(agent_config, "TxtDirectory"),
           (CTX->result == DSR_ISSPAM) ? "spam" : "nonspam");
  } else {
    snprintf(msgfile, sizeof(msgfile), "%s/txt/msgtag.%s",
           _ds_read_attribute(agent_config, "Home"),
           (CTX->result == DSR_ISSPAM) ? "spam" : "nonspam");
  }
  f = fopen(msgfile, "r");
  if (!f) {
    LOG(LOG_ERR, ERR_IO_FILE_OPEN, msgfile, strerror(errno));
    return EFILE;
  }
  b = buffer_create(NULL);
  if (!b) {
    LOG(LOG_CRIT, ERR_MEM_ALLOC);
    fclose(f);
    return EUNKNOWN;
  }
  while(fgets(buff, sizeof(buff), f)!=NULL) {
      buffer_cat(b, buff);
  }
  fclose(f);

  if (block->media_type == MT_MULTIPART && block->terminating_boundary != NULL)
  {
    strlcpy(toplevel_boundary, block->terminating_boundary,
            sizeof(toplevel_boundary));
  }

  while (node_nt != NULL)
  {
    char *body_close = NULL, *dup = NULL;

    block = node_nt->ptr;

    /* Append signature to blocks when... */

    if (block != NULL

        /* Either a text section, or this is a non-multipart message AND...*/
        && (block->media_type == MT_TEXT
            || (block->boundary == NULL && i == 0
                && block->media_type != MT_MULTIPART))
        && (toplevel_boundary[0] == 0 || (block->body && block->body->used)))
    {
      if (block->content_disposition == PCD_ATTACHMENT)
      {
        node_nt = c_nt_next (CTX->message->components, &c_nt);
        i++;
        continue;
      }

      /* Some email clients reformat HTML parts, and require that we include
       * the signature before the HTML close tags (because they're stupid)
       */

      if (body_close		== NULL &&
          block->body		!= NULL &&
          block->body->data	!= NULL &&
          block->media_subtype	== MST_HTML)

      {
        body_close = strcasestr(block->body->data, "</body");
        if (!body_close)
          body_close = strcasestr(block->body->data, "</html");
      }

      /* Save and truncate everything after and including the close tag */
      if (body_close)
      {
        dup = strdup (body_close);
        block->body->used -= (long) strlen (dup);
        body_close[0] = 0;
      }

      buffer_cat (block->body, "\n");
      buffer_cat (block->body, b->data);

      if (dup)
      {
        buffer_cat (block->body, dup);
        free (dup);
      }
    }

    node_nt = c_nt_next (CTX->message->components, &c_nt);
    i++;
  }
  buffer_destroy(b);
  return 0;
}

/*
 * embed_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Embed the DSPAM signature in all relevant parts of the message
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing the message
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int embed_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  char toplevel_boundary[128] = { 0 };
  ds_message_part_t block;
  int i = 0;

  if (CTX->training_mode == DST_NOTRAIN || ! ATX->signature[0])
    return 0;

  node_nt = c_nt_first (CTX->message->components, &c_nt);

  if (node_nt == NULL || node_nt->ptr == NULL)
    return EFAILURE;

  block = node_nt->ptr;

  /* Signed messages are handled differently */

  if (block->media_subtype == MST_SIGNED)
    return embed_signed(CTX, ATX);

  if (block->media_type == MT_MULTIPART && block->terminating_boundary != NULL)
  {
    strlcpy(toplevel_boundary, block->terminating_boundary,
            sizeof(toplevel_boundary));
  }

  while (node_nt != NULL)
  {
    char *body_close = NULL, *dup = NULL;

    block = node_nt->ptr;

    /* Append signature to blocks when... */

    if (block != NULL

        /* Either a text section, or this is a non-multipart message AND...*/
        && (block->media_type == MT_TEXT
            || (block->boundary == NULL && i == 0
                && block->media_type != MT_MULTIPART))
        && (toplevel_boundary[0] == 0 || (block->body && block->body->used)))
    {
      if (block->content_disposition == PCD_ATTACHMENT)
      {
        node_nt = c_nt_next (CTX->message->components, &c_nt);
        i++;
        continue;
      }

      /* Some email clients reformat HTML parts, and require that we include
       * the signature before the HTML close tags (because they're stupid)
       */

      if (body_close		== NULL &&
          block->body		!= NULL &&
          block->body->data	!= NULL &&
          block->media_subtype	== MST_HTML)

      {
        body_close = strcasestr(block->body->data, "</body");
        if (!body_close)
          body_close = strcasestr(block->body->data, "</html");
      }

      /* Save and truncate everything after and including the close tag */
      if (body_close)
      {
        dup = strdup (body_close);
        block->body->used -= (long) strlen (dup);
        body_close[0] = 0;
      }

      buffer_cat (block->body, "\n");
      buffer_cat (block->body, SIGNATURE_BEGIN);
      buffer_cat (block->body, ATX->signature);
      buffer_cat (block->body, SIGNATURE_END);
      buffer_cat (block->body, "\n\n");

      if (dup)
      {
        buffer_cat (block->body, dup);
        buffer_cat (block->body, "\n\n");
        free (dup);
      }
    }

    node_nt = c_nt_next (CTX->message->components, &c_nt);
    i++;
  }
  return 0;
}

/*
 * embed_signed(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Embed the DSPAM signature within a signed message
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing message
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int embed_signed(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  struct nt_node *node_nt, *node_block, *parent;
  struct nt_c c_nt;
  ds_message_part_t block, newblock;
  ds_header_t field;
  char scratch[256], data[256];

  node_block = c_nt_first (CTX->message->components, &c_nt);
  if (node_block == NULL || node_block->ptr == NULL)
    return EFAILURE;

  block = node_block->ptr;

  /* Construct a new block to contain the signed message */

  newblock = (ds_message_part_t) malloc(sizeof(struct _ds_message_part));
  if (newblock == NULL)
    goto MEM_ALLOC;

  newblock->headers = nt_create(NT_PTR);
  if (newblock->headers == NULL)
    goto MEM_ALLOC;

  newblock->boundary		= NULL;
  newblock->terminating_boundary= block->terminating_boundary;
  newblock->encoding		= block->encoding;
  newblock->original_encoding	= block->original_encoding;
  newblock->media_type		= block->media_type;
  newblock->media_subtype	= block->media_subtype;
  newblock->body		= buffer_create (NULL);
  newblock->original_signed_body= NULL;

  /* Move the relevant headers from the main part to the new block */

  parent = NULL;
  node_nt = c_nt_first(block->headers, &c_nt);
  while(node_nt != NULL) {
    field = node_nt->ptr;
    if (field) {
      if (!strcasecmp(field->heading, "Content-Type") ||
          !strcasecmp(field->heading, "Content-Disposition"))
      {
        struct nt_node *old = node_nt;
        node_nt = c_nt_next(block->headers, &c_nt);
        if (parent)
          parent->next = node_nt;
        else
          block->headers->first = node_nt;
        nt_add(newblock->headers, field);
        free(old);
        old = NULL;
        block->headers->items--;
        continue;
      }
    }
    parent = node_nt;
    node_nt = c_nt_next(block->headers, &c_nt);
  }

  /* Create a new top-level boundary */
  snprintf(scratch, sizeof(scratch), "DSPAM_MULTIPART_EX-%ld", (long)getpid());
  block->terminating_boundary = strdup(scratch);

  /* Create a new content-type field */
  block->media_type    = MT_MULTIPART;
  block->media_subtype = MST_MIXED;
  snprintf(data, sizeof(data), "Content-Type: multipart/mixed; boundary=%s", scratch);
  field = _ds_create_header_field(data);
  if (field != NULL)
    nt_add(block->headers, field);

  /* Insert the new block right below the top headers and blank body */
  node_nt = nt_node_create(newblock);
  if (node_nt == NULL)
    goto MEM_ALLOC;
  node_nt->next = node_block->next;
  node_block->next = node_nt;
  CTX->message->components->items++;

  /* Strip the old terminating boundary */

  parent = NULL;
  node_nt = c_nt_first (CTX->message->components, &c_nt);
  while (node_nt)
  {
    if (!node_nt->next && parent) {
      parent->next = NULL;
      CTX->message->components->items--;
      CTX->message->components->insert = NULL;
      _ds_destroy_block(node_nt->ptr);
      free(node_nt);
      node_nt = NULL;
    } else {
      parent = node_nt;
      node_nt = node_nt->next;
    }
  }

  /* Create a new message part containing only the boundary delimiter */

  newblock = (ds_message_part_t)
    malloc(sizeof(struct _ds_message_part));
  if (newblock == NULL)
    goto MEM_ALLOC;

  newblock->headers = nt_create(NT_PTR);
  if (newblock->headers == NULL)
    goto MEM_ALLOC;

  newblock->boundary            = NULL;
  newblock->terminating_boundary= strdup(scratch);
  newblock->encoding            = EN_7BIT;
  newblock->original_encoding   = EN_7BIT;
  newblock->media_type          = MT_TEXT;
  newblock->media_subtype       = MST_PLAIN;
  newblock->body                = buffer_create (NULL);
  newblock->original_signed_body= NULL;
  nt_add (CTX->message->components, newblock);

  /* Create a new message part containing the signature */

  newblock = (ds_message_part_t) malloc(sizeof(struct _ds_message_part));
  if (newblock == NULL)
    goto MEM_ALLOC;

  newblock->headers = nt_create(NT_PTR);
  if (newblock->headers == NULL)
    goto MEM_ALLOC;

  snprintf(data, sizeof(data), "%s--\n\n", scratch);
  newblock->boundary		= NULL;
  newblock->terminating_boundary= strdup(data);
  newblock->encoding		= EN_7BIT;
  newblock->original_encoding	= EN_7BIT;
  newblock->media_type		= MT_TEXT;
  newblock->media_subtype	= MST_PLAIN;
  snprintf (scratch, sizeof (scratch),
    "%s%s%s\n", SIGNATURE_BEGIN, ATX->signature, SIGNATURE_END);
  newblock->body		= buffer_create (scratch);
  newblock->original_signed_body= NULL;

  field = _ds_create_header_field ("Content-Type: text/plain");
  nt_add (newblock->headers, field);
  snprintf(data, sizeof(data), "X-DSPAM-Signature: %s", ATX->signature);
  nt_add (newblock->headers, _ds_create_header_field(data));
  nt_add (CTX->message->components, newblock);

  return 0;

MEM_ALLOC:
  if (newblock) {
    if (newblock->headers)
      nt_destroy(newblock->headers);
    free(newblock);
    newblock = NULL;
  }

  LOG (LOG_CRIT, ERR_MEM_ALLOC);
  return EUNKNOWN;
}

/*
 * tracksources(DSPAM_CTX *CTX)
 *
 * DESCRIPTION
 *   Track the source address of a message, report to syslog and/or RABL
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing filter results and message
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors on failure
 */

int tracksource(DSPAM_CTX *CTX) {
  char ip[32];

  if (!dspam_getsource (CTX, ip, sizeof (ip)))
  {
    if (CTX->totals.innocent_learned + CTX->totals.innocent_classified > 2500) {
      if (CTX->result == DSR_ISSPAM &&
          strcmp(CTX->class, LANG_CLASS_VIRUS) != 0 &&
          _ds_match_attribute(agent_config, "TrackSources", "spam")) {
        FILE *file;
        char dropfile[MAX_FILENAME_LENGTH];
        LOG (LOG_INFO, "spam detected from %s", ip);
        if (_ds_read_attribute(agent_config, "RABLQueue")) {
          snprintf(dropfile, sizeof(dropfile), "%s/%s",
            _ds_read_attribute(agent_config, "RABLQueue"), ip);
          file = fopen(dropfile, "w");
          if (file != NULL)
            fclose(file);
        }
      } else if (CTX->result == DSR_ISSPAM &&
          strcmp(CTX->class, LANG_CLASS_VIRUS) == 0 &&
          _ds_match_attribute(agent_config, "TrackSources", "virus"))
      {
        LOG (LOG_INFO, "infected message from %s", ip);
      } else if (CTX->result != DSR_ISSPAM &&
          strcmp(CTX->class, LANG_CLASS_VIRUS) != 0 &&
          _ds_match_attribute(agent_config, "TrackSources", "nonspam"))
      {
        LOG (LOG_INFO, "innocent message from %s", ip);
      }
    }
  }
  return 0;
}

#ifdef CLAMAV

/*
 * has_virus(buffer *message)
 *
 * DESCRIPTION
 *   Call ClamAV to determine if the message has a virus
 *
 * INPUT ARGUMENTS
 *    message     pointer to buffer containing message for scanning
 *
 * RETURN VALUES
 *   returns 1 if virus, 0 otherwise
 */

int has_virus(buffer *message) {
  struct sockaddr_in addr;
  int sockfd;
  int virus = 0;
  int yes = 1;
  int port = atoi(_ds_read_attribute(agent_config, "ClamAVPort"));
  int addr_len;
  char *host = _ds_read_attribute(agent_config, "ClamAVHost");
  FILE *sock;
  FILE *sockout;
  char buf[128];

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    LOG(LOG_ERR, "socket(AF_INET, SOCK_STREAM, 0): %s", strerror(errno));
    return 0;
  }
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(host);
  addr.sin_port = htons(port);
  addr_len = sizeof(struct sockaddr_in);
  LOGDEBUG("Connecting to %s:%d for virus check", host, port);
  if(connect(sockfd, (struct sockaddr *)&addr, addr_len)<0) {
    LOG(LOG_ERR, ERR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
    close(sockfd);
    return 0;
  }

  setsockopt(sockfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

  sock = fdopen(sockfd, "r");
  if (sock == NULL) {
    LOG(LOG_ERR, ERR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
    close(sockfd);
    return 0;
  }
  sockout = fdopen(sockfd, "w");
  if (sockout == NULL) {
    LOG(LOG_ERR, ERR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
    fclose(sock);
    close(sockfd);
    return 0;
  }
  fprintf(sockout, "STREAM\r\n");
  fflush(sockout);

  if ((fgets(buf, sizeof(buf), sock))!=NULL && !strncmp(buf, "PORT", 4)) {
    int s_port = atoi(buf+5);
    if (feed_clam(s_port, message)==0) {
      if ((fgets(buf, sizeof(buf), sock))!=NULL) {
        if (!strstr(buf, ": OK"))
          virus = 1;
      }
    }
  }
  fclose(sock);
  fclose(sockout);
  close(sockfd);

  return virus;
}

/*
 * feed_clam(int port, buffer *message)
 *
 * DESCRIPTION
 *   Feed a stream to ClamAV for virus detection
 *
 * INPUT ARGUMENTS
 *    sockfd      port number of stream
 *    message     pointer to buffer containing message for scanning
 *
 * RETURN VALUES
 *   returns 0 on success
 */

int feed_clam(int port, buffer *message) {
  struct sockaddr_in addr;
  int sockfd, r, addr_len;
  int yes = 1;
  long sent = 0;
  long size = strlen(message->data);
  char *host = _ds_read_attribute(agent_config, "ClamAVHost");

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    LOG(LOG_ERR, "socket(AF_INET, SOCK_STREAM, 0): %s", strerror(errno));
    return EFAILURE;
  }
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(host);
  addr.sin_port = htons(port);
  addr_len = sizeof(struct sockaddr_in);
  LOGDEBUG("Connecting to %s:%d for virus stream transmission", host, port);
  if(connect(sockfd, (struct sockaddr *)&addr, addr_len)<0) {
    LOG(LOG_ERR, ERR_CLIENT_CONNECT_HOST, host, port, strerror(errno));
    close(sockfd);
    return EFAILURE;
  }

  setsockopt(sockfd,SOL_SOCKET,TCP_NODELAY,&yes,sizeof(int));

  while(sent<size) {
    r = send(sockfd, message->data+sent, size-sent, 0);
    if (r <= 0) {
      close(sockfd);
      return r;
    }
    sent += r;
  }

  close(sockfd);
  return 0;
}

#endif

/*
 * is_blacklisted(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Determine if the source address of the message is blacklisted
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing the message
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 1 if blacklisted, 0 otherwise
 */

int is_blacklisted(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
#ifdef __CYGWIN__
  /* No cygwin support for IP Blacklisting */
  return 0;
#else
  char ip[32];
  int bad = 0;
  struct attribute *attrib;
  struct addrinfo *res = NULL;
  struct sockaddr_in saddr;
  char host[32];
  char lookup[256];
  char *ptr;
  char *octet[4];
  int i = 3;
  octet[0] = octet[1] = octet[2] = octet[3] = NULL;

  if (!dspam_getsource (CTX, ip, sizeof (ip))) {
    host[0] = 0;
    ptr = strtok(ip, ".");
    while(ptr != NULL && i>=0 && i<4) {
      octet[i] = ptr;
      ptr = strtok(NULL, ".");
      if (ptr == NULL && i!=0)
        return 0;
      i--;
    }

    if (octet[0] == NULL || octet[1] == NULL || octet[2] == NULL || octet[3] == NULL)
      return 0;

    snprintf(host, sizeof(host), "%s.%s.%s.%s.", octet[0], octet[1], octet[2], octet[3]);

    attrib = _ds_find_attribute(agent_config, "Lookup");
    while(attrib != NULL) {
      int error;
      snprintf(lookup, sizeof(lookup), "%s%s", host, attrib->value);
      error = getaddrinfo(lookup, NULL, NULL, &res);
      if (!error) {
        char buff[128];
        if (!bad) {
          memcpy(&saddr, res->ai_addr, sizeof(struct sockaddr));
#ifdef HAVE_INET_NTOA_R_2
          inet_ntoa_r(saddr.sin_addr, buff);
#else
          inet_ntoa_r(saddr.sin_addr, buff, sizeof(buff));
#endif
          if (strncmp(buff, "127.0.0.", 8) == 0) {
            STATUS("Blacklisted (%s)", attrib->value);
            bad = 1;
            freeaddrinfo(res);
            break;
          }
        }
        freeaddrinfo(res);
      }
      attrib = attrib->next;
    }
  }

  return bad;
#endif
}

/*
 * is_blocklisted(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Determine if the source address of the message is blocklisted on
 *   the destination user's blocklist
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context containing the message
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 1 if blacklisted, 0 otherwise
 */

int is_blocklisted(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;
  int blocklisted = 0;
  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"),
                   LOOKUP(ATX->PTX, (ATX->managed_group[0]) ? ATX->managed_group
                                     : CTX->username), "blocklist");
  file = fopen(filename, "r");
  if (file != NULL) {
    char *heading = _ds_find_header(CTX->message, "From");
    char buf[256];
    if (heading) {
      char *dup = strdup(heading);
      char *domain = strrchr(dup, '@');
      if (domain) {
        int i;
        for(i=0;domain[i] && domain[i]!='\r' && domain[i]!='\n'
             && domain[i]!='>' && !isspace((int) domain[i]);i++) { }
        domain[i] = 0;
        while((fgets(buf, sizeof(buf), file))!=NULL) {
          chomp(buf);
          if (!strcasecmp(buf, domain+1)) {
            blocklisted = 1;
            break;
          }
        }
      }
      free(dup);
      dup = NULL;
    }
    fclose(file);
  }
  return blocklisted;
}

/*
 * daemon_start(AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Launch into daemon mode and start listener
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on successful termination
 */

#ifdef DAEMON
int daemon_start(AGENT_CTX *ATX) {
  DRIVER_CTX DTX;
  char *pidfile;
  ATX = ATX; /* Keep compiler happy */
  int exitcode = EXIT_SUCCESS;

  if (ATX->fork && fork())    /* Fork DSPAM into the background */
    exit(exitcode);

  __daemon_run  = 1;
  __num_threads = 0;
  __hup = 0;
  pthread_mutex_init(&__lock, NULL);
  if (libdspam_init(_ds_read_attribute(agent_config, "StorageDriver"))) {
    LOG(LOG_CRIT, ERR_DRV_INIT);
    // pthread_mutex_destroy(&__lock);
    exit(EXIT_FAILURE);
  }

  LOG(LOG_INFO, INFO_DAEMON_START);

  while(__daemon_run) {

    DTX.CTX = dspam_create (NULL, NULL,
                      _ds_read_attribute(agent_config, "Home"),
                      DSM_TOOLS, 0);
    if (!DTX.CTX)
    {
      LOG(LOG_ERR, ERR_CORE_INIT);
      // pthread_mutex_destroy(&__lock);
      // libdspam_shutdown();
      exit(EXIT_FAILURE);
    }

    set_libdspam_attributes(DTX.CTX);
    DTX.flags = DRF_STATEFUL;

#ifdef DEBUG
    if (DO_DEBUG)
      DO_DEBUG = 2;
#endif
    if (dspam_init_driver (&DTX))
    {
      LOG (LOG_WARNING, ERR_DRV_INIT);
      // pthread_mutex_destroy(&__lock);
      // libdspam_shutdown();
      exit(EXIT_FAILURE);
    }

    pidfile = _ds_read_attribute(agent_config, "ServerPID");
    if ( pidfile == NULL )
      pidfile = "/var/run/dspam/dspam.pid";

    if (pidfile) {
      FILE *file;
      file = fopen(pidfile, "w");
      if (file == NULL) {
        LOG(LOG_ERR, ERR_IO_FILE_WRITE, pidfile, strerror(errno));
        dspam_shutdown_driver(&DTX);
        libdspam_shutdown();
        exit(EXIT_FAILURE);
      } else {
        fprintf(file, "%ld\n", (long) getpid());
        fclose(file);
      }
    }

    LOGDEBUG("Spawning daemon listener");

    if (daemon_listen(&DTX)) {
      LOG(LOG_CRIT, ERR_DAEMON_FAIL);
      __daemon_run = 0;
      exitcode = EXIT_FAILURE;
    } else {
      LOG(LOG_WARNING, "Received signal. Waiting for processing threads to exit.");
      while(__num_threads) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);
      }
      LOG(LOG_WARNING, "Processing threads terminated.");
    }

    /* only unlink pid file if daemon is shut down */
    if (pidfile && !__daemon_run)
      unlink(pidfile);

    dspam_shutdown_driver(&DTX);
    dspam_destroy(DTX.CTX);

    /* Reload */
    if (__hup) {
      LOG(LOG_WARNING, INFO_DAEMON_RELOAD);

      if (agent_config)
        _ds_destroy_config(agent_config);

      agent_config = read_config(NULL);
      if (!agent_config) {
        LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
        pthread_mutex_destroy(&__lock);
        libdspam_shutdown();
        exit(EXIT_FAILURE);
      }

      __daemon_run = 1;
      __hup = 0;
    }
  }

  LOG(LOG_WARNING, INFO_DAEMON_EXIT);
  pthread_mutex_destroy(&__lock);
  libdspam_shutdown();

  return exitcode;
}
#endif

/*
 * load_aggregated_prefs(AGENT_CTX *ATX, const char *username)
 *
 * DESCRIPTION
 *   Load and aggregate system+user preferences
 *
 * INPUT ARGUMENTS
 *   ATX          Agent context defining processing behavior
 *   username     Target user
 *
 * RETURN VALUES
 *   pointer to aggregated preference structure, NULL on failure
 */

agent_pref_t load_aggregated_prefs(AGENT_CTX *ATX, const char *username) {
  agent_pref_t PTX = NULL;
  agent_pref_t STX = NULL;
  agent_pref_t UTX = NULL;

  LOGDEBUG("loading preferences for user %s", username);
  UTX = _ds_pref_load(agent_config, username,
                      _ds_read_attribute(agent_config, "Home"), ATX->dbh);

  if (!UTX && _ds_match_attribute(agent_config, "FallbackDomains", "on")) {
    if (username != NULL && strchr(username, '@')) {
      char *domain = strchr(username, '@');
      if (domain) {
        UTX = _ds_pref_load(agent_config,
                            domain,
                            _ds_read_attribute(agent_config, "Home"), ATX->dbh);
        if (UTX && !strcmp(_ds_pref_val(UTX, "fallbackDomain"), "on")) {
          LOGDEBUG("empty prefs found. falling back to %s", domain);
        } else {
          _ds_pref_free(UTX);
          UTX = NULL;
        }
      }
    } else {
      LOG(LOG_ERR, "load_aggregated_prefs(): Can not fallback to domains for username '%s' without @domain part.", username);
    }
  }

  if (!UTX) {
    UTX = _ds_pref_load(agent_config, NULL,
                        _ds_read_attribute(agent_config, "Home"), ATX->dbh);
  }

  STX =  _ds_pref_load(agent_config, NULL,
                        _ds_read_attribute(agent_config, "Home"), ATX->dbh);

  if (!STX || STX[0] == 0) {
    if (STX) {
      _ds_pref_free(STX);
    }
    LOGDEBUG("default preferences empty. reverting to dspam.conf preferences.");
    STX = pref_config();
  } else {
    LOGDEBUG("loaded default preferences externally");
  }

  PTX = _ds_pref_aggregate(STX, UTX);
  _ds_pref_free(UTX);
  free(UTX);
  UTX = NULL;
  _ds_pref_free(STX);
  free(STX);
  STX = NULL;

#ifdef VERBOSE
  if (PTX) {
    int j;
    for(j=0;PTX[j];j++) {
      LOGDEBUG("aggregated preference '%s' => '%s'",
               PTX[j]->attribute, PTX[j]->value);
    }
  }
#endif

  return PTX;
}

/*
 * do_notifications(DSPAM_CTX *CTX, AGENT_CTX *ATX)
 *
 * DESCRIPTION
 *   Evaluate and send notifications as necessary
 *
 * INPUT ARGUMENTS
 *   CTX          DSPAM context
 *   ATX          Agent context defining processing behavior
 *
 * RETURN VALUES
 *   returns 0 on success, standard errors in failure
 */

int do_notifications(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;

  /* First run notification */

  if ((_ds_match_attribute(agent_config, "Notifications", "on") ||
      !strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "on")) &&
      strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "off")) {
    _ds_userdir_path(filename,
                    _ds_read_attribute(agent_config, "Home"),
                    LOOKUP(ATX->PTX, CTX->username), "firstrun");
    file = fopen(filename, "r");
    if (file == NULL) {
      LOGDEBUG("sending firstrun.txt to %s (%s): %s",
               CTX->username, filename, strerror(errno));
      send_notice(ATX, "firstrun.txt", ATX->mailer_args, CTX->username);
      _ds_prepare_path_for(filename);
      file = fopen(filename, "w");
      if (file) {
        fprintf(file, "%ld\n", (long) time(NULL));
        fclose(file);
      }
    } else {
      fclose(file);
    }
  }


  /* First spam notification */

  if (CTX->result == DSR_ISSPAM &&
       (_ds_match_attribute(agent_config, "Notifications", "on") ||
        !strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "on")) &&
       strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "off"))
  {
    _ds_userdir_path(filename,
                    _ds_read_attribute(agent_config, "Home"),
                    LOOKUP(ATX->PTX, CTX->username), "firstspam");
    file = fopen(filename, "r");
    if (file == NULL) {
      LOGDEBUG("sending firstspam.txt to %s (%s): %s",
               CTX->username, filename, strerror(errno));
      send_notice(ATX, "firstspam.txt", ATX->mailer_args, CTX->username);
      _ds_prepare_path_for(filename);
      file = fopen(filename, "w");
      if (file) {
        fprintf(file, "%ld\n", (long) time(NULL));
        fclose(file);
      }
    } else {
      fclose(file);
    }
  }

  /* Quarantine size notification */

  if ((_ds_match_attribute(agent_config, "Notifications", "on") ||
      !strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "on")) &&
      strcasecmp(_ds_pref_val(ATX->PTX, "notifications"), "off")) {
    struct stat s;
    char qfile[MAX_FILENAME_LENGTH];
    int qwarn_size = 1024*1024*2;

    if (_ds_read_attribute(agent_config, "QuarantineWarnSize")) {
      qwarn_size = atoi(_ds_read_attribute(agent_config, "QuarantineWarnSize"));
      if (qwarn_size == INT_MAX && errno == ERANGE) {
        LOG (LOG_INFO, "Value for 'QuarantineWarnSize' not valid (will use 2MB for now)");
        qwarn_size = 1024*1024*2;
      }
    }

    _ds_userdir_path(qfile, _ds_read_attribute(agent_config, "Home"),
                     LOOKUP(ATX->PTX, CTX->username), "mbox");

    if (!stat(qfile, &s) && s.st_size > qwarn_size) {
      _ds_userdir_path(qfile, _ds_read_attribute(agent_config, "Home"),
                       LOOKUP(ATX->PTX, CTX->username), "mboxwarn");
      if (stat(qfile, &s)) {
        FILE *f;

        _ds_prepare_path_for(qfile);
        f = fopen(qfile, "w");
        if (f != NULL) {
          fprintf(f, "%ld", (long) time(NULL));
          fclose(f);

          send_notice(ATX, "quarantinefull.txt", ATX->mailer_args, CTX->username);
        }
      }
    }
  }

  return 0;
}
