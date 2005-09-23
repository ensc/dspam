# $Id: Makefile.am,v 1.5 2005/09/23 16:58:44 jonz Exp $
# tools/Makefile.am

EXTRA_DIST = dspam_corpus.in dspam_genaliases.in dspam_logrotate
CLEANFILES = dspam_corpus dspam_genaliases

AM_CPPFLAGS = \
  -DLOGDIR=\"$(logdir)\" -DCONFIG_DEFAULT=\"$(sysconfdir)/dspam.conf\" \
  -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -D_GNU_SOURCE -I$(top_srcdir)

CFLAGS = @CFLAGS@ -I$(top_srcdir)/src

bin_PROGRAMS = dspam_dump dspam_clean dspam_crc dspam_stats dspam_merge dspam_2sql dspam_admin
bin_SCRIPTS = dspam_corpus dspam_genaliases dspam_logrotate

dspam_2sql_SOURCES = dspam_2sql.c ../read_config.c ../config_api.c
dspam_2sql_LDFLAGS = -static
dspam_dump_SOURCES = dspam_dump.c ../read_config.c ../config_api.c
dspam_dump_LDFLAGS = -static
dspam_clean_SOURCES = dspam_clean.c ../read_config.c ../config_api.c
dspam_clean_LDFLAGS = -static
dspam_crc_SOURCES = dspam_crc.c
dspam_crc_LDFLAGS = -static
dspam_stats_SOURCES = dspam_stats.c ../read_config.c ../config_api.c
dspam_stats_LDFLAGS = -static
dspam_merge_SOURCES = dspam_merge.c ../read_config.c ../config_api.c
dspam_merge_LDFLAGS = -static
dspam_admin_SOURCES = dspam_admin.c ../read_config.c ../config_api.c
dspam_admin_LDFLAGS = -static

LDADD = ../libdspam.la

sed_substitute_variables = \
	dspam_transformed=`echo dspam | sed '$(transform)'`; \
	sed -e 's,@bindir\@,$(bindir),g' \
	    -e 's,@PACKAGE_STRING\@,$(PACKAGE_STRING),g' \
	    -e 's,@PACKAGE_BUGREPORT\@,$(PACKAGE_BUGREPORT),g' \
	    -e "s,@dspam_transformed\@,$$dspam_transformed,g"

dspam_corpus: dspam_corpus.in Makefile
	${sed_substitute_variables} < $(srcdir)/dspam_corpus.in > $@
	chmod +x $@

dspam_genaliases: dspam_genaliases.in Makefile
	${sed_substitute_variables} < $(srcdir)/dspam_genaliases.in > $@
	chmod +x $@
