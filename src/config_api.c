/* $Id: config_api.c,v 1.21 2011/06/28 00:13:48 sbajic Exp $ */

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
 * config_api.c - configuration functions for populating libdspam properties 
 *
 * DESCRIPTION
 *   The functions in this section are used to manipulate the libdspam
 *   context using various APIs. Specifically, the properties API and the
 *   storage attach API are used here.
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <string.h>

#include "config_api.h"
#include "read_config.h"
#include "error.h"
#include "language.h"
#include "libdspam.h"
#include "util.h"

/*
 * set_libdspam_attributes(DSPAM_CTX *CTX)
 *
 * DESCRIPTION
 *   populate libdspam properties with relevant configuration values from
 *   dspam.conf. since dspam.conf is owned by the agent, libdspam doesn't read
 *   it directly; we rely on this api to configure the context.
 *
 * INPUT ARGUMENTS
 *      CTX	dspam context to configure
 *
 * RETURN VALUES
 *   returns 0 on success
 */

int set_libdspam_attributes(DSPAM_CTX *CTX) {
  attribute_t t;
  int i, ret = 0;
  char *profile;

  t = _ds_find_attribute(agent_config, "IgnoreHeader");
  while(t) {
    ret += dspam_addattribute(CTX, t->key, t->value);
    t = t->next;
  }

  profile = _ds_read_attribute(agent_config, "DefaultProfile");

  for(i=0;agent_config[i];i++) {
    t = agent_config[i];

    while(t) {

      if (!strncasecmp(t->key, "MySQL", 5)  ||
          !strncasecmp(t->key, "PgSQL", 5)  ||
          !strncasecmp(t->key, "SQLite", 6) ||
          !strcasecmp(t->key, "LocalMX")    ||
          !strncasecmp(t->key, "Storage", 7) ||
          !strncasecmp(t->key, "Processor", 9) ||
          !strncasecmp(t->key, "Hash", 4))
      {
        if (profile == NULL || profile[0] == 0)
        {
          ret += dspam_addattribute(CTX, t->key, t->value);
        }
        else if (strchr(t->key, '.'))
        {
          if (!strcasecmp((strchr(t->key, '.')+1), profile)) {
            char *x = strdup(t->key);
            char *y = strchr(x, '.');
            y[0] = 0;

            ret += dspam_addattribute(CTX, x, t->value);
            free(x);
          }
        }
      }
      t = t->next;
    }
  }

  ret += configure_algorithms(CTX);

  return ret;
}

/*
 * attach_context(DSPAM_CTX *CTX, void *dbh)
 *  
 * DESCRIPTION
 *   attach a database handle to an initialized dspam context
 *  
 * INPUT ARGUMENTS
 *      CTX    dspam context 
 *      dbh    database handle to attach
 *    
 * RETURN VALUES
 *   returns 0 on success
 */

int attach_context(DSPAM_CTX *CTX, void *dbh) {
  int maxtries = 1, tries = 0;
  int r;

  if (!_ds_read_attribute(agent_config, "DefaultProfile"))
    return dspam_attach(CTX, dbh);

  /* Perform failover if an attach fails */

  if (_ds_read_attribute(agent_config, "FailoverAttempts"))
    maxtries = atoi(_ds_read_attribute(agent_config, "FailoverAttempts"));

  r = dspam_attach(CTX, dbh);
  while (r && tries < maxtries) {
    char key[128];
    char *failover;

    snprintf(key, sizeof(key), "Failover.%s", _ds_read_attribute(agent_config, "DefaultProfile"));

    failover = _ds_read_attribute(agent_config, key);

    if (!failover) {
      LOG(LOG_ERR, ERR_AGENT_FAILOVER_OUT);
      return r;
    }

    LOG(LOG_WARNING, ERR_AGENT_FAILOVER, failover);
    _ds_overwrite_attribute(agent_config, "DefaultProfile", failover);

    if (dspam_clearattributes(CTX)) {
      LOG(LOG_ERR, ERR_AGENT_CLEAR_ATTRIB);
      return r;
    }

    set_libdspam_attributes(CTX);

    tries++;
    r = dspam_attach(CTX, dbh);
  }

  return r;
}
