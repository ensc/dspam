/* $Id: dspam.c,v 1.67 2005/01/17 21:00:42 jonz Exp $ */

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
#include <errno.h>
#ifdef HAVE_UNISTD_H_
#include <unistd.h>
#include <pwd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <netdb.h>
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

static double timestart;

static double gettime()
{
  double t;

#ifdef _WIN32
  t = GetTickCount()/1000.;
#else /* !_WIN32 */
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != -1 )
    t = tv.tv_usec/1000000.0 + tv.tv_sec;
  else
    t = 0.;
#endif /* _WIN32/!_WIN32 */

  return t;
}

int
main (int argc, char *argv[])
{
  AGENT_CTX ATX;		/* Agent configuration */
  buffer *message = NULL;       /* Input Message */
  int exitcode = EXIT_SUCCESS;
  int **results;
  int agent_init = 0;		/* Agent is initialized */
  int driver_init = 0;
  int i = 0;

  timestart = gettime();	/* Set tick count to calculate run time */
  srand (getpid ());		/* Random numbers for signature creation */
  umask (006);                  /* rw-rw---- */
  setbuf (stdout, NULL);	/* Unbuffered output */
#ifdef DEBUG
  DO_DEBUG = 0;
#endif

  /* Read dspam.conf */
  agent_config = read_config(NULL);
  if (!agent_config) {
    report_error(ERROR_READ_CONFIG);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

  if (!_ds_read_attribute(agent_config, "Home")) {
    report_error(ERROR_DSPAM_HOME);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

  /* Set up our agent configuration */
  if (initialize_atx(&ATX)) {
    report_error(ERROR_INITIALIZE_ATX);
    exitcode = EXIT_FAILURE;
    goto bail;
  } else {
    agent_init = 1;
  }

  /* Parse commandline arguments */ 
  if (process_arguments(&ATX, argc, argv)) {
    report_error(ERROR_INITIALIZE_ATX);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

#ifdef DAEMON
#ifdef TRUSTED_USER_SECURITY
  if (ATX.operating_mode == DSM_DAEMON && ATX.trusted) {
#else
  if (ATX.operating_mode == DSM_DAEMON) {
#endif
    daemon_start(&ATX);

    libdspam_shutdown();
    if (agent_init)
      nt_destroy(ATX.users);

    if (agent_config)
      _ds_destroy_attributes(agent_config);

    exit(EXIT_SUCCESS);
  }
 
#endif

  /* Set defaults if an option wasn't specified on the commandline */
  if (apply_defaults(&ATX)) {
    report_error(ERROR_INITIALIZE_ATX);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

  /* Sanity check the configuration before proceeding */
  if (check_configuration(&ATX)) {
    report_error(ERROR_DSPAM_MISCONFIGURED);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

  message = read_stdin(&ATX);
  if (message == NULL) {
    exitcode = EXIT_FAILURE;
    goto bail;
  }

  if (ATX.users->items == 0)
  {
    LOG (LOG_ERR, ERROR_USER_UNDEFINED);
    report_error (ERROR_USER_UNDEFINED);
    fprintf (stderr, "%s\n", SYNTAX);
    exitcode = EXIT_FAILURE;
    goto bail;
  }

#ifdef DAEMON
  if (ATX.client_mode &&
      _ds_read_attribute(agent_config, "ClientIdent") &&
      (_ds_read_attribute(agent_config, "ClientHost") ||
       _ds_read_attribute(agent_config, "ServerDomainSocketPath")))
  {
    exitcode = client_process(&ATX, message);
    if (exitcode)
      report_error_printf(ERROR_CLIENT_EXIT, exitcode);
  } else {
#endif
    libdspam_init();
 
    if (dspam_init_driver (NULL))
    {
      LOG (LOG_WARNING, "unable to initialize storage driver");
      exitcode = EXIT_FAILURE;
      goto bail;
    } else {
      driver_init = 1;
    }
  
    /* Process the message once for each destination user */
    results = process_users(&ATX, message);
    exitcode = *results[ATX.users->items];
    for(i=0;i<=ATX.users->items;i++) {
      if (results[i] != NULL) 
        free(results[i]);
    }
    free(results);
#ifdef DAEMON
  }
#endif

bail:

  if (message)
    buffer_destroy(message);

  if (agent_init)
    nt_destroy(ATX.users);

  if (!_ds_read_attribute(agent_config, "ClientHost")) {
    if (driver_init)
      dspam_shutdown_driver (NULL);
    libdspam_shutdown();
  }

  if (agent_config)
    _ds_destroy_attributes(agent_config);

  exit (exitcode);
}

/*
   process_message: calls the dspam processor for each destination user. 
                    manages signatures.
           returns: DSR_ISINNOCENT    - Message is innocent
                    DSR_ISSPAM        - Message is spam
                    DSR_ISWHITELISTED - Message is whitelisted
                    (other)           - Error
*/

int
process_message (AGENT_CTX *ATX, 
                 agent_pref_t PTX, 
                 buffer * message, 
                 const char *username)
{
  DSPAM_CTX *CTX = NULL;		/* dspam context */
  struct _ds_message *components;

  char *copyback;
  int have_signature = 0;
  int have_decision = 0;
  int result, i;

  /* Create a DSPAM context based on the agent context */
  CTX = ctx_init(ATX, PTX, username);
  if (CTX == NULL)
  {
    LOG (LOG_WARNING, "unable to create dspam context");
    result = EUNKNOWN;
    goto RETURN;
  }

  /* Pass any libdspam preferences through the API, then attach storage */
  set_libdspam_attributes(CTX);
  if (ATX->sockfd && ATX->dbh == NULL) 
    ATX->dbh = _ds_connect(CTX);

  if (attach_context(CTX, ATX->dbh)) {

    /* Try again */
    if (ATX->sockfd) {
      ATX->dbh = _ds_connect(CTX);

      if (attach_context(CTX, ATX->dbh)) {
        LOGDEBUG("unable to attach dspam context");
        result = EUNKNOWN;
        goto RETURN;
      }
    } else {
      LOGDEBUG("unable to attach dspam context");
      result = EUNKNOWN;
      goto RETURN;
    }
  }

  /* First Run Message */
  if (! CTX->totals.innocent_learned && ! CTX->totals.spam_learned &&
      _ds_match_attribute(agent_config, "Notifications", "on")) { 
    send_notice("firstrun.txt", ATX->mailer_args, username);
  }

  /* Decode the message into a series of structures for easy processing */
  if (message->data == NULL) {
    LOGDEBUG("Message provided is NULL");
    return EINVAL;
  }

  components = _ds_actualize_message (message->data);
  if (components == NULL)
  {
    LOG (LOG_WARNING, "_ds_actualize_message() failed.  unable to process.");
    result = EUNKNOWN;
    goto RETURN;
  }

  CTX->message = components;

  if (CTX->classification == DSR_NONE &&
      _ds_read_attribute(agent_config, "Lookup"))
  {
    int bad;
    char ip[32];

    if (!dspam_getsource (CTX, ip, sizeof (ip))) {
      bad = is_blacklisted(ip);
      if (bad) {
        LOGDEBUG("source address is blacklisted. learning as spam.");
        CTX->classification = DSR_ISSPAM;
        CTX->source = DSS_CORPUS;
      }
    }
  }

  /* If a signature was provided, load it */
  have_signature = find_signature(CTX, ATX, PTX);
  if (have_signature)
  {
    have_decision = 1;

    if (_ds_get_signature (CTX, &ATX->SIG, ATX->signature))
    {
      LOGDEBUG ("signature retrieval for '%s' failed", ATX->signature);
      have_signature = 0;
    }
    else
    {
      CTX->signature = &ATX->SIG;
    }
#ifdef NEURAL
    if (_ds_get_decision (CTX, &ATX->DEC, ATX->signature))
      have_decision = 0;
#endif
  } else if (CTX->operating_mode == DSM_CLASSIFY || 
             CTX->classification != DSR_NONE)
  {
    CTX->flags = CTX->flags ^ DSF_SIGNATURE;
    CTX->signature = NULL;
  }

  /* Set neural node reliability based on which nodes classified the message
     correctly and incorrectly */
#ifdef NEURAL
  if (have_decision                   &&
      CTX->classification != DSR_NONE && 
      CTX->source == DSS_ERROR)
  {
    process_neural_decision(CTX, &ATX->DEC);
  }
#endif

  /* Classify or retrain the message */
  if (have_signature && CTX->classification != DSR_NONE) {
    retrain_message(CTX, ATX);
  } else {
    CTX->signature = NULL;
    if (!_ds_match_attribute(agent_config, "TrainPristine", "on") && 
        strcmp(_ds_pref_val(PTX, "trainPristine"), "on")) {
      if (CTX->classification != DSR_NONE && CTX->source == DSS_ERROR) {
        LOGDEBUG("unable to find signature; bailing.");
        result = EFAILURE;
        goto RETURN;
      }
    }
    result = dspam_process (CTX, message->data);
  }

  result = CTX->result;

  /* First Spam Message */
  if (result == DSR_ISSPAM &&
        CTX->totals.spam_learned == 1 &&
        CTX->totals.spam_misclassified == 0 && 
       _ds_match_attribute(agent_config, "Notifications", "on")) 
  {
    send_notice("firstspam.txt", ATX->mailer_args, username);
  }

  /* Quarantine Size Check */
  if (_ds_match_attribute(agent_config, "Notifications", "on")) {
    struct stat s;
    char qfile[MAX_FILENAME_LENGTH];

    _ds_userdir_path(qfile, _ds_read_attribute(agent_config, "Home"), 
                     username, "mbox");

    if (!stat(qfile, &s) && s.st_size > 1024*1024*2) {
      _ds_userdir_path(qfile, _ds_read_attribute(agent_config, "Home"), 
                       username, "mboxwarn");
      if (stat(qfile, &s)) {
        FILE *f;

        f = fopen(qfile, "w");
        if (f != NULL) {
          fprintf(f, "%ld", (long) time(NULL));
          fclose(f);

          send_notice("quarantinefull.txt", ATX->mailer_args, username);
        }
      }
    }    
  }

  if (result != DSR_ISWHITELISTED)
    result = ensure_confident_result(CTX, ATX, result);
  if (result<0) 
   goto RETURN;

  /* Inoculate other users: Signature */
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
      inoculate_user ((const char *) node_int->ptr, &ATX->SIG, NULL, ATX);
      node_int = c_nt_next (ATX->inoc_users, &c_i);
    }
  }

  /* Inoculate other users: Message */
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
      inoculate_user ((const char *) node_int->ptr, NULL, message->data, ATX);
      node_int = c_nt_next (ATX->inoc_users, &c_i);
    }
    inoculate_user (username, NULL, message->data, ATX);
    result = DSR_ISSPAM;
    CTX->result = DSR_ISSPAM;
    
    goto RETURN;
  }

  /* Save the DSPAM signature */
  if (CTX->operating_mode == DSM_PROCESS &&
      CTX->classification == DSR_NONE    &&
      CTX->signature != NULL)
  {
    int valid = 0;

    while (valid == 0)
    {
      _ds_create_signature_id (CTX, ATX->signature, sizeof (ATX->signature));
      if (_ds_verify_signature (CTX, ATX->signature))
          valid = 1;
    }

    LOGDEBUG ("saving signature as %s", ATX->signature);

    if (CTX->classification == DSR_NONE && CTX->training_mode != DST_NOTRAIN)
    {
      if (!_ds_match_attribute(agent_config, "TrainPristine", "on") && 
          strcmp(_ds_pref_val(PTX, "trainPristine"), "on")) {

        int x = _ds_set_signature (CTX, CTX->signature, ATX->signature);
        if (x) {
          LOGDEBUG("_ds_set_signature() failed with error %d", x);
        }
      }
#ifdef NEURAL
      if (ATX->DEC.length != 0)
        _ds_set_decision(CTX, &ATX->DEC, ATX->signature);
#endif
      }
  }

  /* Write .stats file for CGI */
  if (CTX->training_mode != DST_NOTRAIN) {
    write_web_stats (
      (CTX->group == NULL || CTX->flags & DSF_MERGED) ?  username : CTX->group, 
      (CTX->group != NULL && CTX->flags & DSF_MERGED) ? CTX->group: NULL,
      &CTX->totals);
  }

  LOGDEBUG ("libdspam returned probability of %f", CTX->probability);
  LOGDEBUG ("message result: %s", (result != DSR_ISSPAM) ? "NOT SPAM" : "SPAM");

  /* System and User Logging */

  if (_ds_match_attribute(agent_config, "SystemLog", "on") ||
      _ds_match_attribute(agent_config, "UserLog", "on"))
  {
    log_events(CTX);
  }

  if (PTX != NULL && !strcmp(_ds_pref_val(PTX, "makeCorpus"), "on")) {
    if (ATX->source != DSS_ERROR) {
      char dirname[MAX_FILENAME_LENGTH];
      char corpusfile[MAX_FILENAME_LENGTH];
      FILE *file;

      _ds_userdir_path(dirname, _ds_read_attribute(agent_config, "Home"),
                   CTX->username, "corpus");
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
                   CTX->username, "corpus");
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

  /* FP and SM can return */
  if (CTX->message == NULL)
    goto RETURN;

  if (!_ds_match_attribute(agent_config, "TrainPristine", "on") && 
        strcmp(_ds_pref_val(PTX, "trainPristine"), "on"))
    add_xdspam_headers(CTX, ATX, PTX);

  if (!strcmp(_ds_pref_val(PTX, "spamAction"), "tag") && 
      result == DSR_ISSPAM)
  {
    tag_message((struct _ds_message_block *) CTX->message->components->first->ptr, PTX);
  }

  if (strcmp(_ds_pref_val(PTX, "signatureLocation"), "headers") &&
      !_ds_match_attribute(agent_config, "TrainPristine", "on") &&
        strcmp(_ds_pref_val(PTX, "trainPristine"), "on"))
  {
    i = embed_signature(CTX, ATX, PTX);
    if (i<0) {
      result = i; 
      goto RETURN;
    }
  }
  
  /* Reconstruct message from components */
  copyback = _ds_assemble_message (CTX->message);
  buffer_clear (message);
  buffer_cat (message, copyback);
  free (copyback);

  /* Track source address and report to syslog, SBL */
  if ( _ds_read_attribute(agent_config, "TrackSources") &&
       CTX->operating_mode == DSM_PROCESS               &&
       CTX->source != DSS_CORPUS)
  {
    tracksource(CTX);
  }

  if (CTX->operating_mode == DSM_CLASSIFY || ATX->flags & DAF_SUMMARY)
  {
    char data[128];
    FILE *fout;

    switch (CTX->result) {
      case DSR_ISSPAM:
        strcpy(data, "Spam");
        break;
      case DSR_ISWHITELISTED:
        strcpy(data, "Whitelisted");
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

    fprintf(fout, "X-DSPAM-Result: %s; result=\"%s\"; probability=%01.4f; "
           "confidence=%02.2f\n",
           CTX->username,
           data,
           CTX->probability,
           CTX->confidence);

  }

  ATX->learned = CTX->learned;

RETURN:
  if (have_signature)
    free(ATX->SIG.data);
  nt_destroy (ATX->inoc_users);
  nt_destroy (ATX->classify_users);
  if (CTX)
    dspam_destroy (CTX);
  return result;
}

/*
    deliver_message: delivers the message to the delivery agent
    message	message to be delivered
    mailer_args	args to pass to the delivery agent
    username	username of the user being processed
*/

int
deliver_message (const char *message, const char *mailer_args,
                 const char *username, FILE *stream)
{
  char args[1024];
  char *margs, *mmargs, *arg;
  FILE *file;
  int rc;

  if (message == NULL)
    return EINVAL;

  if (mailer_args == NULL)
  {
    fputs (message, stream);
    return 0;
  }

  args[0] = 0;
  margs = strdup (mailer_args);
  mmargs = margs;
  arg = strsep (&margs, " ");
  while (arg != NULL)
  {
    char a[256], b[256];
    size_t i;
 
    if (!strcmp (arg, "$u") || !strcmp (arg, "\\$u") || !strcmp (arg, "%u"))
      strlcpy(a, username, sizeof(a));
    else
      strlcpy(a, arg, sizeof(a));

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

    if (arg != NULL) {
      strlcat (args, a, sizeof(args));
    }
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
    file_error ("Error opening pipe to LDA", args, strerror (errno));
    return EFILE;
  }

  fputs (message, file);
  rc = pclose (file);
  if (rc == -1)
  {
    file_error ("Error obtaining exit status of LDA", args, strerror (errno));
    return EFILE;
  }
  else if (WIFEXITED (rc))
  {
    int lda_exit_code;
    lda_exit_code = WEXITSTATUS (rc);
    if (lda_exit_code == 0)
    {
      LOGDEBUG ("LDA returned success");
    }
    else
    {
      report_error_printf (ERROR_AGENT_RETURN, lda_exit_code, args);
      return lda_exit_code;
    }
  }
#ifndef _WIN32
  else if (WIFSIGNALED (rc))
  {
    int sig;
    sig = WTERMSIG (rc);
    report_error_printf (ERROR_AGENT_SIGNAL, sig, args);
    return sig;
  }
  else
  {
    report_error_printf (ERROR_AGENT_CLOSE, rc);
    return rc;
  }
#endif

  return 0;
}

/*
    tag_message: tags a message's subject line as spam
    block	message block to tag
    PTX		preferences
*/

int tag_message(struct _ds_message_block *block, agent_pref_t PTX)
{
  struct nt_node *node_header = block->headers->first; 
  char spam_subject[16];
  int tagged = 0;

  strlcpy(spam_subject, "[SPAM]", sizeof(spam_subject));
  if (_ds_pref_val(PTX, "spamSubject")[0] != '\n' &&
      _ds_pref_val(PTX, "spamSubject")[0] != 0)
  {
    strlcpy(spam_subject, _ds_pref_val(PTX, "spamSubject"), 
            sizeof(spam_subject));
  }

  /* We'll only scan the first (i.e. main) header of the message. */
  while (node_header != NULL) 
  {
    struct _ds_header_field *head;

    head = (struct _ds_header_field *) node_header->ptr;
    if (head->heading && 
        !strcasecmp(head->heading, "Subject")) 
    {
      tagged = 1;

      /* Is this header already tagged ? */
      if (strncmp(head->data, spam_subject, strlen(spam_subject))) 
      {
        /* Not tagged => tag it. */
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

      /* Is this header already tagged ? */
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
    }
    node_header = node_header->next;
  }

  if (!tagged) 
  {
    char text[512];
    struct _ds_header_field *header;
    snprintf(text,
             sizeof(text),
             "Subject: %s",
             spam_subject);
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
    quarantine_message: place a message in the user's quarantine
    message	message to quarantine
    username	user being processed
*/

int
quarantine_message (const char *message, const char *username)
{
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;
  int line = 1;
  char *x, *msg, *omsg;
  int i;

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"), 
                   username, "mbox");
  _ds_prepare_path_for(filename);
  file = fopen (filename, "a");
  if (file == NULL)
  {
    file_error (ERROR_FILE_WRITE, filename, strerror (errno));
    return EFILE;
  }

  i = _ds_get_fcntl_lock(fileno(file));
  if (i) {
    LOG(LOG_WARNING, "Failed to lock %s: Error %d: %s\n", filename, i, strerror(errno));
    return EFILE;
  }

  /* From header */
  if (strncmp (message, "From ", 5))
  {
    char head[128];
    time_t tm = time (NULL);

    snprintf (head, sizeof (head), "From QUARANTINE %s", ctime (&tm));
    fputs (head, file);
  }

  msg = strdup(message);
  omsg = msg;

  if (msg == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  x = strsep (&msg, "\n");
  while (x != NULL)
  {
    if (!strncmp (x, "From ", 5) && line != 1)
      fputs (">", file);
    fputs (x, file);
    fputs ("\n", file);
    line++;
    x = strsep (&msg, "\n");
  }
  fputs ("\n\n", file);

  _ds_free_fcntl_lock(fileno(file));
  fclose (file);

  free (omsg);
  return 0;
}

/* 
    write_web_stats: writes a .stats file to user.stats for CGI
    username	user being processed
    group	name of shared group to create .stats file within
    totals	user's processing totals
*/

int
write_web_stats (
  const char *username, 
  const char *group, 
  struct _ds_spam_totals *totals)
{
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;

  if (totals == NULL)
  {
    LOGDEBUG ("totals are null\n");
    return EINVAL;
  }

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"), username, "stats");
  _ds_prepare_path_for (filename);
  file = fopen (filename, "w");
  if (file == NULL)
  {
    file_error (ERROR_FILE_WRITE, filename, strerror (errno));
    return EFILE;
  }

  fprintf (file, "%ld,%ld,%ld,%ld,%ld,%ld\n",
           (totals->spam_learned + totals->spam_classified) - 
             (totals->spam_misclassified + totals->spam_corpusfed),
           (totals->innocent_learned + totals->innocent_classified) -
             (totals->innocent_misclassified + totals->innocent_corpusfed),
           totals->spam_misclassified, totals->innocent_misclassified,
           totals->spam_corpusfed, totals->innocent_corpusfed);

  if (group != NULL) 
    fprintf(file, "%s\n", group);
  
  fclose (file);
  return 0;
}

/* function: inoculate_user
   parameters: only SIG _OR_ message should be passed.  the other
               should be left NULL depending on whether we have
               a signature or a message
*/

int
inoculate_user (const char *username, struct _ds_spam_signature *SIG,
                const char *message, AGENT_CTX *ATX)
{
  DSPAM_CTX *INOC;
  int do_inoc = 1, result = 0;
  int f_all = 0;

  LOGDEBUG ("checking to see if user %s requires this inoculation", username);

  /* First see if the user needs to be inoculated */
  if (ATX->flags & DAF_CHAINED)
    f_all |= DSF_CHAINED;
                                                                                
  if (ATX->flags & DAF_NOISE)
    f_all |= DSF_NOISE;
                                                                                
  if (ATX->flags & DAF_SBPH)
    f_all |= DSF_SBPH;


  INOC = dspam_create (username, 
                     NULL, 
                     _ds_read_attribute(agent_config, "Home"), 
                     DSM_CLASSIFY, 
                     f_all);
  if (INOC)
  {
    set_libdspam_attributes(INOC);
    if (attach_context(INOC, ATX->dbh)) {
      LOG (LOG_WARNING, "unable to attach dspam context");
      dspam_destroy(INOC);
      return EUNKNOWN;
    }

    if (SIG)
    {
      INOC->flags |= DSF_SIGNATURE;
      INOC->signature = SIG;
      result = dspam_process (INOC, NULL);
    }
    else
    {
      INOC->signature = NULL;
      result = dspam_process (INOC, message);
    }

    if (!result && INOC->result == DSR_ISSPAM)
      do_inoc = 0;

    if (SIG)
      INOC->signature = NULL;
    dspam_destroy (INOC);
  }

  if (!do_inoc)
  {
    LOGDEBUG ("skipping user %s: doesn't require inoculation", username);
    return EFAILURE;
  }
  else
  {
    LOGDEBUG ("inoculating user %s", username);

    if (ATX->flags & DAF_CHAINED)
      f_all |= DSF_CHAINED;
                                                                                
    if (ATX->flags & DAF_NOISE)
      f_all |= DSF_NOISE;
                                                                                
    if (ATX->flags & DAF_SBPH)
      f_all |= DSF_SBPH;

    INOC = dspam_create (username, 
                       NULL, 
                       _ds_read_attribute(agent_config, "Home"), 
                       DSM_PROCESS, 
                       f_all);
    if (INOC)
    {
      set_libdspam_attributes(INOC);
      if (attach_context(INOC, ATX->dbh)) {
        LOG (LOG_WARNING, "unable to attach dspam context");
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

/* user_classify: classifies message for another user
   parameters: only SIG _OR_ message should be passed.  the other
               should be left NULL depending on whether we have
               a signature or a message
*/


int
user_classify (const char *username, struct _ds_spam_signature *SIG,
               const char *message, AGENT_CTX *ATX)
{
  DSPAM_CTX *CLX;
  int result = 0;
  int f_all = 0;

  if (ATX->flags & DAF_CHAINED)
    f_all |= DSF_CHAINED;
                                                                                
  if (ATX->flags & DAF_NOISE)
    f_all |= DSF_NOISE;
                                                                                
  if (ATX->flags & DAF_SBPH)
    f_all |= DSF_SBPH;

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
      LOG (LOG_WARNING, "unable to attach dspam context");
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
      result = dspam_process (CLX, message);
    }

    if (SIG)
      CLX->signature = NULL;

    if (result)
    {
      LOGDEBUG ("user_classify() returned error %d", result);
      result = 0;
    }
    else
      result = CLX->result;

    dspam_destroy (CLX);
  }

  return result;
}

/*
    send_notice: deliver a notification to the user's mailbox
    filename	filename of notification
    mailer_args	arguments to pass to delivery agent
    username	user being processed
*/
	
int send_notice(const char *filename, const char *mailer_args, 
                const char *username) {
  FILE *f;
  char msgfile[MAX_FILENAME_LENGTH];
  buffer *b;
  char buff[1024];
  time_t now;

  time(&now);
                                                                                
  snprintf(msgfile, sizeof(msgfile), "%s/txt/%s", _ds_read_attribute(agent_config, "Home"), filename);
  f = fopen(msgfile, "r");
  if (f == NULL) 
    return EFILE;

  b = buffer_create(NULL);
  if (b == NULL) {
    report_error(ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  strftime(buff,sizeof(buff), "Date: %a, %d %b %Y %H:%M:%S %z\n",
     localtime(&now));
  buffer_cat(b, buff);

  while(fgets(buff, sizeof(buff), f)!=NULL) {
    char *s = buff;
    char *w = strstr(buff, "$u");
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
  deliver_message(b->data, mailer_args, username, stdout);

  buffer_destroy(b);

  return 0;
}

/*
    process_users: cycle through each user and process
    ATX		agent context containing userlist
    message	message to process for each user
*/

int **process_users(AGENT_CTX *ATX, buffer *message) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  int retcode, exitcode = EXIT_SUCCESS;
  int **x = malloc(sizeof(int *)*(ATX->users->items+1));
  int i = 0;
  int *code;
  FILE *fout;
  buffer *parse_message;

  if (x == NULL)
    return NULL;

  if (ATX->sockfd) {
    fout = ATX->sockfd;
  } else {
    fout = stdout;
  }

  /* Process message for each user */
  node_nt = c_nt_first (ATX->users, &c_nt);
  while (node_nt != NULL)
  {
    agent_pref_t PTX = NULL;
    agent_pref_t STX = NULL;
    agent_pref_t UTX = NULL;
    struct stat s;
    char filename[MAX_FILENAME_LENGTH];
    int result, optin, optout;

    parse_message = buffer_create(message->data);
    if (parse_message == NULL) {
      LOG(LOG_CRIT, ERROR_MEM_ALLOC);
      x[i] = NULL;
      i++;
      continue;
    }

    x[i] = NULL;


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
            ATX->source == -1 &&
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

    LOGDEBUG("Loading preferences for user %s", (const char *) node_nt->ptr);
    UTX = _ds_pref_load(agent_config, 
                        node_nt->ptr, 
                        _ds_read_attribute(agent_config, "Home"), ATX->dbh);

    STX = pref_config();

    PTX = _ds_pref_aggregate(STX, UTX);
    _ds_pref_free(UTX);
    free(UTX);
    _ds_pref_free(STX);
    free(STX);

#ifdef VERBOSE
    if (PTX) {
      int j;
      for(j=0;PTX[j];j++) {
        LOGDEBUG("Aggregated Preference '%s' => '%s'", PTX[j]->attribute, PTX[j]->value);
      }
    }
#endif

    _ds_userdir_path(filename, 
                     _ds_read_attribute(agent_config, "Home"), 
                     node_nt->ptr, "dspam");
    optin = stat(filename, &s);

#ifdef HOMEDIR
    if (!optin && (!S_ISDIR(s.st_mode))) {
      optin = -1;
      LOG(LOG_WARNING, "Opt-in file %s is not a directory", filename);
    }
#endif

    _ds_userdir_path(filename, 
                     _ds_read_attribute(agent_config, "Home"), 
                     node_nt->ptr, "nodspam");
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

    if (!optout ||
        !strcmp(_ds_pref_val(PTX, "optOut"), "on") ||

        (_ds_match_attribute(agent_config, "Opt", "in") &&
        optin && strcmp(_ds_pref_val(PTX, "optIn"), "on"))
    )
    {
      if (ATX->flags & DAF_DELIVER_INNOCENT)
      {
        retcode =
          deliver_message (parse_message->data,
                           (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
                            node_nt->ptr, fout);
        if (retcode && exitcode == EXIT_SUCCESS)
          exitcode = retcode;
        if (ATX->sockfd && ATX->flags & DAF_STDOUT)
          ATX->sockfd_output = 1;
      }
    }

    /* Process/Classify Message */

    else
    {
      result = process_message (ATX, PTX, parse_message, node_nt->ptr);
      if (_ds_match_attribute(agent_config, "Broken", "returnCodes")) {
        if (result == DSR_ISSPAM)
          exitcode = 99;
      }
      code = malloc(sizeof(int));
      *code = result;
      x[i] = code;

      /* Classify Only */

      if (ATX->operating_mode == DSM_CLASSIFY) 
      {
        node_nt = c_nt_next (ATX->users, &c_nt);
        _ds_pref_free(PTX);
        free(PTX);
        buffer_destroy(parse_message);
        i++;
        continue;
      }

      /* Classify and Process */

      /* Innocent */

      if (result != DSR_ISSPAM)
      {
        /* Processing Error */

        if (result != DSR_ISINNOCENT && result != DSR_ISWHITELISTED)
        {
          LOG (LOG_WARNING,
               "process_message returned error %d.  delivering message.",
               result);
        }

        /* Deliver */
        if (ATX->flags & DAF_DELIVER_INNOCENT) {
          LOGDEBUG ("delivering message");
          retcode = deliver_message
            (parse_message->data,
             (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
             node_nt->ptr, fout);
          if (ATX->sockfd && ATX->flags & DAF_STDOUT)
            ATX->sockfd_output = 1;
          if (retcode) {
            exitcode = retcode;
            if ((result == DSR_ISINNOCENT || result == DSR_ISWHITELISTED) && 
                _ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                ATX->learned)
            {
              ATX->classification =
                (result == DSR_ISWHITELISTED) ? DSR_ISINNOCENT : result;
              ATX->source = DSS_ERROR;
              ATX->flags |= DAF_UNLEARN;
              process_message (ATX, PTX, parse_message, node_nt->ptr);
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
                 (PTX != NULL && 
                   ( !strcmp(_ds_pref_val(PTX, "spamAction"), "tag") ||
                     !strcmp(_ds_pref_val(PTX, "spamAction"), "deliver") )
                 )
               )
            {
              if (ATX->classification == -1) {
                if (ATX->spam_args[0] != 0) {
                  retcode = deliver_message
                    (parse_message->data,
                     (ATX->flags & DAF_STDOUT) ? NULL : ATX->spam_args, 
                     node_nt->ptr, fout);
                  if (ATX->sockfd && ATX->flags & DAF_STDOUT)
                    ATX->sockfd_output = 1;
                } else {
                  retcode = deliver_message
                    (parse_message->data,
                     (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
                     node_nt->ptr, fout);
                  if (ATX->sockfd && ATX->flags & DAF_STDOUT)
                    ATX->sockfd_output = 1;
                }
              }
            }
            else
            {
              /* Use standard quarantine procedure */
              if (ATX->source == DSS_INOCULATION || ATX->classification == -1) {
                if (ATX->managed_group[0] == 0)
                  retcode = quarantine_message (parse_message->data, node_nt->ptr);
                else
                  retcode = quarantine_message (parse_message->data, ATX->managed_group);
              }
            }

            if (retcode) {
              exitcode = retcode;
                                                                                
              /* Unlearn the message on a local delivery failure */
              if (_ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                  ATX->learned) {
                ATX->classification =
                  (result == DSR_ISWHITELISTED) ? DSR_ISINNOCENT : result;
                ATX->source = DSS_ERROR;
                ATX->flags |= DAF_UNLEARN;
                process_message (ATX, PTX, parse_message, node_nt->ptr);
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
            (parse_message->data,
             (ATX->flags & DAF_STDOUT) ? NULL : ATX->mailer_args,
             node_nt->ptr, fout);
          if (retcode) {
            exitcode = retcode;
            if (_ds_match_attribute(agent_config, "OnFail", "unlearn") &&
                ATX->learned) {
              ATX->classification =
                (result == DSR_ISWHITELISTED) ? DSR_ISINNOCENT : result;
              ATX->source = DSS_ERROR;
              ATX->flags |= DAF_UNLEARN;
              process_message (ATX, PTX, parse_message, node_nt->ptr);
            }
          }
        }
      }
    }

    _ds_pref_free(PTX);
    free(PTX);
    node_nt = c_nt_next (ATX->users, &c_nt);

    LOGDEBUG ("DSPAM Instance Shutdown.  Exit Code: %d", exitcode);
    i++;
    buffer_destroy(parse_message);
  }

  code = malloc(sizeof(int));
  *code = exitcode;
  x[ATX->users->items] = code;
  return x;
}


/* find_signature: find, parse, and strip DSPAM signature */

int find_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX, agent_pref_t PTX) {
  struct nt_node *node_nt, *prev_node = NULL;
  struct nt_c c;
  struct _ds_message_block *block = NULL;
  char first_boundary[512];
  int is_signed = 0, i;
  char *signature_begin = NULL, *signature_end, *erase_begin;
  int signature_length, have_signature = 0;
  struct nt_node *node_header;
  struct nt_c c2;

  i = 0;
  first_boundary[0] = 0;

  if (ATX->signature[0] != 0) 
    return 1;

  /* Iterate through each message component in search of a signature
   * Decode components as necessary */

  node_nt = c_nt_first (CTX->message->components, &c);
  while (node_nt != NULL)
  {
    block = (struct _ds_message_block *) node_nt->ptr;

    if (block->media_type == MT_MULTIPART
        && block->media_subtype == MST_SIGNED)
      is_signed = 1;

    if (!strcmp(_ds_pref_val(PTX, "signatureLocation"), "headers"))
      is_signed = 2;

#ifdef VERBOSE
      LOGDEBUG ("Scanning component %d for a DSPAM signature", i);
#endif

    if (block->media_type == MT_TEXT
        || block->media_type == MT_MESSAGE 
        || block->media_type == MT_UNKNOWN ||
                (i == 0 && (block->media_type == MT_TEXT ||
                            block->media_type == MT_MULTIPART ||
                            block->media_type == MT_MESSAGE) ))
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
          struct _ds_header_field *header =
            (struct _ds_header_field *) node_header->ptr;
          LOGDEBUG ("    %-32s  %s", header->heading, header->data);
          node_header = c_nt_next (block->headers, &c2);
        }
      }
#endif

      body = block->body->data;
      if (block->encoding == EN_BASE64
          || block->encoding == EN_QUOTED_PRINTABLE)
      {
        struct _ds_header_field *field;
        int is_attachment = 0;
        struct nt_node *node_hnt;
        struct nt_c c_hnt;

        node_hnt = c_nt_first (block->headers, &c_hnt);
        while (node_hnt != NULL)
        {
          field = (struct _ds_header_field *) node_hnt->ptr;
          if (field != NULL
              && field->heading != NULL && field->data != NULL)
            if (!strncasecmp (field->heading, "Content-Disposition", 19))
              if (!strncasecmp (field->data, "attachment", 10))
                is_attachment = 1;
          node_hnt = c_nt_next (block->headers, &c_hnt);
        }

        if (!is_attachment)
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
            block->encoding = EN_7BIT;

            node_header = c_nt_first (block->headers, &c2);
            while (node_header != NULL)
            {
              struct _ds_header_field *header =
                (struct _ds_header_field *) node_header->ptr;
              if (!strcasecmp
                  (header->heading, "Content-Transfer-Encoding"))
              {
                free (header->data);
                header->data = strdup ("7bit");
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

      if (!strcmp(_ds_pref_val(PTX, "signatureLocation"), "headers")) {
        if (block->headers != NULL && !have_signature)
        {
          struct nt_node *node_header;
          struct _ds_header_field *head;

          node_header = block->headers->first;
          while(node_header != NULL) {
            head = (struct _ds_header_field *) node_header->ptr;
            if (head->heading && 
                !strcmp(head->heading, "X-DSPAM-Signature")) {
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

      if (!_ds_match_attribute(agent_config, "TrainPristine", "on") &&
        strcmp(_ds_pref_val(PTX, "trainPristine"), "on")) {
        /* Look for signature */
        if (body != NULL)
        {
          signature_begin = strstr (body, SIGNATURE_BEGIN);
          if (signature_begin == NULL)
            signature_begin = strstr (body, LOOSE_SIGNATURE_BEGIN);
 
          if (signature_begin != NULL)
          {
            erase_begin = signature_begin;
            if (!strncmp(signature_begin, SIGNATURE_BEGIN, strlen(SIGNATURE_BEGIN))) 
              signature_begin += strlen(SIGNATURE_BEGIN);
            else
              signature_begin =
                strstr (signature_begin,
                  SIGNATURE_DELIMITER) + strlen (SIGNATURE_DELIMITER);
                  signature_end = signature_begin;
  
            /* Find the signature's end character */
            while (signature_end != NULL
              && signature_end[0] != 0
              && (isalnum
                  ((int) signature_end[0]) || signature_end[0] == 32))
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

                if (strcmp(_ds_pref_val(PTX, "signatureLocation"), 
                    "headers")) {

                  if (!is_signed && ATX->classification == -1) {
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
    prev_node = node_nt;
    node_nt = c_nt_next (CTX->message->components, &c);
    i++;
  }
  return have_signature;
}

/* ctx_init: initialize a DSPAM context from an agent context */

DSPAM_CTX *ctx_init(AGENT_CTX *ATX, agent_pref_t PTX, const char *username) {
  DSPAM_CTX *CTX;
  char filename[MAX_FILENAME_LENGTH];
  char ctx_group[128] = { 0 };
  int f_all = 0, f_mode = DSM_PROCESS;
  FILE *file;

  ATX->inoc_users = nt_create (NT_CHAR);
  if (ATX->inoc_users == NULL)
  {
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  ATX->classify_users = nt_create (NT_CHAR);
  if (ATX->classify_users == NULL)
  {
    nt_destroy(ATX->inoc_users);
    LOG (LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  /* Set Group Membership */

  if (strcmp(_ds_pref_val(PTX, "ignoreGroups"), "on")) {
    snprintf (filename, sizeof (filename), "%s/group", 
              _ds_read_attribute(agent_config, "Home"));
    file = fopen (filename, "r");
    if (file != NULL)
    {
      char *group;
      char *user;
      char buffer[10240];
  
      while (fgets (buffer, sizeof (buffer), file) != NULL)
      {
        int do_inocgroups = 0;
        char *type, *list;
        chomp (buffer);

        if (buffer[0] == 0 || buffer[0] == '#' || buffer[0] == ';')
          continue;
       
        list = strdup (buffer);
        group = strtok (buffer, ":");

        if (group != NULL)
        {
          type = strtok (NULL, ":");
          user = strtok (NULL, ",");
  
          if (!type)
            continue;
  
          if (!strcasecmp (type, "INOCULATION") &&
              ATX->classification == DSR_ISSPAM &&
              ATX->source != DSS_CORPUS)
          {
            do_inocgroups = 1;
          }
  
          while (user != NULL)
          {
            if (!strcmp (user, username) || user[0] == '*' ||
               (user[0] == '@' && !strcmp(user, strchr(username,'@'))))
            {
  
              /* If we're reporting a spam, report it as a spam to all other
               * users in the inoculation group */
              if (do_inocgroups)
              {
                char *l = list, *u;
                u = strsep (&l, ":");
                u = strsep (&l, ":");
                u = strsep (&l, ",");
                while (u != NULL)
                {
                  if (strcmp (u, username))
                  {
                    LOGDEBUG ("adding user %s to inoculation group %s", u,
                              group);
                   if (u[0] == '*') {
                      nt_add (ATX->inoc_users, u+1);
                    } else
                      nt_add (ATX->inoc_users, u);
                  }
                  u = strsep (&l, ",");
                }
              }
              else if (!strncasecmp (type, "SHARED", 6)) 
              {
                strlcpy (ctx_group, group, sizeof (ctx_group));
                LOGDEBUG ("assigning user %s to group %s", username, group);
  
                if (!strncasecmp (type + 6, ",MANAGED", 8))
                  strlcpy (ATX->managed_group, 
                           ctx_group, 
                           sizeof(ATX->managed_group));
  
              }
              else if (!strcasecmp (type, "CLASSIFICATION") ||
                       !strcasecmp (type, "NEURAL")) 
              {
                char *l = list, *u;
                u = strsep (&l, ":");
                u = strsep (&l, ":");
                u = strsep (&l, ",");
                while (u != NULL)
                {
                  if (strcmp (u, username))
                  {
                    LOGDEBUG ("adding user %s to classification group %s", u,
                              group);
                    if (!strcasecmp (type, "NEURAL"))
                    ATX->flags |= DAF_NEURAL;
               
                    if (u[0] == '*') {
                      ATX->flags |= DAF_GLOBAL;
                      nt_add (ATX->classify_users, u+1);
                    } else
                      nt_add (ATX->classify_users, u);
                  }
                  u = strsep (&l, ",");
                }
              }
              else if (!strcasecmp (type, "MERGED") && strcmp(group, username))
              {
                char *l = list, *u;
                u = strsep (&l, ":");
                u = strsep (&l, ":");
                u = strsep (&l, ",");
                while (u != NULL)
                {
                  if (!strcmp (u, username) || u[0] == '*')
                  {
                      LOGDEBUG ("adding user to merged group %s", group);
  
                      ATX->flags |= DAF_MERGED;
                                                                                  
                      strlcpy(ctx_group, group, sizeof(ctx_group));
                  } else if (u[0] == '-' && !strcmp(u+1, username)) {
                      LOGDEBUG ("removing user from merged group %s", group);
  
                      ATX->flags ^= DAF_MERGED;
                      ctx_group[0] = 0;
                  }
                  u = strsep (&l, ",");
                }
              }
            }
            do_inocgroups = 0;
            user = strtok (NULL, ",");
          }
        }
  
        free (list);
      }
      fclose (file);
    }
  }

  /* Crunch our agent context into a DSPAM context */

  f_mode = ATX->operating_mode;
  f_all  = DSF_SIGNATURE;

  if (ATX->flags & DAF_UNLEARN)
    f_all |= DSF_UNLEARN;

  if (ATX->flags & DAF_CHAINED)
    f_all |= DSF_CHAINED;
 
  if (ATX->flags & DAF_SBPH)
    f_all |= DSF_SBPH;

  /* If there is no preference, defer to commandline */
  if (PTX != NULL && strcmp(_ds_pref_val(PTX, "enableBNR"), "")) {
    if (!strcmp(_ds_pref_val(PTX, "enableBNR"), "on"))
      f_all |= DSF_NOISE;
  } else {
    if (ATX->flags & DAF_NOISE)
     f_all |= DSF_NOISE;
  }

  if (PTX != NULL && strcmp(_ds_pref_val(PTX, "enableWhitelist"), "")) {
    if (!strcmp(_ds_pref_val(PTX, "enableWhitelist"), "on"))
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

  if (PTX != NULL && strcmp(_ds_pref_val(PTX, "statisticalSedation"), ""))
    CTX->training_buffer = atoi(_ds_pref_val(PTX, "statisticalSedation"));
  else if (ATX->training_buffer>=0)
    CTX->training_buffer = ATX->training_buffer;
    LOGDEBUG("sedation level set to: %d", CTX->training_buffer); 

  if (PTX != NULL && strcmp(_ds_pref_val(PTX, "whitelistThreshold"), ""))
    CTX->wh_threshold = atoi(_ds_pref_val(PTX, "whitelistThreshold"));

  if (ATX->classification != -1) {
    CTX->classification  = ATX->classification;
    CTX->source          = ATX->source;
  }

  if (PTX != NULL && strcmp(_ds_pref_val(PTX, "trainingMode"), "")) {
    if (!strcasecmp(_ds_pref_val(PTX, "trainingMode"), "TEFT"))
      CTX->training_mode = DST_TEFT;
    else if (!strcasecmp(_ds_pref_val(PTX, "trainingMode"), "TOE"))
      CTX->training_mode = DST_TOE;
    else if (!strcasecmp(_ds_pref_val(PTX, "trainingMode"), "TUM"))
      CTX->training_mode = DST_TUM;
    else if (!strcasecmp(_ds_pref_val(PTX, "trainingMode"), "NOTRAIN"))
      CTX->training_mode = DST_NOTRAIN;
    else
      CTX->training_mode = ATX->training_mode;
  } else {
    CTX->training_mode = ATX->training_mode;
  }

  if (CTX->training_buffer != -1)
    CTX->training_buffer = ATX->training_buffer;

  return CTX;
}

/* process_neural_decision: determine reliability based on which nodes
   correcly marked a message */

#ifdef NEURAL
int process_neural_decision(DSPAM_CTX *CTX, struct _ds_neural_decision *DEC) { 
  struct _ds_neural_record r;
  char d;
  void *ptr;

  for(ptr = DEC->data;ptr<DEC->data+DEC->length;ptr+=sizeof(uid_t)+1) {
     memcpy(&r.uid, ptr, sizeof(uid_t));
     memcpy(&d, ptr+sizeof(uid_t), 1);
     if ((d == 'S' && CTX->classification == DSR_ISINNOCENT) ||
         (d == 'I' && CTX->classification == DSR_ISSPAM)) {
       if (!_ds_get_node(CTX, NULL, &r)) {
         r.total_incorrect++;
         r.total_correct--;
         _ds_set_node(CTX, NULL, &r);
       }
     }
  }
  free(DEC->data);
  return 0;
}
#endif

/* retrain_message: reclassify a message, perform iterative training */

int retrain_message(DSPAM_CTX *CTX, AGENT_CTX *ATX) {
  int result;

#ifdef TEST_COND_TRAINING
  int do_train = 1, iter = 0, ck_result = 0, t_mode = CTX->source;

  /* train until test conditions are met, 5 iterations max */
  while (do_train && iter < 5)
  {
    DSPAM_CTX *CLX;
    int match = (CTX->classification == DSR_ISSPAM) ? 
                 DSR_ISSPAM : DSR_ISINNOCENT;
    iter++;
#endif

    result = dspam_process (CTX, NULL);

#ifdef TEST_COND_TRAINING

    /* Only subtract innocent values once */
    CTX->source = DSS_CORPUS;

    LOGDEBUG ("reclassifying iteration %d result: %d", iter, result);

    if (t_mode == DSS_CORPUS)
      do_train = 0;

    /* only attempt test-conditional training on a mature corpus */
    if (CTX->totals.innocent_learned + CTX->totals.innocent_classified 
        < 1000 && 
          CTX->classification == DSR_ISSPAM)
      do_train = 0;
    else
    {
      int f_all =  DSF_SIGNATURE;

      /* CLX = Classify Context */
      if (ATX->flags & DAF_CHAINED)
        f_all |= DSF_CHAINED;

      if (ATX->flags & DAF_NOISE)
        f_all |= DSF_NOISE;

      if (ATX->flags & DAF_SBPH)
        f_all |= DSF_SBPH;

      CLX = dspam_create (CTX->username, 
                        CTX->group, 
                        _ds_read_attribute(agent_config, "Home"), 
                        DSM_CLASSIFY, 
                        f_all);
      if (!CLX)
      {
        do_train = 0;
        break;
      }

      set_libdspam_attributes(CLX);
      if (attach_context(CLX, ATX->dbh)) {
        do_train = 0;
        dspam_destroy(CLX);
        break;
      }

      CLX->signature = &ATX->SIG;
      ck_result = dspam_process (CLX, NULL);
      if (ck_result || CLX->result == match)
        do_train = 0;
      CLX->signature = NULL;
      dspam_destroy (CLX);
    }
  }

  CTX->source = DSS_ERROR;
#endif

  return 0;
}

/* ensure_confident_result: consult global group, neural network, or
   clasification network if the user isn't confident in their result */

int ensure_confident_result(DSPAM_CTX *CTX, AGENT_CTX *ATX, int result) {
  int was_spam = 0;

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

#ifdef NEURAL
    /* Consult neural network */
    if (ATX->flags & DAF_NEURAL) {
      struct _ds_neural_record r;
      struct nt_node *node_int;
      struct nt_c c_i;
      ds_heap_t heap_sort;
      ds_heap_element_t heap_element;
      int total_nodes = ATX->classify_users->items;
      float bay_top = 0.0;
      float bay_bot = 0.0;
      float bay_result;
      int bay_used = 0;
      int res, i = 0;

      ATX->DEC.length = (total_nodes * (sizeof(uid_t)+1));
      ATX->DEC.data = calloc(1, ATX->DEC.length);
      
      if (ATX->DEC.data == NULL) {
        LOG(LOG_CRIT, ERROR_MEM_ALLOC);
        return EUNKNOWN;
      }

      total_nodes /= 5;
      if (total_nodes<2)
        total_nodes = 2;
      heap_sort = ds_heap_create(total_nodes, HP_VALUE);
      
      node_int = c_nt_first (ATX->classify_users, &c_i);
      while (node_int != NULL) 
      {
        res = _ds_get_node(CTX, node_int->ptr, &r);
        if (!res) {
          memcpy(ATX->DEC.data+(i*(sizeof(uid_t)+1)), &r.uid, sizeof(uid_t));
          memset(ATX->DEC.data+(i*(sizeof(uid_t)+1))+sizeof(uid_t), 'E', 1);
       
          LOGDEBUG ("querying node %s", (const char *) node_int->ptr);
          res = user_classify ((const char *) node_int->ptr,
                                  CTX->signature, NULL, ATX);
          if (res == DSR_ISWHITELISTED)
            res = DSR_ISINNOCENT;

          if ((res == DSR_ISSPAM || res == DSR_ISINNOCENT) && 
              r.total_incorrect+r.total_correct>4) 
          {
            ds_heap_insert(heap_sort,
               (double) r.total_correct / (r.total_correct+r.total_incorrect),
               r.uid, res, 0);
          }
          r.total_correct++;
          _ds_set_node(CTX, NULL, &r);

          if (res == DSR_ISSPAM)
            memset(ATX->DEC.data+(i*(sizeof(uid_t)+1))+sizeof(uid_t), 'S', 1);
          else if (res == DSR_ISINNOCENT)
            memset(ATX->DEC.data+(i*(sizeof(uid_t)+1))+sizeof(uid_t), 'I', 1);

          i++;
        }
        node_int = c_nt_next (ATX->classify_users, &c_i);
      }

      total_nodes /= 5;
      if (total_nodes<2)
        total_nodes = 2;
      heap_element = heap_sort->root;

      /* include the top n reliable sources */
      while(heap_element && total_nodes>0) {
        float probability = (heap_element->frequency == DSR_ISINNOCENT     || 
                             heap_element->frequency == DSR_ISWHITELISTED) ?
          1-heap_element->probability : heap_element->probability;

        LOGDEBUG("including node %llu [%2.6f]", heap_element->token, probability);
        if (bay_used == 0)
        {
          bay_top = probability;
          bay_bot = 1 - probability; 
        }
        else
        {
          bay_top *= probability;
          bay_bot *= (1 - probability);
        }
      
        bay_used++;
        total_nodes--;
        heap_element = heap_element->next;
      }
      ds_heap_destroy(heap_sort);

      if (bay_used) { 
        bay_result = (bay_top) / (bay_top + bay_bot);
        if (bay_result > 0.80) { 
          result = DSR_ISSPAM;
        }

        LOGDEBUG("Neural Network Result: %2.6f", bay_result); 
      }

    /* Consult classification network */
    } else {
#endif
      struct nt_node *node_int;
      struct nt_c c_i;

      node_int = c_nt_first (ATX->classify_users, &c_i);
      while (node_int != NULL && result != DSR_ISSPAM)
      {
        LOGDEBUG ("checking result for user %s", (const char *) node_int->ptr);
        result = user_classify ((const char *) node_int->ptr,
                                CTX->signature, NULL, ATX);
        if (result == DSR_ISSPAM)
        {
          LOGDEBUG ("CLASSIFY CATCH: %s", (const char *) node_int->ptr);
          CTX->result = result;
        }
  
        node_int = c_nt_next (ATX->classify_users, &c_i);
      }
#ifdef NEURAL
    }
#endif

    /* Re-add as spam */
    if (result == DSR_ISSPAM && !was_spam)
    {
      DSPAM_CTX *CTC = malloc(sizeof(DSPAM_CTX));

      if (CTC == NULL) {
        report_error(ERROR_MEM_ALLOC);
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
      CTX->totals.spam_misclassified--;
      CTX->result = result;
    }

    /* If the global user thinks it's innocent, and the user thought it was
       spam, retrain the user as a false positive */
    if ((result == DSR_ISINNOCENT || result == DSR_ISWHITELISTED) && was_spam) {
      DSPAM_CTX *CTC = malloc(sizeof(DSPAM_CTX));
      if (CTC == NULL) {
        report_error(ERROR_MEM_ALLOC);
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
      CTX->totals.innocent_misclassified--;
      CTX->result = result;
    }
  }

  return result;
}

/* log_events: write journal to system.log and user.log */

int log_events(DSPAM_CTX *CTX) {
  char filename[MAX_FILENAME_LENGTH];
  char *subject = NULL, *from = NULL;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  FILE *file;
  char class;
  char x[1024];
  size_t y;

  _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"), CTX->username, "log");

  node_nt = c_nt_first (CTX->message->components, &c_nt);
  if (node_nt != NULL)
  {
    struct _ds_message_block *block;
                                                                              
    block = node_nt->ptr;
    if (block->headers != NULL)
    {
      struct _ds_header_field *head;
      struct nt_node *node_header;

      node_header = block->headers->first;
      while(node_header != NULL) {
        head = (struct _ds_header_field *) node_header->ptr;
        if (head != NULL && !strcasecmp(head->heading, "Subject")) 
          subject = head->data;
        else if (head != NULL && !strcasecmp(head->heading, "From"))
          from = head->data;

        node_header = node_header->next;
      }
    }
  }

  if (CTX->result == DSR_ISSPAM)
    class = 'S';
  else if (CTX->result == DSR_ISWHITELISTED)
    class = 'W';
  else 
    class = 'I';

  if (CTX->source == DSS_ERROR) { 
    if (CTX->classification == DSR_ISSPAM)
      class = 'M';
    else if (CTX->classification == DSR_ISINNOCENT)
      class = 'F';
  }

  if (CTX->source == DSS_INOCULATION)
    class = 'N';
  else if (CTX->source == DSS_CORPUS)
    class = 'C';
     
  snprintf(x, sizeof(x), "%ld\t%c\t%s\t%s", 
          (long) time(NULL), 
          class,
          (from == NULL) ? "<None Specified>" : from,
          (subject == NULL) ? "<None Specified>" : subject);
  for(y=0;y<strlen(x);y++)
    if (x[y] == '\n') 
      x[y] = 32;

  if (_ds_match_attribute(agent_config, "UserLog", "on")) {
    _ds_prepare_path_for(filename);
    file = fopen(filename, "a");
    if (file != NULL) {
      int i = _ds_get_fcntl_lock(fileno(file));
      if (!i) {
          fputs(x, file);
          fputs("\n", file);
          _ds_free_fcntl_lock(fileno(file));
      } else {
        LOG(LOG_WARNING, "Failed to lock %s: %d: %s\n", filename, i, 
                         strerror(errno));
      }

      fclose(file);
    }
  }

  if (_ds_match_attribute(agent_config, "SystemLog", "on")) {

    snprintf(filename, sizeof(filename), "%s/system.log", _ds_read_attribute(agent_config, "Home"));
   
    file = fopen(filename, "a");
    if (file != NULL) {
      int i = _ds_get_fcntl_lock(fileno(file));
      if (!i) {
        char s[1024];

        snprintf(s, sizeof(s), "%s\t%f\n", x, gettime()-timestart);
        fputs(s, file);
        _ds_free_fcntl_lock(fileno(file));
      } else {
        LOG(LOG_WARNING, "Failed to lock %s: %d: %s\n", filename, i, 
                          strerror(errno));
      }
      fclose(file);
    }
  }
  return 0;
}

/* add_xdspam_headers: add headers from this round of processing */

int add_xdspam_headers(DSPAM_CTX *CTX, AGENT_CTX *ATX, agent_pref_t PTX) {
  struct nt_node *node_nt;
  struct nt_c c_nt;

  node_nt = c_nt_first (CTX->message->components, &c_nt);
  if (node_nt != NULL && ! FALSE_POSITIVE(CTX))
  {
    struct _ds_message_block *block = node_nt->ptr;
    struct nt_node *node_ft;
    struct nt_c c_ft;
    if (block != NULL && block->headers != NULL)
    {
      struct _ds_header_field *head; 
      char data[10240];
      char scratch[128];
  
      strcpy(data, "X-DSPAM-Result: ");
      switch (CTX->result) {
        case DSR_ISSPAM:
          strcat(data, "Spam");
          break;
        case DSR_ISWHITELISTED:
          strcat(data, "Whitelisted");
          break;
        default:
          strcat(data, "Innocent");
          break;
      }
  
      head = _ds_create_header_field(data);
      if (head != NULL)
      {
#ifdef VERBOSE
        LOGDEBUG ("appending header %s: %s", head->heading, head->data)
#endif
        nt_add (block->headers, (void *) head);
      }
      else {
        LOG (LOG_CRIT, ERROR_MEM_ALLOC);
      }

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
        LOG (LOG_CRIT, ERROR_MEM_ALLOC);

      snprintf(data, sizeof(data), "X-DSPAM-Probability: %01.4f", 
               CTX->probability);

      head = _ds_create_header_field(data);
      if (head != NULL)
      {
#ifdef VERBOSE
        LOGDEBUG ("appending header %s: %s", head->heading, head->data)
#endif
          nt_add (block->headers, (void *) head);
      }
      else
        LOG (LOG_CRIT, ERROR_MEM_ALLOC);

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
          LOGDEBUG ("appending header %s: %s", head->heading, head->data)
#endif
          nt_add (block->headers, (void *) head);
        }
        else
          LOG (LOG_CRIT, ERROR_MEM_ALLOC);
      }

      if (CTX->result == DSR_ISSPAM)
      {
        snprintf(data, sizeof(data), "X-DSPAM-User: %s", CTX->username);
        head = _ds_create_header_field(data);
        if (head != NULL)
        {
#ifdef VERBOSE
          LOGDEBUG ("appending header %s: %s", head->heading, head->data)
#endif
            nt_add (block->headers, (void *) head);
        }
        else
          LOG (LOG_CRIT, ERROR_MEM_ALLOC);
      }

      if (!strcmp(_ds_pref_val(PTX, "showFactors"), "on")) {

        if (CTX->factors != NULL) {
          snprintf(data, sizeof(data), "X-DSPAM-Factors: %d",
                   CTX->factors->items);
          node_ft = c_nt_first(CTX->factors, &c_ft);
          while(node_ft != NULL) {
            struct dspam_factor *f = (struct dspam_factor *) node_ft->ptr;
            strlcat(data, ",\n\t", sizeof(data));
            snprintf(scratch, sizeof(scratch), "%s, %2.5f",
                     f->token_name, f->value);
            strlcat(data, scratch, sizeof(data));
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
    }
  }
  return 0;
}

/* embed_signature: embed the signature in all relevant parts of the message */

int embed_signature(DSPAM_CTX *CTX, AGENT_CTX *ATX, agent_pref_t PTX) {
  struct nt_node *node_nt;
  struct nt_c c_nt;
  char toplevel_boundary[128] = { 0 };
  struct _ds_message_block *block;
  int i = 0;

  if (CTX->training_mode == DST_NOTRAIN || ! ATX->signature[0])
    return 0;

  /* Add our own X-DSPAM Headers */
  node_nt = c_nt_first (CTX->message->components, &c_nt);

  if (node_nt == NULL || node_nt->ptr == NULL)
    return EFAILURE;

  block = node_nt->ptr;

  /* Signed messages are handled differently */
  if (block->media_subtype == MST_SIGNED)
    return embed_signed(CTX, ATX, PTX);

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

        /* The part is one of the top-level parts */ 
        && (toplevel_boundary[0] == 0 ||
           (block->terminating_boundary && 
            !strncmp(block->terminating_boundary, toplevel_boundary,
             strlen(toplevel_boundary)))))
    {
      int is_attachment = 0;
      struct _ds_header_field *field;
      struct nt_node *node_hnt;
      struct nt_c c_hnt;

      node_hnt = c_nt_first (block->headers, &c_hnt);
      while (node_hnt != NULL)
      {
        field = (struct _ds_header_field *) node_hnt->ptr;
        if (field != NULL && field->heading != NULL && field->data != NULL)
          if (!strncasecmp (field->heading, "Content-Disposition", 19))
            if (!strncasecmp (field->data, "attachment", 10))
              is_attachment = 1;
        node_hnt = c_nt_next (block->headers, &c_hnt);
      }

      if (is_attachment)
      {
        node_nt = c_nt_next (CTX->message->components, &c_nt);
        i++;
        continue;
      }

      /* Some email clients reformat HTML parts, and require that we include
         the signature before the HTML close tags (because they're stupid) */

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
        free (dup);
      }
    }

    node_nt = c_nt_next (CTX->message->components, &c_nt);
    i++;
  }
  return 0;
}

/* embed_signed: reformat a signed message to include a signature */

int embed_signed(DSPAM_CTX *CTX, AGENT_CTX *ATX, agent_pref_t PTX) {
  struct nt_node *node_nt, *node_block, *parent;
  struct nt_c c_nt;
  struct _ds_message_block *block, *newblock;
  struct _ds_header_field *field;
  char scratch[256], data[256];

  node_block = c_nt_first (CTX->message->components, &c_nt);
  if (node_block == NULL || node_block->ptr == NULL)
    return EFAILURE;

  block = node_block->ptr;

  /* Construct a new block to contain the signed message */

  newblock = (struct _ds_message_block *) 
    malloc(sizeof(struct _ds_message_block));
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
      parent->next = node_nt->next;
      CTX->message->components->items--;
      CTX->message->components->insert = NULL;
      _ds_destroy_block(node_nt->ptr);    
      free(node_nt);
    } else {
      parent = node_nt;
    }
    node_nt = node_nt->next;
  }

  /* Create a new message part containing only the boundary delimiter */

  newblock = (struct _ds_message_block *)
    malloc(sizeof(struct _ds_message_block));
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

  newblock = (struct _ds_message_block *)
    malloc(sizeof(struct _ds_message_block));
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
  }

  LOG (LOG_CRIT, ERROR_MEM_ALLOC);
  return EUNKNOWN;
}

/* tracksource: report spam/ham sources as requested, log to SBL */

int tracksource(DSPAM_CTX *CTX) {
  char ip[32];

  if (!dspam_getsource (CTX, ip, sizeof (ip)))
  {
    if (CTX->totals.innocent_learned + CTX->totals.innocent_classified > 2500) {
      if (CTX->result == DSR_ISSPAM && 
          _ds_match_attribute(agent_config, "TrackSources", "spam")) {
        FILE *file;
        char dropfile[MAX_FILENAME_LENGTH];
        LOG (LOG_INFO, "spam detected from %s", ip);
        if (_ds_read_attribute(agent_config, "SBLQueue")) {
          snprintf(dropfile, sizeof(dropfile), "%s/%s", 
            _ds_read_attribute(agent_config, "SBLQueue"), ip);
          file = fopen(dropfile, "w");
          if (file != NULL) 
            fclose(file);
        }
      }
      if (CTX->result != DSR_ISSPAM &&
          _ds_match_attribute(agent_config, "TrackSources", "nonspam"))
      {
        LOG (LOG_INFO, "innocent message from %s", ip);
      }
    }
  }
  return 0;
}

int is_blacklisted(const char *ip) {
  struct attribute *attrib;
  struct in_addr ip32;
  struct hostent *h;
  char host[32];
  char lookup[256];
  char *ptr;
  char *str;
  char *octet[4];
  int bad = 0, i = 3;

  host[0] = 0;
  str = strdup(ip);
  ptr = strtok(str, ".");
  while(ptr != NULL && i>=0) {
    octet[i] = ptr;
    ptr = strtok(NULL, ".");
    i--;
  }

  snprintf(host, sizeof(host), "%s.%s.%s.%s.", octet[0], octet[1], octet[2], octet[3]);
  free(str);

  attrib = _ds_find_attribute(agent_config, "Lookup");
  while(attrib != NULL) {
    snprintf(lookup, sizeof(lookup), "%s%s", host, attrib->value);
    h = gethostbyname(lookup);

    if (h != NULL && h->h_addr_list[0]) {
      memcpy(&ip32, h->h_addr_list[0], h->h_length);
      if (!strcmp(inet_ntoa(ip32), "127.0.0.2"))
        bad = 1;
    }

    free(h);

    attrib = attrib->next;
  }

  return bad;
}

#ifdef DAEMON
int daemon_start(AGENT_CTX *ATX) {
  DRIVER_CTX DTX;
  char *pidfile;

  __daemon_run  = 1;
  __num_threads = 0;
  __hup = 0;
  pthread_mutex_init(&__lock, NULL);
  libdspam_init();
  LOG(LOG_INFO, DAEMON_START);

  while(__daemon_run) {
    pidfile = _ds_read_attribute(agent_config, "ServerPID");

    DTX.CTX = dspam_create (NULL, NULL,
                      _ds_read_attribute(agent_config, "Home"),
                      DSM_TOOLS, 0);
    if (!DTX.CTX)
    {
      LOGDEBUG("unable to initialize dspam context");
      exit(EXIT_FAILURE);
    }

    set_libdspam_attributes(DTX.CTX);
    DTX.flags |= DRF_STATEFUL;

#ifdef DEBUG
    if (DO_DEBUG)
      DO_DEBUG = 2;
#endif
    if (dspam_init_driver (&DTX))
    {
      LOG (LOG_WARNING, "unable to initialize storage driver");
      exit(EXIT_FAILURE);
    }

    if (pidfile) {
      FILE *file;
      file = fopen(pidfile, "w");
      if (file == NULL) {
        file_error(ERROR_FILE_WRITE, pidfile, strerror(errno));
      } else {
        fprintf(file, "%ld\n", (long) getpid());
        fclose(file);
      }
    }

    LOGDEBUG("spawning daemon listener");

    daemon_listen(&DTX);

    LOGDEBUG("waiting for processing threads to exit");

    while(__num_threads) { 
      struct timeval tv;
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      select(0, NULL, NULL, NULL, &tv);
    }

    if (pidfile)
      unlink(pidfile);

    dspam_destroy(DTX.CTX);
    dspam_shutdown_driver(&DTX);

    /* Reload */
    if (__hup) {
      if (agent_config)
        _ds_destroy_attributes(agent_config);

      agent_config = read_config(NULL);
      if (!agent_config) {
        report_error(ERROR_READ_CONFIG);
        exit(EXIT_FAILURE);
      }

      __daemon_run = 1;
    }
  }

  LOG(LOG_INFO, DAEMON_EXIT);
  pthread_mutex_destroy(&__lock);
  libdspam_shutdown();

  return 0;
}
#endif
