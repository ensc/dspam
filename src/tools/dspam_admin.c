/* $Id: dspam_admin.c,v 1.3 2004/12/18 15:02:52 jonz Exp $ */

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
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "config.h"

#include "libdspam.h"
#include "read_config.h"
#include "language.h"
#ifdef PREFERENCES_EXTENSION
#include "pref.h"
#endif 

#define TSYNTAX	"syntax: dspam_admin [function] [arguments] [--profile=PROFILE]"

/* 
  PREFERENCE FUNCTIONS

  Add a Preference:	dspam_admin add preference [user] [attr] [value]
  Set a Preference:	dspam_admin change preference [user] [attr] [value]
  Delete a Preference:	dspam_admin delete preference [user] [attr]
  List Preferences:     dspam_admin list preferences [user]

*/

int set_preference_attribute(const char *, const char *, const char *);
int del_preference_attribute(const char *, const char *);
int list_preference_attributes(const char *);

void dieout (int signal);
void usage (void);
void min_args(int argc, int min);

int
main (int argc, char **argv)
{
#ifdef TRUSTED_USER_SECURITY
  struct passwd *p = getpwuid (getuid ());
#endif
  int i, valid = 0;

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

  signal (SIGINT, dieout);
  signal (SIGPIPE, dieout);
  signal (SIGTERM, dieout);

  dspam_init_driver (NULL);

  if (argc < 3 || !strcmp(argv[1], "help"))
  {
    usage();
  }

#ifdef PREFERENCES_EXTENSION
  /* PREFERENCE FUNCTIONS */
  if (!strncmp(argv[2], "pref", 4)) {
 
    /* Delete */
    if (!strncmp(argv[1], "d", 1)) {
      min_args(argc, 4);
      valid = 1;
      del_preference_attribute(argv[3], argv[4]);
    }

    /* Add, Change */
    if (!strncmp(argv[1], "ch", 2) || !strncmp(argv[1], "a", 1)) {
      min_args(argc, 5);
      valid = 1; 
      set_preference_attribute(argv[3], argv[4], argv[5]); 
    }

    /* List */
    if (!strncmp(argv[1], "l", 1)) {
      min_args(argc, 3);
      valid = 1;
      list_preference_attributes(argv[3]);
    }
  }
#endif

  if (!valid) 
    usage();

  dspam_shutdown_driver (NULL);
  _ds_destroy_attributes(agent_config);
  exit (EXIT_SUCCESS);
}

void
dieout (int signal)
{
  fprintf (stderr, "terminated.\n");
  _ds_destroy_attributes(agent_config);
  exit (EXIT_SUCCESS);
}

void
usage (void)
{
  fprintf(stderr, "%s\n", TSYNTAX);

#ifdef PREFERENCES_EXTENSION
  fprintf(stderr, "\tadd preference [user] [attrib] [value]\n");
  fprintf(stderr, "\tchange preference [user] [attrib] [value]\n");
  fprintf(stderr, "\tdelete preference [user] [attrib] [value]\n");
  fprintf(stderr, "\tlist preference [user] [attrib] [value]\n");
#endif
  _ds_destroy_attributes(agent_config);
  exit(EXIT_FAILURE);
}

void min_args(int argc, int min) {
  if (argc<(min+1)) {
    fprintf(stderr, "invalid command syntax.\n");
    usage();
    _ds_destroy_attributes(agent_config);
    exit(EXIT_FAILURE);
  }
  return;
}

#ifdef PREFERENCES_EXTENSION
int set_preference_attribute(
  const char *username, 
  const char *attr, 
  const char *value)
{
  int i;

  if (username[0] == 0) 
    i = _ds_pref_set(agent_config, NULL, _ds_read_attribute(agent_config, "Home"), attr, value, NULL);
  else
    i = _ds_pref_set(agent_config, username, _ds_read_attribute(agent_config, "Home"), attr, value, NULL);

  if (!i) 
    printf("operation successful.\n");
  else
    printf("operation failed with error %d.\n", i);

  return i;
}

int del_preference_attribute(
  const char *username,
  const char *attr)
{
  int i;

  if (username[0] == 0)
    i = _ds_pref_del(agent_config, NULL, _ds_read_attribute(agent_config, "Home"), attr, NULL);
  else
    i = _ds_pref_del(agent_config, username, _ds_read_attribute(agent_config, "Home"), attr, NULL);

  if (!i)
    printf("operation successful.\n");
  else
    printf("operation failed with error %d.\n", i);

  return i;
}

int list_preference_attributes(const char *username)
{
  AGENT_PREF PTX;
  AGENT_ATTRIB *pref;
  int i;
  
  if (username[0] == 0)
    PTX = _ds_pref_load(agent_config, NULL, _ds_read_attribute(agent_config, "Home"), NULL);
  else
    PTX = _ds_pref_load(agent_config, username,  _ds_read_attribute(agent_config, "Home"), NULL);

  if (PTX == NULL) 
    return 0;

  for(i=0;PTX[i];i++) {
    pref = PTX[i];
    printf("%s=%s\n", pref->attribute, pref->value);
  }

  _ds_pref_free(PTX);
  return 0;
}
#endif
