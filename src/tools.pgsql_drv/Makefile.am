# $Id: Makefile.am,v 1.3 2005/03/11 21:16:03 jonz Exp $
# tools.pgsql_drv/Makefile.am
#

EXTRA_DIST = pgsql_objects.sql virtual_users.sql purge.sql 

AM_CPPFLAGS = \
  -DLOGDIR=\"$(logdir)\" -DCONFIG_DEFAULT=\"$(sysconfdir)/dspam.conf\" \
  -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -I$(top_srcdir)

CFLAGS = @CFLAGS@ -I$(top_srcdir)/src

bin_PROGRAMS = dspam_pg2int8

dspam_pg2int8_SOURCES = dspam_pg2int8.c ../read_config.c ../config_api.c
dspam_pg2int8_LDFLAGS = -static

LDADD = ../libdspam.la

