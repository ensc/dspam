/* $Id: dspamc.c,v 1.1 2004/12/03 01:30:32 jonz Exp $ */

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

#include "dspamc.h"
#include "agent_shared.h"
#include "pref.h"
#include "libdspam.h"
#include "language.h"
#include "buffer.h"
#include "pref.h"
#include "error.h"

#ifdef DEBUG
int DO_DEBUG;
char debug_text[1024];
#endif

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
  int agent_init = 0;		/* Agent is initialized */

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

  if (_ds_read_attribute(agent_config, "ClientIdent")) {
    exitcode = client_process(&ATX, message);
  } else {
    report_error(ERROR_INVALID_CLIENT_CONFIG);
    exitcode = EINVAL;
  }

bail:

  if (message)
    buffer_destroy(message);

  if (agent_init)
    nt_destroy(ATX.users);

  if (agent_config)
    _ds_destroy_attributes(agent_config);

  exit (exitcode);
}

