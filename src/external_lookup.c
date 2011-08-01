/* $Id: external_lookup.c,v 0.6.1 2009/12/31 04:00:01 sbajic Exp $ */

/*
 COPYRIGHT (C) 2006 HUGO MONTEIRO

 external lookup library for DSPAM v0.6
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2
 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#ifdef EXT_LOOKUP
#define LDAP_DEPRECATED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include "libdspam.h"
#include "external_lookup.h"
#include "config.h"
#include "language.h"
#include "error.h"
#include "config_shared.h"

/* LDAP */
#ifdef USE_LDAP
#   include <ldap.h>
#   define BIND_TIMEOUT	10
#endif

void 
sig_alrm(int signum)
{
   signum = signum;  /* Keep compiler happy */
   LOG(LOG_ERR,"%s: Timed out.", ERR_EXT_LOOKUP_INIT_FAIL);
   exit(200);
}


char*
external_lookup(config_t agent_config, const char *username, char *external_uid)
{

	char *driver = _ds_read_attribute(agent_config, "ExtLookupDriver");

	if (strcmp(driver, "ldap") == 0) {
#ifdef USE_LDAP
		return ldap_lookup(agent_config, username, external_uid);
#else
		LOG(LOG_ERR, "external_lookup: LDAP driver was not enabled at compile time.");
		return NULL;
#endif
	} else if (strcmp(driver, "program") == 0) {
		return program_lookup(agent_config, username, external_uid);
	/* add here your 'else if' statements like the one above to extend */
	} else if (driver == NULL) {
		LOG(LOG_ERR, "external_lookup: lookup driver not defined");
		return NULL;
	} else {
		LOG(LOG_ERR, "external_lookup: lookup driver %s not yet implemented.", driver);
		return NULL;
	}
}


char
*transcode_query(const char *query, const char *username, char *transcoded_query)
{	
	
	char *saveptr = NULL, *token;
	int i, j, len, replacements;
	int namelen = strlen(username);
	int querylen = strlen(query);
	char *str = malloc (querylen);

	/* count aprox. the number of replacements.
	 * %% escaping is also accounted and shouldn't
	 * in the TODO */
	for (replacements = 0, str=strdup(query); ; str = NULL) {
		token = strtok_r(str, "%", &saveptr);
		if (token == NULL)
			break;
		if (token[0] == 'u') {
			replacements++;
		}
	}

	free(str);
	
	len = querylen + namelen * replacements - 2 * replacements + 1;

	transcoded_query = malloc (len);
	memset(transcoded_query, 0, len);

    for (i=j=0;j<len && query[i]; ){
		if (query[i] == '%') {
		    switch (query[i+1]) {
				case '%': /* escaped '%' character */
				    transcoded_query[j++] = '%';
				    break;
				case 'u': /* paste in the username */
				    if (j+namelen>=len) {
						LOG(LOG_ERR, "%s: check ExtLookupQuery", ERR_EXT_LOOKUP_MISCONFIGURED);
						return NULL;
				    }
				    memcpy (transcoded_query+j, username, namelen);
				    j += namelen;
				    break;
				default: /* unrecognised formatting, abort!  */
					LOG(LOG_ERR, "%s: check ExtLookupQuery", ERR_EXT_LOOKUP_MISCONFIGURED);
			    	return NULL;
		    }
		    i += 2;
		} else {
		    transcoded_query[j++] = query[i++];
		}
	}

    if (j>=len) {
		LOG(LOG_ERR, "%s: check ExtLookupQuery", ERR_EXT_LOOKUP_MISCONFIGURED);
		return NULL;
    }
    /* and finally zero terminate string */
    transcoded_query[j] = 0;
	
	return transcoded_query;
}

#ifdef USE_LDAP
char*
ldap_lookup(config_t agent_config, const char *username, char *external_uid)
{
	LDAP		*ld;
	LDAPMessage	*result = (LDAPMessage *) 0;
	LDAPMessage	*e;
	BerElement	*ber;
	char		*a, *dn;
	char		*sane_username = malloc(strlen(username)*2);
	char		*p = sane_username;
	char		**vals = NULL;
	struct		timeval	ldaptimeout = {.tv_sec = BIND_TIMEOUT, .tv_usec = 0};
	int			i, rc=0, num_entries=0;
	char		*transcoded_query = NULL;
	char		*ldap_uri = NULL;
	char		*end_ptr;
	char		*ldap_host = _ds_read_attribute(agent_config, "ExtLookupServer");
	char		*port = _ds_read_attribute(agent_config, "ExtLookupPort");
	long		lldap_port;
	int			ldap_port = 389;
	char		*ldap_binddn = _ds_read_attribute(agent_config, "ExtLookupLogin");
	char		*ldap_passwd = _ds_read_attribute(agent_config, "ExtLookupPassword");
	char		*ldap_base = _ds_read_attribute(agent_config, "ExtLookupDB");
	char		*ldap_attrs[] = {_ds_read_attribute(agent_config, "ExtLookupLDAPAttribute"),0};
	char		*version = _ds_read_attribute(agent_config, "ExtLookupLDAPVersion");
	long		lldap_version;
	int			ldap_version = 3;
	char		*ldap_filter = _ds_read_attribute(agent_config, "ExtLookupQuery");
	int			ldap_scope;

	if (port != NULL) {
		errno=0;
		lldap_port = strtol(port, &end_ptr, 0);
		if ( (errno != 0) || (lldap_port < INT_MIN) || (lldap_port > INT_MAX) || (*end_ptr != '\0')) {
			LOG(LOG_ERR, "External Lookup: bad LDAP port number");
			return NULL;
		} else
			ldap_port = (int)lldap_port;
	}

	/* set ldap protocol version */
	if (version != NULL) {
		errno=0;
		lldap_version = strtol(version, &end_ptr, 0);
		if ((errno != 0) || (lldap_version < 1) || (lldap_version > 3) || (*end_ptr != '\0')) {
			LOG(LOG_ERR, "External Lookup: bad LDAP protocol version");
			return NULL;
		} else
			ldap_version = (int)lldap_version;
	}
		
	if (_ds_match_attribute(agent_config, "ExtLookupLDAPScope", "one"))
		ldap_scope = LDAP_SCOPE_ONELEVEL;
	else /* defaults to sub */
		ldap_scope = LDAP_SCOPE_SUBTREE;

	/* set alarm handler */
	signal(SIGALRM, sig_alrm);

	/* sanitize username for filter integration */
	for (; *username != '\0'; username++) {
		switch(*username) {
			case 0x2a: /* '*' */
			case 0x28: /* '(' */
			case 0x29: /* ')' */
			case 0x5c: /* '\' */
			case 0x00: /* NUL */
				*p++ = 0x5c; /* '\' */
				*p++ = *username;
				break;

			default:
				*p++ = *username;
				break;
		}
	}

	*p = '\0';

	LOGDEBUG("External Lookup: sanitized username is %s\n", sane_username);

	/* build proper LDAP filter*/
	transcoded_query = strdup(transcode_query(ldap_filter, sane_username, transcoded_query));
	free(sane_username);
	if (transcoded_query == NULL) {
		LOG(LOG_ERR, "External Lookup: %s", ERR_EXT_LOOKUP_MISCONFIGURED); 
		return NULL;
	}

	if( ldap_host != NULL || ldap_port ) {
	/* construct URL */
		LDAPURLDesc url;
		memset( &url, 0, sizeof(url));

		url.lud_scheme = "ldap";
		url.lud_host = ldap_host;
		url.lud_port = ldap_port;
		url.lud_scope = LDAP_SCOPE_SUBTREE;

		ldap_uri = ldap_url_desc2str( &url );
	}

	rc = ldap_initialize( &ld, ldap_uri );
	if( rc != LDAP_SUCCESS ) {
		LOG(LOG_ERR, "External Lookup: Could not create LDAP session handle for URI=%s (%d): %s\n", ldap_uri, rc, ldap_err2string(rc));
		return NULL;
	}

	if( ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &ldap_version ) != LDAP_OPT_SUCCESS ) {
		LOG(LOG_ERR, "External Lookup: Could not set LDAP_OPT_PROTOCOL_VERSION %d\n", ldap_version );
		return NULL;
	}

	/* use TLS if configured */
	if ( _ds_match_attribute(agent_config, "ExtLookupCrypto", "tls" )) {
		if (ldap_version != 3) {
			LOG(LOG_ERR, "External Lookup: TLS only supported with LDAP protocol version 3");
			return NULL;
		}
		if ( ldap_start_tls_s( ld, NULL, NULL ) != LDAP_SUCCESS ) {
			LOG(LOG_ERR, "External Lookup: %s: %s (%d)", ERR_EXT_LOOKUP_INIT_FAIL, strerror(errno), errno);
			return NULL;
		}
	}

	/* schedules alarm */
	alarm(BIND_TIMEOUT);
	
	/* authenticate to the directory */
	if ( (rc = ldap_simple_bind_s( ld, ldap_binddn, ldap_passwd )) != LDAP_SUCCESS ) {
		/* cancel alarms */
		alarm(0);
		
		LOG(LOG_ERR, "External Lookup: %s: %s", ERR_EXT_LOOKUP_INIT_FAIL, ldap_err2string(rc) );
		ldap_unbind(ld);
		return NULL;
	}
	/* cancel alarms */
	alarm(0);
	
	/* search for all entries matching the filter */

	if ( (rc = ldap_search_st( ld, 
				   ldap_base, 
				   ldap_scope,
				   transcoded_query,
				   ldap_attrs, 
				   0,
				   &ldaptimeout, 
				   &result )) != LDAP_SUCCESS ) {

	free(transcoded_query);

	switch(rc) {
		case LDAP_TIMEOUT:
		case LDAP_BUSY:
		case LDAP_UNAVAILABLE:
		case LDAP_UNWILLING_TO_PERFORM:
		case LDAP_SERVER_DOWN:
		case LDAP_TIMELIMIT_EXCEEDED:
			LOG(LOG_ERR, "External Lookup: %s: %s", ERR_EXT_LOOKUP_SEARCH_FAIL, ldap_err2string(ldap_result2error(ld, result, 1)) );
			ldap_unbind( ld );
			return NULL;
			break;
		case LDAP_FILTER_ERROR:
			LOG(LOG_ERR, "External Lookup: %s: %s", ERR_EXT_LOOKUP_SEARCH_FAIL, ldap_err2string(ldap_result2error(ld, result, 1)) );
			ldap_unbind( ld );
			return NULL;
			break;
		case LDAP_SIZELIMIT_EXCEEDED:
			if ( result == NULL ) {
				LOG(LOG_ERR, "External Lookup: %s: %s", ERR_EXT_LOOKUP_SEARCH_FAIL, ldap_err2string(ldap_result2error(ld, result, 1)) );
				ldap_unbind( ld );
				return NULL;
			}
			break;
		default:		       
			LOG(LOG_ERR, "External Lookup: %s: code=%d, %s", ERR_EXT_LOOKUP_SEARCH_FAIL, rc, ldap_err2string(ldap_result2error(ld, result, 1)) );
			ldap_unbind( ld );
			return NULL;
		}
	}

	num_entries=ldap_count_entries(ld,result);

	LOGDEBUG("External Lookup: found %d LDAP entries", num_entries);

    switch (num_entries) {
				case 1: /* only one entry, let's proceed */
					break;
					
				case -1: /* an error occured */
				    LOG(LOG_ERR, "External Lookup: %s: %s", ERR_EXT_LOOKUP_SEARCH_FAIL, ldap_err2string(ldap_result2error(ld, result, 1)));
					ldap_unbind( ld );
					return NULL ;
					
				case 0: /* no entries found */
					LOGDEBUG("External Lookup: %s: no entries found.", ERR_EXT_LOOKUP_SEARCH_FAIL);
					ldap_msgfree( result );
					ldap_unbind( ld );
					return NULL ;

				default: /* more than one entry returned */
					LOG(LOG_ERR, "External Lookup: %s: more than one entry returned.", ERR_EXT_LOOKUP_SEARCH_FAIL);
					ldap_msgfree( result );
					ldap_unbind( ld );
			    	return NULL;
	}

	/* for each entry print out name + all attrs and values */
	for ( e = ldap_first_entry( ld, result ); e != NULL;
	    e = ldap_next_entry( ld, e ) ) {
		if ( (dn = ldap_get_dn( ld, e )) != NULL ) {
		    ldap_memfree( dn );
		}
		for ( a = ldap_first_attribute( ld, e, &ber );
		    a != NULL; a = ldap_next_attribute( ld, e, ber ) ) {
			if ((vals = ldap_get_values( ld, e, a)) != NULL ) {
				for ( i = 0; vals[i] != NULL; i++ ) {
					external_uid = strdup(vals[i]);
				}
				ldap_value_free( vals );
			}
			ldap_memfree( a );
		}
		if ( ber != NULL ) {
			ber_free( ber, 0 );
		}
	}
	ldap_msgfree( result );
	ldap_unbind( ld );
	return external_uid;
}
#endif

char*
program_lookup(config_t agent_config, const char *username, char *external_uid)
{
	pid_t wstatus, pid;
	int i, status;
	int fd[2];
	char *output = malloc (1024);
	char **args = malloc (1024);
	char *token;
	char *saveptr;
	char *str;
	char *command_line = 0;
	
    /* build proper command line*/
	command_line = strdup(transcode_query(_ds_read_attribute(agent_config, "ExtLookupServer"), username, command_line));

	if (command_line == NULL) {
		LOG(LOG_ERR, ERR_EXT_LOOKUP_MISCONFIGURED);
		free(output);
		free(args);
		return NULL;
	}

	LOGDEBUG("command line is %s", command_line);
	
	/* break the command line into arguments */
	for (i = 0, str = command_line; ; i++, str = NULL) {
		token = strtok_r(str, " ", &saveptr);
		if (token == NULL)
			break;
		args[i] = token;
		LOGDEBUG("args[%d] = %s",i,token);
	}
	args[i] = (char *) 0;

	if (pipe(fd) == -1) {
		LOG(LOG_ERR, "%s: errno=%i (%s)", ERR_EXT_LOOKUP_INIT_FAIL, errno, strerror(errno));
		free(output);
		free(args);
		return NULL;
	}

	switch(pid=fork()) {

		case -1: /* couldn't fork - something went wrong */
			LOG(LOG_ERR, "%s: errno=%i (%s)", ERR_EXT_LOOKUP_INIT_FAIL, errno, strerror(errno));
			free(output);
			free(args);
			return NULL;

		case 0: /* execute the command and write to fd */
			close(fd[0]);
			dup2(fd[1], fileno(stdout));
			execve(args[0], args, 0);
			exit(EXIT_FAILURE);
			
		default: /* read from fd the first output line */
			do {
				wstatus = waitpid( pid, &status, WUNTRACED | WCONTINUED);
				if (wstatus == -1) {
					LOGDEBUG("waitpid() exited with an error: %s: errno=%i", strerror(errno), errno);
					free(output);
					free(args);
					return NULL;
				}
				if (WIFEXITED(status)) {
					LOGDEBUG("exited, status=%d\n", WEXITSTATUS(status));
					if (WEXITSTATUS(status)) {
						LOGDEBUG("Error running %s. Check path and permissions.\n", args[0]);
					}
				} else if (WIFSIGNALED(status)) {
					LOGDEBUG("killed by signal %d\n", WTERMSIG(status));
				} else if (WIFSTOPPED(status)) {
					LOGDEBUG("stopped by signal %d\n", WSTOPSIG(status));
				} else if (WIFCONTINUED(status)) {
					LOGDEBUG("continued\n");
				}
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			close(fd[1]);
			/* just in case there's no line break at the end of the return... */
			memset(output, 0, 1024);
			if (read(fd[0], output, 1024) == -1) {
				LOG(LOG_ERR, "%s: errno=%i (%s)", ERR_EXT_LOOKUP_INIT_FAIL, errno, strerror(errno));
				free(output);
				free(args);
				return NULL;
			}
			close(fd[0]);
	}

	if (strlen(output) == 0) {
		free(output);
		free(args);
		return NULL;
	}
	
	/* terminate the output string at the first \n */
	token = strchr(output, '\n');
	if (token != NULL)
		*token = '\0';
	
	external_uid = strdup(output);
	free(output);
	free(command_line);
	free(args);
	return external_uid;
}
#endif
