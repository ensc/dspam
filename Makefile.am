# $Id: Makefile.am,v 1.2 2005/01/03 22:37:43 jonz Exp $
#
# Makefile.am
# Jonathan A. Zdziarski <jonathan@nuclearelephant.com>
# Andrew W. Nosenko <awn@bcs.zp.ua>
#
# autoconf (GNU Autoconf) 2.59
# ltmain.sh (GNU libtool) 1.5.6
# automake (GNU automake) 1.9.2

includedir=@includedir@/dspam

SUBDIRS = . cgi m4 man src
DIST_SUBDIRS = . cgi m4 txt man src

EXTRA_DIST = autogen.sh RELEASE.NOTES CHANGELOG LICENSE \
  README README.sendmail README.exim README.courier README.postfix \
  README.pop3filter README.qmail UPGRADING

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 auto-config.h.in \
	config.guess config.sub configure depcomp install-sh   \
	ltmain.sh missing mkinstalldirs

DEFS = @DEFS@ 

ACLOCAL_AMFLAGS = -I m4

