/* $Id: dspam_stats.c,v 1.2 2004/12/01 17:29:11 jonz Exp $ */

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
#include "language.h"

#define TSYNTAX	"syntax: dspam_stats [username]"

DSPAM_CTX *open_ctx, *open_mtx;
int opt_humanfriendly;

int stat_user (const char *username);
int process_all_users (void);
void dieout (int signal);
void usage (void);

int
main (int argc, char **argv)
{
  int ch,i,users = 0;
#ifndef HAVE_GETOPT
  int optind = 1;
#endif

#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  struct passwd *p = getpwuid (getuid ());
                                                                                
#endif
#endif

 /* Read dspam.conf */
                                                                                
  agent_config = read_config(NULL);
  if (!agent_config) {
    report_error(ERROR_READ_CONFIG);
    exit(EXIT_FAILURE);
  }
                                                                                
  if (!_ds_read_attribute(agent_config, "Home")) {
    report_error(ERROR_DSPAM_HOME);
    _ds_destroy_attributes(agent_config);
    exit(EXIT_FAILURE);
  }
                                                                                
#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  if (!_ds_match_attribute(agent_config, "Trust", p->pw_name) && p->pw_uid) {
    fprintf(stderr, ERROR_TRUSTED_MODE "\n");
    _ds_destroy_attributes(agent_config);
    exit(EXIT_FAILURE);
  }
#endif
#endif

  for(i=0;i<argc;i++) {
                                                                                
    if (!strncmp (argv[i], "--profile=", 10))
    {
      if (!_ds_match_attribute(agent_config, "Profile", argv[i]+10)) {
        report_error_printf(ERROR_NO_SUCH_PROFILE, argv[i]+10);
        _ds_destroy_attributes(agent_config);
        exit(EXIT_FAILURE);
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
#ifdef HAVE_GETOPT
  while((ch = getopt(argc, argv, "hH")) != -1)
#else
  while ( argv[optind] &&
            argv[optind][0] == '-' &&
              (ch = argv[optind][1]) &&
                argv[optind][2] == '\0' )
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
  /* reset our option array and index to where we are after getopt */
  argv += optind;
  argc -= optind;

  /* process arguments */
  for (i=0; i < argc; i++)
  {
      if (strncmp(argv[i], "--", 2)) {
        stat_user(argv[i]);
        users++;
      }
  }

  if (!users)
    process_all_users ();

  dspam_shutdown_driver (NULL);
  _ds_destroy_attributes(agent_config);
  exit (EXIT_SUCCESS);
}

int
process_all_users (void)
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
    dspam_destroy(CTX);
    return EFAILURE;
  }

  user = _ds_get_nextuser (CTX);
  while (user != NULL)
  {
    stat_user (user);
    user = _ds_get_nextuser (CTX);
  }

  dspam_destroy (CTX);
  open_ctx = NULL;
  return 0;
}

int
stat_user (const char *username)
{
  DSPAM_CTX *MTX;
  long total_spam, total_innocent, spam_misclassified, innocent_misclassified, spam_corpusfed, innocent_corpusfed;

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
    return EUNKNOWN;
  }


  /* Convenience variables. Compiling with optimization will cause this to have 0 slowdown,
   * as it is essentially dead code */
  total_spam =
        MAX(0, (MTX->totals.spam_learned + MTX->totals.spam_classified) -
          (MTX->totals.spam_misclassified + MTX->totals.spam_corpusfed));
  total_innocent =
        MAX(0, (MTX->totals.innocent_learned + MTX->totals.innocent_classified) - 
          (MTX->totals.innocent_misclassified + MTX->totals.innocent_corpusfed));
  spam_misclassified = MTX->totals.spam_misclassified;
  innocent_misclassified = MTX->totals.innocent_misclassified;
  spam_corpusfed = MTX->totals.spam_corpusfed;
  innocent_corpusfed = MTX->totals.innocent_corpusfed;

  if (opt_humanfriendly)
  {
    (void)printf("%s:\n\
        \tTS Total Spam:             %6ld\n\
        \tTI Total Innocent:         %6ld\n\
        \tSM Spam Misclassified:     %6ld\n\
        \tIM Innocent Misclassified: %6ld\n\
        \tSC Spam Corpusfed:         %6ld\n\
        \tIC Innocent Corpusfed:     %6ld\n\
        \tTL Training Left:          %6ld\n\
        \n",
            username,
            total_spam, total_innocent,
            spam_misclassified, innocent_misclassified,
            spam_corpusfed, innocent_corpusfed, 
            MAX(0, 2500 - (MTX->totals.innocent_learned +
                           MTX->totals.innocent_classified))
    );
  }
  else
  {
    (void)printf ("%-16s  TS:% 6ld TI:% 6ld SM:% 6ld IM:% 6ld SC:% 6ld IC:% 6ld\n",
            username,
            total_spam, total_innocent,
            spam_misclassified, innocent_misclassified,
            spam_corpusfed, innocent_corpusfed);
            
  }

  dspam_destroy (MTX);
  open_mtx = NULL;
  return 0;
}

void
dieout (int signal)
{
  fprintf (stderr, "terminated.\n");
  if (open_ctx != NULL)
    dspam_destroy (open_ctx);
  if (open_mtx != NULL)
    dspam_destroy (open_mtx);
  _ds_destroy_attributes(agent_config);
  exit (EXIT_SUCCESS);
}

void
usage (void)
{
  (void)fprintf (stderr,
    "usage: dspam_stats [-hH] [user [user...]]\n\
      \tPrint dspam statistics for users.\n\
      \tIf no users are specified, stats for all users are printed.\n\
      \t-h: print this message\n\
      \t-H: print stats in \"human friendly\" format\n");
  _ds_destroy_attributes(agent_config);
  exit(EXIT_FAILURE);
}
