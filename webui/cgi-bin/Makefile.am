# $Id: Makefile.am,v 1.29 2010/01/06 00:25:05 sbajic Exp $
#

bin_SCRIPTS = configure_pl
CLEANFILES = configure.pl

EXTRA_DIST =		\
	dspam.cgi	\
	graph.cgi	\
	default.prefs	\
	rgb.txt		\
	admin.cgi	\
	admingraph.cgi	\
	admins		\
	subadmins	\
	configure.pl.in	\
	htmlize.pl

SUBDIRS = 		\
	templates	\
	templates/cs	\
	templates/de	\
	templates/es-es	\
	templates/fr	\
	templates/he	\
	templates/pt-br	\
	templates/ro	\
	templates/ru

sed_substitute_variables = \
	dspam_transformed=`echo dspam | sed '$(transform)'`; \
	sed -e 's,@dspam_home\@,$(dspam_home),g' \
	-e 's,@bindir\@,$(bindir),g'

configure_pl: configure.pl.in Makefile
	${sed_substitute_variables} < $(srcdir)/configure.pl.in > configure.pl
