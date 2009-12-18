# $Id: Makefile.am,v 1.26 2009/12/18 09:30:58 sbajic Exp $
#

bin_SCRIPTS = configure_pl
CLEANFILES = configure.pl

EXTRA_DIST =            \
	dspam.cgi       \
	graph.cgi       \
	default.prefs   \
	rgb.txt         \
	admin.cgi       \
	admingraph.cgi  \
	admins          \
	configure.pl.in

SUBDIRS = 		\
	templates	\
	templates/cs	\
	templates/de	\
	templates/es-es	\
	templates/fr	\
	templates/he	\
	templates/pt-br	\
	templates/ro

sed_substitute_variables = \
	dspam_transformed=`echo dspam | sed '$(transform)'`; \
	sed -e 's,@dspam_home\@,$(dspam_home),g' \
	-e 's,@bindir\@,$(bindir),g'

configure_pl: configure.pl.in Makefile
	${sed_substitute_variables} < $(srcdir)/configure.pl.in > configure.pl
