/* $Id: agent_shared.c,v 1.5 2004/12/20 12:25:33 jonz Exp $ */

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

#include "agent_shared.h"
#include "pref.h"
#include "libdspam.h"
#include "language.h"
#include "buffer.h"
#include "base64.h"
#include "tbt.h"
#include "pref.h"

/*
    process_features: convert a plain text feature line into agent context 
                      values
    ATX		agent context
    features	plain text list of features to parse
*/
 
int process_features(AGENT_CTX *ATX, const char *features) {
  char *s, *d, *ptrptr;
  int ret = 0;

  if (features[0] == 0)
    return 0;

  d = strdup(features);
  if (d == NULL) {
    report_error(ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

  s = strtok_r(d, ",", &ptrptr);
  while(s != NULL) {
    if (!strcmp(s, "chained") || !strcmp(s, "ch"))
      ATX->flags |= DAF_CHAINED;
    else if (!strcmp(s, "sbph") || !strcmp(s, "sb"))
      ATX->flags |= DAF_SBPH;
    else if (!strcmp(s, "noise") || !strcmp(s, "no") || !strcmp(s, "bnr"))
      ATX->flags |= DAF_NOISE;
    else if (!strcmp(s, "whitelist") || !strcmp(s, "wh"))
      ATX->flags |= DAF_WHITELIST;
    else if (!strncmp(s, "tb=", 3)) {
      ATX->training_buffer = atoi(strchr(s, '=')+1);

      if (ATX->training_buffer < 0 || ATX->training_buffer > 10) {
        report_error(ERROR_TB_INVALID);
        exit(EXIT_FAILURE);
      }
    }
    else
    {
      report_error_printf(ERROR_UNKNOWN_FEATURE, s);
      ret = EINVAL;
    }

    s = strtok_r(NULL, ",", &ptrptr);
  }
  free(d);
  return ret;
}

/*
    process_mode: reads plain text mode into an agent context
    ATX		agent context
    mode	plain text name of mode
*/

int process_mode(AGENT_CTX *ATX, const char *mode) {
  if (!strcmp(mode, "toe"))
    ATX->training_mode = DST_TOE;
  else if (!strcmp(mode, "teft"))
    ATX->training_mode = DST_TEFT;
  else if (!strcmp(mode, "tum"))
    ATX->training_mode = DST_TUM;
  else if (!strcmp(mode, "notrain"))
    ATX->training_mode = DST_NOTRAIN;
  else if (!strcmp(mode, "unlearn")) {
    ATX->training_mode = DST_TEFT;
    ATX->flags |= DAF_UNLEARN;
  }
  else
  {
    report_error_printf(ERROR_TR_MODE_INVALID, mode);
    return EINVAL;
  }

  return 0;
}

/*
    initialize_atx: initialize an agent context
    ATX		agent context to initialize
*/

int initialize_atx(AGENT_CTX *ATX) {
#if defined(TRUSTED_USER_SECURITY) && defined(_REENTRANT) && defined(HAVE_GETPWUID_R)
  struct passwd pwbuf;
  char buf[1024];
#endif

  /* Initialize Agent Context */
  memset(ATX, 0, sizeof(AGENT_CTX));
  ATX->training_mode   = -1;
  ATX->training_buffer = 5;
  ATX->classification  = -1;
  ATX->source          = -1;
  ATX->operating_mode  = DSM_PROCESS;
  ATX->users           = nt_create (NT_CHAR);

  if (ATX->users == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return EUNKNOWN;
  }

#ifdef TRUSTED_USER_SECURITY

#if defined(_REENTRANT) && defined(HAVE_GETPWUID_R)
  if (getpwuid_r(getuid(), &pwbuf, buf, sizeof(buf), &ATX->p))
    ATX->p = NULL;
#else
  ATX->p = getpwuid (getuid());
#endif

  if (!ATX->p) {
    report_error(ERROR_RUNTIME_USER);
    exit(EXIT_FAILURE);
  }

  if (ATX->p->pw_uid == 0)
    ATX->trusted = 1;
  else
    ATX->trusted = _ds_match_attribute(agent_config, "Trust", ATX->p->pw_name);

  if (!ATX->trusted)
    nt_add (ATX->users, ATX->p->pw_name);
#endif

  return 0;
}

/*
 * Process commandline arguments 
 * In the future this may be called from an XML or LMTP query
 */

int process_arguments(AGENT_CTX *ATX, int argc, char **argv) {
  int i, user_flag = 0;
  char *clienthost = _ds_read_attribute(agent_config, "ClientHost");
  char *ptrptr;

  for (i = 0; i < argc; i++)
  {

#ifdef DEBUG
    strlcat (ATX->debug_args, argv[i], sizeof (ATX->debug_args));
    strlcat (ATX->debug_args, " ", sizeof (ATX->debug_args));
#endif

    if (user_flag &&
       (argv[i][0] == '-' || argv[i][0] == 0 || !strcmp(argv[i], "--")))
    {
       user_flag = 0;
       if (!strcmp(argv[i], "--"))
         continue;
    }

    if (clienthost && !user_flag && i && strcmp(argv[i], "--user")) {
      strlcat (ATX->client_args, argv[i], sizeof(ATX->client_args));
      strlcat (ATX->client_args, " ", sizeof(ATX->client_args));
    }

    /* Debug */
    if (!strcmp (argv[i], "--debug"))
    {
#ifdef DEBUG
      if (DO_DEBUG == 0)
        DO_DEBUG = 1;
#endif
      continue;
    }

#if defined(DAEMON) && !defined(_DSPAMC_H)
    /* Launch into daemon mode */

#ifdef TRUSTED_USER_SECURITY
    if (!strcmp (argv[i], "--daemon") && ATX->trusted) {
#else
    if (!strcmp (argv[i], "--daemon")) {
#endif
      ATX->operating_mode = DSM_DAEMON;
      continue;
    }
#endif
 
    /* Set training mode */
    if (!strncmp (argv[i], "--mode=", 7))
    {
      char *mode = strchr(argv[i], '=')+1;
      process_mode(ATX, mode);
      continue;
    }

    /* Set runtime target user(s) */

    if (!strcmp (argv[i], "--user"))
    {
      user_flag = 1;
      continue;
    }

    if (user_flag)
    {
      if (argv[i] != NULL && strlen (argv[i]) <= MAX_USERNAME_LENGTH)
      {
        char user[MAX_USERNAME_LENGTH];

        if (_ds_match_attribute(agent_config, "Broken", "case")) 
          lc(user, argv[i]);
        else 
          strlcpy(user, argv[i], MAX_USERNAME_LENGTH);

#ifdef TRUSTED_USER_SECURITY
        if (!ATX->trusted && strcmp(user, ATX->p->pw_name)) {
          report_error_printf(ERROR_TRUSTED_USER, ATX->p->pw_uid,
                              ATX->p->pw_name);
          return EFAILURE;
        }

        if (ATX->trusted)
#endif
          nt_add (ATX->users, user);
      }
    }

    /* Print Syntax and Exit */

    if (!strcmp (argv[i], "--help"))
    {
      fprintf (stderr, "%s\n", SYNTAX);
      exit(EXIT_SUCCESS);
    }

    /* Print Version and Exit */

    if (!strcmp (argv[i], "--version"))
    {
      printf ("\nDSPAM Anti-Spam Suite %s (agent/library)\n\n", VERSION);
      printf ("Copyright (c) 2002-2004 Network Dweebs Corporation\n");
      printf ("http://www.nuclearelephant.com/projects/dspam/\n\n");
      printf ("DSPAM may be copied only under the terms of the GNU "
              "General Public License,\n");
      printf ("a copy of which can be found with the DSPAM distribution "
              "kit.\n\n");
#ifdef TRUSTED_USER_SECURITY
      if (ATX->trusted) {
#endif
        printf("Configuration parameters: %s\n\n", CONFIGURE_ARGS);
#ifdef TRUSTED_USER_SECURITY
      }
#endif
      exit (EXIT_SUCCESS);
    }

    /* Storage profile */

    if (!strncmp (argv[i], "--profile=", 10))
    {
#ifdef TRUSTED_USER_SECURITY
      if (!ATX->trusted) {
        report_error_printf(ERROR_TRUSTED_OPTION, "--profile", ATX->p->pw_uid,
                            ATX->p->pw_name);
        return EFAILURE;
      }
#endif
      if (!_ds_match_attribute(agent_config, "Profile", argv[i]+10)) {
        report_error_printf(ERROR_NO_SUCH_PROFILE, argv[i]+10);
        return EINVAL;
      } else {
        _ds_overwrite_attribute(agent_config, "DefaultProfile", argv[i]+10);
      }
      continue;
    }

    /* Signature specified via commandline */

    if (!strncmp (argv[i], "--signature=", 12)) 
    {
      strlcpy(ATX->signature, strchr(argv[i], '=')+1, sizeof(ATX->signature));
      continue;
    }

    /* If the message already has a classification */

    if (!strncmp (argv[i], "--class=", 8))
    {
      char *c = strchr(argv[i], '=')+1;
      if (!strcmp(c, "spam"))
        ATX->classification = DSR_ISSPAM;
      else if (!strcmp(c, "innocent"))
        ATX->classification = DSR_ISINNOCENT;
      else
      {
        report_error_printf(ERROR_UNKNOWN_CLASS, c);
        return EINVAL;
      }

      continue;
    }

    /*
       The source of the classification:
         error: classification error made by dspam
         corpus: message from user's corpus
         inoculation: message inoculation
    */

    if (!strncmp (argv[i], "--source=", 9))
    {
      char *s = strchr(argv[i], '=')+1;

      if (!strcmp(s, "corpus"))
        ATX->source = DSS_CORPUS;
      else if (!strcmp(s, "inoculation"))
        ATX->source = DSS_INOCULATION;
      else if (!strcmp(s, "error"))
        ATX->source = DSS_ERROR;
      else
      {
        report_error_printf(ERROR_UNKNOWN_SOURCE, s); 
        return EINVAL;
      }
      continue;
    }

    /* Operating Mode: Classify Only. */
    if (!strcmp (argv[i], "--classify"))
    {
      ATX->operating_mode = DSM_CLASSIFY;
      ATX->training_mode = DST_NOTRAIN;
      continue;
    }

    /* Operating Mode: Process Message */
    if (!strcmp (argv[i], "--process"))
    {
      ATX->operating_mode = DSM_PROCESS;
      continue;
    }

    /*
      Which messages should be delivered? 
      spam,innocent
      If spam is not specified, standard quarantine procedure will be used

      summary
      Output classify summary headers
    */

    if (!strncmp (argv[i], "--deliver=", 10))
    {
      char *d = strdup(strchr(argv[i], '=')+1);
      char *s;
      if (d == NULL) {
        report_error(ERROR_MEM_ALLOC);
        return EUNKNOWN;
      }

      s = strtok_r(d, ",", &ptrptr);
      while(s != NULL) {
        if (!strcmp(s, "spam")) 
          ATX->flags |= DAF_DELIVER_SPAM;
        else if (!strcmp(s, "innocent"))
          ATX->flags |= DAF_DELIVER_INNOCENT;
        else if (!strcmp(s, "summary"))
          ATX->flags |= DAF_SUMMARY;
        else
        {
          report_error_printf(ERROR_UNKNOWN_DELIVER, s);
          return EINVAL;
        }
      
        s = strtok_r(NULL, ",", &ptrptr);
      }
      free(d);
      continue;
    }

    /* 
      Which features should be enabled?
      chained,noise,whitelist

      chained:   chained tokens (nGrams)
      sbph:      use sparse binary polynomial hashing tokenizer
      noise:     bayesian noise reduction
      whitelist: automatic whitelisting
      tb=N:      set training buffer level (0-10)

      all features have their own internal instantiation requirements
    */

    if (!strncmp (argv[i], "--feature=", 10))
    {
      ATX->feature = 1;
      process_features(ATX, strchr(argv[i], '=')+1);
      continue;
    }

    /* Output message to stdout */
    if (!strcmp (argv[i], "--stdout"))
    {
      ATX->flags |= DAF_STDOUT;
      continue;
    }

    /* Append all other arguments as mailer args */
    if (i > 0 && !user_flag
#ifdef TRUSTED_USER_SECURITY
         && ATX->trusted
#endif
    )
    {
      if (argv[i][0] == 0)
        strlcat (ATX->mailer_args, "\"\"", sizeof (ATX->mailer_args));
      else
        strlcat (ATX->mailer_args, argv[i], sizeof (ATX->mailer_args));
      strlcat (ATX->mailer_args, " ", sizeof (ATX->mailer_args));
    }
  }
 
  return 0;
}

/*
   Apply default values from dspam.conf in absence of other options 
   ATX	agent context

*/

int apply_defaults(AGENT_CTX *ATX) {

  /* Training mode */

  if (ATX->training_mode == -1) {
    char *v = _ds_read_attribute(agent_config, "TrainingMode");
    process_mode(ATX, v);
  }

  /* Delivery agent */

  if (!(ATX->flags & DAF_STDOUT)) {
    char key[32];
#ifdef TRUSTED_USER_SECURITY
    if (!ATX->trusted) 
      strcpy(key, "UntrustedDeliveryAgent");
    else
#endif
      strcpy(key, "TrustedDeliveryAgent");

    if (_ds_read_attribute(agent_config, key)) {
      char fmt[sizeof(ATX->mailer_args)];
      snprintf(fmt,
               sizeof(fmt),
               "%s ", 
               _ds_read_attribute(agent_config, key));
#ifdef TRUSTED_USER_SECURITY
      if (ATX->trusted)
#endif
        strlcat(fmt, ATX->mailer_args, sizeof(fmt));
      strcpy(ATX->mailer_args, fmt);
    } else {
      if (!(ATX->flags & DAF_STDOUT)) {
        report_error_printf(ERROR_NO_AGENT, key);
        return EINVAL;
      }
    }
  }

  /* Quarantine */

  if (_ds_read_attribute(agent_config, "QuarantineAgent")) {
    snprintf(ATX->spam_args,
             sizeof(ATX->spam_args),
             "%s ",
             _ds_read_attribute(agent_config, "QuarantineAgent"));
  } else {
    LOGDEBUG("No QuarantineAgent option found. Using quarantine.");
  }

  /* Features */

  if (!ATX->feature && _ds_find_attribute(agent_config, "Feature")) {
    attribute_t *attrib = _ds_find_attribute(agent_config, "Feature");

    while(attrib != NULL) { 
      process_features(ATX, attrib->value);
      attrib = attrib->next;
    }
  }

  return 0;
}

/* Sanity-Check ATX */

int check_configuration(AGENT_CTX *ATX) {

  if (ATX->classification != -1 && ATX->source == -1)
  {
    report_error(ERROR_NO_SOURCE);
    return EINVAL;
  }

  if (ATX->source != -1 && ATX->classification == -1)
  {
    report_error(ERROR_NO_CLASS);
    return EINVAL;
  }

  if (ATX->operating_mode == -1)
  {
    report_error(ERROR_NO_OP_MODE);
    return EINVAL;
  }

  if (ATX->training_mode == -1)
  {
    report_error(ERROR_NO_TR_MODE);
    return EINVAL;
  }

  if (!_ds_match_attribute(agent_config, "ParseToHeaders", "on")) {

    if (ATX->users->items == 0)
    {
      LOG (LOG_ERR, ERROR_USER_UNDEFINED);
      report_error (ERROR_USER_UNDEFINED);
      fprintf (stderr, "%s\n", SYNTAX);
      return EINVAL;
    }
  }

  return 0;
}

/* Read message from stdin */

buffer * read_stdin(AGENT_CTX *ATX) {
  char buff[1024];
  buffer *message;
  int body = 0, line = 1;

  message = buffer_create(NULL);
  if (message == NULL) {
    LOG(LOG_CRIT, ERROR_MEM_ALLOC);
    return NULL;
  }

  if (ATX->signature[0] == 0) {
    while ((fgets (buff, sizeof (buff), stdin)) != NULL)
    {

      if (_ds_match_attribute(agent_config, "Broken", "lineStripping")) {
        size_t len = strlen(buff);
  
        while (len>1 && buff[len-2]==13) {
          buff[len-2] = buff[len-1];
          buff[len-1] = 0;
          len--;
        }
      }
  
      if (line > 1 || strncmp (buff, "From QUARANTINE", 15))
      {
        if (_ds_match_attribute(agent_config, "ParseToHeaders", "on")) {
  
          /* Parse the To: address for a username */
          if (buff[0] == 0)
            body = 1;
          if (ATX->classification != -1 && !body && 
              !strncasecmp(buff, "To: ", 4))
          {
            char *y = NULL;
            if (ATX->classification == DSR_ISSPAM) {
              char *x = strstr(buff, "spam-");
              if (x != NULL) {
                y = strdup(x+5);
              }
            } else {
              char *x = strstr(buff, "fp-");
              if (x != NULL) {
                y = strdup(x+3);
              }
            }
    
            if (y) {
              char *ptrptr;
              char *z = strtok_r(y, "@", &ptrptr);
              nt_add (ATX->users, z);
              free(y);
            }
          }
        }
  
        if (buffer_cat (message, buff))
        {
          LOG (LOG_CRIT, ERROR_MEM_ALLOC);
          goto bail;
        }
      }
  
      /* Use the original user id if we are reversing a false positive */
      if (!strncasecmp (buff, "X-DSPAM-User: ", 14) && 
          ATX->managed_group[0] != 0                 &&
          ATX->operating_mode == DSM_PROCESS    &&
          ATX->classification == DSR_ISINNOCENT &&
          ATX->source         == DSS_ERROR)
      {
        char user[MAX_USERNAME_LENGTH];
        strlcpy (user, buff + 14, sizeof (user));
        chomp (user);
        nt_destroy (ATX->users);
        ATX->users = nt_create (NT_CHAR);
        if (ATX->users == NULL)
        {
          report_error (ERROR_MEM_ALLOC);
          goto bail;
        }
        nt_add (ATX->users, user);
      }
  
      line++;
    }
  }

  if (!message->used)
  {
    if (ATX->signature[0] != 0)
    {
      buffer_cat(message, "\n\n");
    }
    else { 
      LOG (LOG_INFO, "empty message (no data received)");
      goto bail;
    }
  }

  return message;

bail:
  buffer_destroy(message);
  return NULL;
}
