# $Id: Makefile.am,v 1.5 2005/03/11 21:16:02 jonz Exp $
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
DIST_SUBDIRS = . cgi m4 txt man src doc

EXTRA_DIST = autogen-glibtool.sh autogen.sh RELEASE.NOTES CHANGELOG LICENSE \
  README UPGRADING

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 auto-config.h.in \
	config.guess config.sub configure depcomp install-sh   \
	ltmain.sh missing mkinstalldirs

DEFS = @DEFS@ 

ACLOCAL_AMFLAGS = -I m4

