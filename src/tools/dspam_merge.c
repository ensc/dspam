/* $Id: dspam_merge.c,v 1.8 2005/04/25 13:05:48 jonz Exp $ */

/*
 DSPAM
 COPYRIGHT (C) 2002-2005 DEEP LOGIC INC.

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
#include <sys/param.h>
#include "config.h"
#include "util.h"

#include "libdspam.h"
#include "language.h"
#include "read_config.h"
#include "config_api.h"

#define TSYNTAX	"syntax: dspam_merge [user1] [user2] ... [userN] [-o user]"

DSPAM_CTX *open_ctx, *open_mtx;
void dieout (int signal);

int
main (int argc, char **argv)
{
  char destuser[MAX_USERNAME_LENGTH];
  struct nt *users = NULL;
  struct nt_node *node_nt;
  struct nt_c c_nt;
  struct _ds_storage_record *token;
  struct _ds_spam_stat s;
  ds_diction_t merge;
  DSPAM_CTX *CTX, *MTX;
  long i;
#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  struct passwd *p = getpwuid (getuid ());

#endif
#endif

 /* Read dspam.conf */
                                                                                
  agent_config = read_config(NULL);
  if (!agent_config) {
    LOG(LOG_ERR, ERR_AGENT_READ_CONFIG);
    exit(EXIT_FAILURE);
  }
                                                                                
  if (!_ds_read_attribute(agent_config, "Home")) {
    LOG(LOG_ERR, ERR_AGENT_DSPAM_HOME);
    _ds_destroy_config(agent_config);
    exit(EXIT_FAILURE);
  }
                                                                                
#ifndef _WIN32
#ifdef TRUSTED_USER_SECURITY
  if (!_ds_match_attribute(agent_config, "Trust", p->pw_name) && p->pw_uid) {
    fprintf(stderr, ERR_TRUSTED_MODE "\n");
    _ds_destroy_config(agent_config);
    exit(EXIT_FAILURE);
  }
#endif
#endif

  if (argc < 4)
  {
    printf ("%s\n", TSYNTAX);
    _ds_destroy_config(agent_config);
    exit(EXIT_FAILURE);
  }

  open_ctx = open_mtx = NULL;
  signal (SIGINT, dieout);
  signal (SIGPIPE, dieout);
  signal (SIGTERM, dieout);

  dspam_init_driver (NULL);
  users = nt_create (NT_CHAR);
  merge = ds_diction_create(196613);

  if (users == NULL || merge == NULL)
  {
    fprintf (stderr, ERR_MEM_ALLOC);
    goto bail;
  }

  for (i = 1; i < argc; i++)
  {

    if (!strncmp (argv[i], "--profile=", 10))
    {
      if (!_ds_match_attribute(agent_config, "Profile", argv[i]+10)) {
        LOG(LOG_ERR, ERR_AGENT_NO_SUCH_PROFILE, argv[i]+10);
        goto bail;
      } else {
        _ds_overwrite_attribute(agent_config, "DefaultProfile", argv[i]+10);
      }
      continue;
    }

    if (!strcmp (argv[i], "-o"))
    {
      strlcpy (destuser, argv[i + 1], sizeof (destuser));
      i += 1;
      continue;
    }
    nt_add (users, argv[i]);
  }

  CTX = dspam_create (destuser, NULL, _ds_read_attribute(agent_config, "Home"), DSM_TOOLS, 0);
  open_ctx = CTX;
  if (CTX == NULL)
  {
    fprintf (stderr, "unable to initialize context: %s\n", strerror (errno));
    goto bail;
  }

  set_libdspam_attributes(CTX);
  if (dspam_attach(CTX, NULL)) {
    LOG (LOG_WARNING, "unable to attach dspam context");
    goto bail;
  }

  node_nt = c_nt_first (users, &c_nt);
  while (node_nt != NULL)
  {
#ifdef DEBUG
    printf ("Merging user: %s\n", (const char *) node_nt->ptr);
#endif
    MTX = dspam_create ((const char *) node_nt->ptr, NULL, _ds_read_attribute(agent_config, "Home"), DSM_CLASSIFY, 0);
    open_mtx = MTX;
    if (MTX == NULL)
    {
      fprintf (stderr, "unable to initialize context: %s\n",
               strerror (errno));
      node_nt = c_nt_next (users, &c_nt);
      continue;
    }
  
    set_libdspam_attributes(MTX);
    if (dspam_attach(MTX, NULL)) {
      LOG (LOG_WARNING, "unable to attach dspam context");
      goto bail;
    }

    CTX->totals.spam_learned += MTX->totals.spam_learned;
    CTX->totals.innocent_learned += MTX->totals.innocent_learned;
    CTX->totals.spam_corpusfed += MTX->totals.spam_learned;
    CTX->totals.innocent_corpusfed += MTX->totals.innocent_learned;
    CTX->totals.spam_misclassified = 0;
    CTX->totals.innocent_misclassified = 0;
    i = 0;
    token = _ds_get_nexttoken (MTX);
    while (token != NULL)
    {
      if (ds_diction_getstat (merge, token->token, &s))
      {
        s.spam_hits = token->spam_hits;
        s.innocent_hits = token->innocent_hits;
        s.status &= ~TST_DISK;
      }
      else
      {
        s.spam_hits += token->spam_hits;
        s.innocent_hits += token->innocent_hits;
      }
      ds_diction_setstat (merge, token->token, &s);
      free (token);
      i++;
      token = _ds_get_nexttoken (MTX);
    }
#ifdef DEBUG
    printf ("processed %ld tokens\n", i);
#endif
    node_nt = c_nt_next (users, &c_nt);
    dspam_destroy (MTX);
    open_mtx = NULL;
  }

#ifdef DEBUG
  printf ("storing merged tokens...\n");
#endif
  _ds_setall_spamrecords (CTX, merge);
#ifdef DEBUG
  printf ("completed.\n");
#endif
  nt_destroy (users);
  ds_diction_destroy(merge);
  dspam_destroy (CTX);
  open_ctx = NULL;
  dspam_shutdown_driver (NULL);
  _ds_destroy_config(agent_config);
  exit (EXIT_SUCCESS);

bail:
  if (open_ctx != NULL)
    dspam_destroy (open_ctx);
  if (open_mtx != NULL)
    dspam_destroy (open_mtx);
  dspam_shutdown_driver (NULL);
  nt_destroy(users);
  ds_diction_destroy(merge);
  _ds_destroy_config(agent_config);
  exit (EXIT_FAILURE);
}

void
dieout (int signal)
{
  fprintf (stderr, "terminated.\n");
  if (open_ctx != NULL)
    dspam_destroy (open_ctx);
  if (open_mtx != NULL)
    dspam_destroy (open_mtx);
  _ds_destroy_config(agent_config);
  exit (EXIT_SUCCESS);
}

