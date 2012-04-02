/* $Id: dspam_stats.c,v 1.36 2011/06/28 00:13:48 sbajic Exp $ */

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

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#endif
#include "config.h"

#include "libdspam.h"
#include "read_config.h"
#include "config_api.h"
#include "language.h"
#include "util.h"

#define TSYNTAX	"syntax: dspam_stats [-h]\|[--profile=PROFILE] [-HrsSt] [user [user...]]"

#ifdef _WIN32
/* no trusted users under Windows */
#undef TRUSTED_USER_SECURITY
#endif

DSPAM_CTX *open_ctx, *open_mtx;
int opt_humanfriendly;
int opt_reset;
int opt_snapshot;
int opt_stats;
int opt_total;

int stat_user (const char *username, struct _ds_spam_totals *totals);
int process_all_users (struct _ds_spam_totals *totals);
void dieout (int signal);
void usage (void);

int
main (int argc, char **argv)
{
  int ch, i, users = 0;
#ifndef HAVE_GETOPT
  int optind = 1;
#endif
  struct _ds_spam_totals totals;

#ifdef TRUSTED_USER_SECURITY
  struct passwd *p = getpwuid (getuid ());
  int trusted = 0;                                                                                
#endif


 memset(&totals, 0, sizeof(struct _ds_spam_totals));

 /* Read dspam.conf */
                                                                                
  agent_config = read_config(NULL);
  if (!agent_config) {
    LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
    fprintf (stderr, ERR_AGENT_READ_CONFIG "\n");
    exit(EXIT_FAILURE);
  }
                                                                                
  if (!_ds_read_attribute(agent_config, "Home")) {
    LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
    fprintf (stderr, ERR_AGENT_DSPAM_HOME "\n");
    _ds_destroy_config(agent_config);
    exit(EXIT_FAILURE);
  }
                                                                                
  if (libdspam_init(_ds_read_attribute(agent_config, "StorageDriver")) != 0) {
    LOG(LOG_ERR, ERR_DRV_INIT);
    fprintf (stderr, ERR_DRV_INIT "\n");
    _ds_destroy_config(agent_config);
    exit(EXIT_FAILURE);
  }

#ifdef TRUSTED_USER_SECURITY
  if (_ds_match_attribute(agent_config, "Trust", p->pw_name) || !p->pw_uid) {
    trusted = 1;
  }
#endif

  for(i=0;i<argc;i++) {
    if (!strncmp (argv[i], "--profile=", 10))
    {
#ifdef TRUSTED_USER_SECURITY
      if (!trusted) {
        LOG(LOG_ERR, ERR_TRUSTED_PRIV, "--profile", p->pw_uid, p->pw_name);
        fprintf (stderr, ERR_TRUSTED_PRIV "\n", "--profile", p->pw_uid, p->pw_name);
        _ds_destroy_config(agent_config);
        goto BAIL;
      }
#endif
      if (!_ds_match_attribute(agent_config, "Profile", argv[i]+10)) {
        LOG(LOG_ERR, ERR_AGENT_NO_SUCH_PROFILE, argv[i]+10);
        fprintf (stderr, ERR_AGENT_NO_SUCH_PROFILE "\n", argv[i]+10);
        _ds_destroy_config(agent_config);
        goto BAIL;
      } else {
        _ds_overwrite_attribute(agent_config, "DefaultProfile", argv[i]+10);
      }
      break;
    }
  }

  open_ctx = open_mtx = NULL;

  signal (SIGINT, dieout);
#ifndef _WIN32
  signal (SIGPIPE, dieout);
#endif
  signal (SIGTERM, dieout);

  dspam_init_driver (NULL);

  /* Process command line */
  ch = opt_humanfriendly = 0;
  opt_reset = opt_snapshot = opt_stats = opt_total =  0;

#ifndef HAVE_GETOPT
  while ( argv[optind] &&
            argv[optind][0] == '-' &&
              (ch = argv[optind][1]) &&
                argv[optind][2] == '\0' )
#else
  while((ch = getopt(argc, argv, "hHrsS")) != -1)
#endif
  {
    switch(ch) {
      case 'h':
        /* print help, and then exit. usage exits for us */
        usage();
        break;
      case 'H':
        opt_humanfriendly = 1;
        break;
      case 'r':
        opt_reset = 1;
        break;
      case 's':
        opt_snapshot = 1;
        break;
      case 'S':
        opt_stats = 1;
        break;
      case 't':
        opt_total = 1;
        break;

#ifndef HAVE_GETOPT
      default:
        fprintf(stderr, "%s: unknown option \"%s\".\n",
                argv[0], argv[optind] + 1);
        usage();
#endif
    }
#ifndef HAVE_GETOPT
    optind++;
#endif
  }
#ifndef HAVE_GETOPT
  /* reset our option array and index to where we are after getopt */
  argv += optind;
  argc -= optind;
#endif

  /* process arguments */
  for (i=0; i < argc; i++)
  {
      if (argv[i] && strncmp(argv[i], "--", 2)) {
#ifdef TRUSTED_USER_SECURITY
        if ( !trusted && strcmp(argv[i], p->pw_name) )
        {
          fprintf(stderr, ERR_TRUSTED_MODE "\n");
          _ds_destroy_config(agent_config);
          goto BAIL;
        }
#endif
        stat_user(argv[i], &totals);
        users++;
      }
  }

  if (!users)
  {
#ifdef TRUSTED_USER_SECURITY
    if ( !trusted )
    {
      fprintf(stderr, ERR_TRUSTED_MODE "\n");
      _ds_destroy_config(agent_config);
      goto BAIL;
    }
#endif

    process_all_users (&totals);
  }

  if (opt_total)
    stat_user(NULL, &totals);
  dspam_shutdown_driver (NULL);
  _ds_destroy_config(agent_config);
  libdspam_shutdown();
  exit (EXIT_SUCCESS);

BAIL:
  libdspam_shutdown();
  exit(EXIT_FAILURE);
}

int
process_all_users (struct _ds_spam_totals *totals)
{
  DSPAM_CTX *CTX;
  char *user;

  CTX = dspam_create (NULL, NULL, _ds_read_attribute(agent_config, "Home"), DSM_TOOLS, 0);
  open_ctx = CTX;
  if (CTX == NULL)
  {
    fprintf (stderr, "Could not initialize context: %s\n", strerror (errno));
    return EFAILURE;
  }

  set_libdspam_attributes(CTX);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    fprintf (stderr, "Unable to attach DSPAM context\n");
    dspam_destroy(CTX);
    return EFAILURE;
  }

  user = _ds_get_nextuser (CTX);
  while (user != NULL)
  {
    stat_user (user, totals);
    user = _ds_get_nextuser (CTX);
  }

  dspam_destroy (CTX);
  open_ctx = NULL;
  return 0;
}

int
stat_user (const char *username, struct _ds_spam_totals *totals)
{
  DSPAM_CTX *MTX = NULL;
  long total_spam, total_innocent, spam_misclassified, innocent_misclassified, spam_corpusfed, innocent_corpusfed, all_spam, all_innocent;
  char filename[MAX_FILENAME_LENGTH];
  FILE *file;
  struct _ds_spam_totals *tptr;

  if (username) {
    MTX = dspam_create (username, NULL, _ds_read_attribute(agent_config, "Home"), DSM_CLASSIFY, 0);
    open_mtx = MTX;
    if (MTX == NULL)
    {
      fprintf (stderr, "Could not init context: %s\n", strerror (errno));
      return EUNKNOWN;
    }
    set_libdspam_attributes(MTX);
    if (dspam_attach(MTX, NULL)) {
      LOG (LOG_WARNING, "unable to attach dspam context");
      fprintf (stderr, "Unable to attach DSPAM context\n");
      return EUNKNOWN;
    }
    tptr = &MTX->totals;
  } else {
    tptr = totals;
  }

  /* Convenience variables. Compiling with optimization will cause this to 
     have 0 slowdown, as it is essentially dead code */
  total_spam =
      MAX(0, (tptr->spam_learned + tptr->spam_classified) -
        (tptr->spam_misclassified + tptr->spam_corpusfed));
  total_innocent =
      MAX(0, (tptr->innocent_learned + tptr->innocent_classified) - 
        (tptr->innocent_misclassified + tptr->innocent_corpusfed));
  spam_misclassified     = tptr->spam_misclassified;
  innocent_misclassified = tptr->innocent_misclassified;
  spam_corpusfed         = tptr->spam_corpusfed;
  innocent_corpusfed     = tptr->innocent_corpusfed;

  if (MTX) {
    totals->spam_learned += MTX->totals.spam_learned;
    totals->innocent_learned += MTX->totals.innocent_learned;
    totals->spam_misclassified += MTX->totals.spam_misclassified;
    totals->innocent_misclassified += MTX->totals.innocent_misclassified;
    totals->spam_corpusfed += MTX->totals.spam_corpusfed;
    totals->innocent_corpusfed += MTX->totals.innocent_corpusfed; 
  }

  /* Subtract the snapshot from the current totals to get stats "since last
     reset" for the user */

  if (opt_snapshot && username) {
    long s_total_spam, s_total_innocent, s_spam_misclassified,
         s_innocent_misclassified, s_spam_corpusfed, s_innocent_corpusfed;

    _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"),
                     username, "rstats");
    _ds_prepare_path_for (filename);

    file = fopen (filename, "r");
    if (file != NULL) {
      if (fscanf(file, "%ld,%ld,%ld,%ld,%ld,%ld", 
                 &s_total_spam,
                 &s_total_innocent,
                 &s_spam_misclassified,
                 &s_innocent_misclassified,
                 &s_spam_corpusfed,
                 &s_innocent_corpusfed)==6) {
        total_spam             -= s_total_spam;
        total_innocent         -= s_total_innocent;
        spam_misclassified     -= s_spam_misclassified;
        innocent_misclassified -= s_innocent_misclassified;
        spam_corpusfed         -= s_spam_corpusfed;
        innocent_corpusfed     -= s_innocent_corpusfed;
      }
      fclose(file);
    }
  }

  all_spam = total_spam + spam_misclassified,
  all_innocent = total_innocent + innocent_misclassified;

  if (opt_humanfriendly)
  {
    printf("%s:\n\
        \tTP True Positives:                %6ld\n\
        \tTN True Negatives:                %6ld\n\
        \tFP False Positives:               %6ld\n\
        \tFN False Negatives:               %6ld\n\
        \tSC Spam Corpusfed:                %6ld\n\
        \tNC Nonspam Corpusfed:             %6ld\n\
        \tTL Training Left:                 %6ld\n\
        \tSHR Spam Hit Rate               % 7.2f%%\n\
        \tHSR Ham Strike Rate:            % 7.2f%%\n\
        \tPPV Positive predictive value:  % 7.2f%%\n\
        \tOCA Overall Accuracy:           % 7.2f%%\n\
        \n",
            (username) ? username : "TOTAL",
            total_spam, total_innocent,
            innocent_misclassified, spam_misclassified, 
            spam_corpusfed, innocent_corpusfed, 
            MAX(0, 2500 - (tptr->innocent_learned +
                           tptr->innocent_classified)),
       (all_spam) ?
          (100.0-((float)spam_misclassified / (float)all_spam )*100.0)
          : 100.0,
        (all_innocent) ?
          100-(100.0-((float)innocent_misclassified / (float)all_innocent )*100.0)
          : 100.0,
        (total_spam + innocent_misclassified) ?
          100-(100.0-((float)total_spam /
                  (float)(total_spam + innocent_misclassified))*100)
          : 100.0,
        (all_spam + all_innocent) ?
          (100.0-(((float)spam_misclassified +(float)innocent_misclassified) /
                   (float)(all_spam + all_innocent))*100.0)
          : 100.0);
  }
  else
  {
#ifdef LONG_USERNAMES
    printf ("%s\n    TP:%6ld TN:%6ld FP:%6ld FN:%6ld SC:%6ld NC:%6ld\n",
#else
    printf ("%-16s  TP:%6ld TN:%6ld FP:%6ld FN:%6ld SC:%6ld NC:%6ld\n",
#endif
            (username) ? username : "TOTAL",
            total_spam, total_innocent,
            innocent_misclassified, spam_misclassified,
            spam_corpusfed, innocent_corpusfed);

    if (opt_stats) 
      printf (
#ifdef LONG_USERNAMES
"    "
#else
"                  "
#endif
              "SHR: % 7.2f%%       HSR: % 7.2f%%       OCA: % 7.2f%%\n",
        (all_spam) ?
          (100.0-((float)spam_misclassified / (float)all_spam )*100.0) 
          : 100.0,
        (all_innocent) ? 
          100.0-
          (100.0-((float)innocent_misclassified / (float)all_innocent )*100.0)
          : 0.0,
        (all_spam + all_innocent) ? 
          (100.0-(((float)spam_misclassified +(float)innocent_misclassified) / 
                   (float)(all_spam + all_innocent))*100.0) 
          : 100.0);
  }

  if (opt_reset && username) {
    _ds_userdir_path(filename, _ds_read_attribute(agent_config, "Home"), 
                     username, "rstats");
    _ds_prepare_path_for (filename);
    file = fopen (filename, "w");
    if (file == NULL)
    {
      LOG(LOG_ERR, ERR_IO_FILE_WRITE, filename, strerror (errno));
      if (MTX)
        dspam_destroy (MTX);
      open_mtx = NULL;
      return EFILE;
    }

    fprintf (file, "%ld,%ld,%ld,%ld,%ld,%ld\n",
     MAX(0,(tptr->spam_learned + tptr->spam_classified) -
       (tptr->spam_misclassified + tptr->spam_corpusfed)),
     MAX(0,(tptr->innocent_learned + tptr->innocent_classified) -
       (tptr->innocent_misclassified + tptr->innocent_corpusfed)),
     tptr->spam_misclassified, 
     tptr->innocent_misclassified,
     tptr->spam_corpusfed, 
     tptr->innocent_corpusfed);
    fclose(file);
  }

  if (MTX)
    dspam_destroy (MTX);
  open_mtx = NULL;
  return 0;
}

void
dieout (int signal)
{
  signal = signal; /* Keep compile happy */
  fprintf (stderr, "terminated.\n");
  if (open_ctx != NULL)
    dspam_destroy (open_ctx);
  if (open_mtx != NULL)
    dspam_destroy (open_mtx);
  _ds_destroy_config(agent_config);
  exit (EXIT_SUCCESS);
}

void
usage (void)
{
  (void)fprintf (stderr,
    "usage: dspam_stats [-h]|[--profile=PROFILE] [-HrsSt] [user [user...]]\n\
      \tPrint dspam statistics for users.\n\
      \tIf no users are specified, stats for all users are printed.\n\
      \t-h: print this message\n\
      \t-H: print stats in \"human friendly\" format\n\
      \t-r: Resets the current snapshot\n\
      \t-s: Displays stats since last snapshot (instead of since epoch)\n\
      \t-S: Displays accuracy percentages in addition to stats\n\
      \t-t: Displays a total of all statistics displayed\n");
  _ds_destroy_config(agent_config);
  exit(EXIT_FAILURE);
}
