# $Id: Makefile.am,v 1.2 2004/10/24 20:54:44 jonz Exp $
#
# Makefile.am
# Jonathan A. Zdziarski <jonathan@nuclearelephant.com>
# Andrew W. Nosenko <awn@bcs.zp.ua>
#
# autoconf (GNU Autoconf) 2.59
# ltmain.sh (GNU libtool) 1.5.6
# automake (GNU automake) 1.9.2

includedir=@includedir@/dspam

SUBDIRS = . tools $(storage_drv_subdirs) win32
DIST_SUBDIRS = . \
	tools.libdb3_drv tools.libdb4_drv tools.mysql_drv tools.ora_drv \
	tools.pgsql_drv tools.sqlite_drv tools win32

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dspam.pc
etcdir = $(sysconfdir)
etc_SCRIPTS = dspam_conf

# Defaults from dspam.conf

EXTRA_DIST = example.c dspam-button.gif dspam.pc.in dspam-uninstalled.pc.in \
        dspam.conf.in 

CLEANFILES = dspam.conf

MAINTAINERCLEANFILES = Makefile.in aclocal.m4 auto-config.h.in \
	config.guess config.sub configure depcomp install-sh   \
	ltmain.sh missing mkinstalldirs

DEFS = @DEFS@ -DLOGDIR=\"$(logdir)\" \
       -DCONFIG_DEFAULT=\"$(sysconfdir)/dspam.conf\"\
       -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS 

lib_LTLIBRARIES = libdspam.la
bin_PROGRAMS = dspam

# installed for libdspam
include_HEADERS = buffer.h decode.h error.h lht.h libdspam.h \
	libdspam_objects.h nodetree.h storage_driver.h

# libdspam.a contans objects common for dspam and tools/* binaries
libdspam_la_SOURCES = \
	config.h libdspam_objects.h \
	libdspam.c libdspam.h \
	tbt.c tbt.h \
	lht.c lht.h \
        base64.c base64.h \
	buffer.c buffer.h \
	util.c util.h \
	nodetree.c nodetree.h \
        error.c error.h \
	decode.c decode.h \
	storage_driver.h \
	pref.c pref.h \
	bnr.c bnr.h \
	config_shared.c config_shared.h
 
libdspam_la_LIBADD = $(storage_drv_objects)
libdspam_la_LIBADD += -lm $(NETLIBS)
libdspam_la_DEPENDENCIES = $(storage_drv_objects)

EXTRA_libdspam_la_SOURCES = \
	libdb4_drv.c libdb4_drv.h \
	libdb3_drv.c libdb3_drv.h \
	mysql_drv.c mysql_drv.h \
	pgsql_drv.c pgsql_drv.h \
        ora_drv.c ora_drv.h \
	sqlite_drv.c sqlite_drv.h

# Libtools veriooning scheme is:
#   current:revision:age
libdspam_la_LDFLAGS = -version-info 6:0:0

dspam_SOURCES = \
        config.h libdspam.h language.h \
        dspam.c dspam.h \
        buffer.h \
	storage_driver.h \
	md5.c md5.h \
	read_config.c read_config.h 

dspam_LDADD = libdspam.la
dspam_LDFLAGS = -static

# Failing of chgrp is expected and normal case when someone do
#   make install DESTDIR=...
# from user other than root (from user not included in the `mail' group
# is exactly), and only after this and after checking what was installed
# changes ownership.
#
# Caveat: creating the $(dspam_home) in the install-exec-hook is not the
# right way, but for some (unknown to me) reason `installdirs-local'
# target have no chances to be executed under automake-1.7.3 (at least).
#
# Solaris' ln seems to have a problem with -sf and existing symlinks.
install-exec-hook:
	-if test ! -d $(DESTDIR)$(sysconfdir); then \
		mkdir -p $(DESTDIR)$(sysconfdir); \
		chmod 755 $(DESTDIR)$(sysconfdir); \
	fi
	-if test ! -f $(DESTDIR)$(sysconfdir)/dspam.conf; then \
		cp dspam.conf $(DESTDIR)$(sysconfdir)/dspam.conf; \
		chmod 750 $(DESTDIR)$(sysconfdir)/dspam.conf; \
		chown "$(dspam_owner)" $(DESTDIR)$(sysconfdir)/dspam.conf; \
		chgrp "$(dspam_group)" $(DESTDIR)$(sysconfdir)/dspam.conf; \
	fi
	-if test x"$(dspam_owner)" != xnone; then \
                chown "$(dspam_owner)" $(DESTDIR)$(bindir)/dspam; \
        fi
	-if test x"$(dspam_group)" != xnone; then \
                chgrp "$(dspam_group)" $(DESTDIR)$(bindir)/dspam; \
        fi
	-if test x"$(dspam_mode)" != xnone; then \
                chmod "$(dspam_mode)" $(DESTDIR)$(bindir)/dspam; \
	fi
	-if test ! -d $(DESTDIR)$(dspam_home); then \
                $(mkinstalldirs) $(DESTDIR)$(dspam_home); \
		if test x"$(dspam_home_owner)" != xnone; then \
	        	chown "$(dspam_home_owner)" $(DESTDIR)$(dspam_home); \
	        fi; \
		if test x"$(dspam_home_group)" != xnone; then \
	                chgrp "$(dspam_home_group)" $(DESTDIR)$(dspam_home); \
	        fi; \
		if test x"$(dspam_home_mode)" != xnone; then \
	                chmod "$(dspam_home_mode)" $(DESTDIR)$(dspam_home); \
	        fi; \
	fi
	-if test ! -d $(DESTDIR)$(logdir); then \
		$(mkinstalldirs) $(DESTDIR)$(logdir); \
		if test x"$(dspam_home_owner)" != xnone; then \
			chown "$(dspam_home_owner)" $(DESTDIR)$(logdir); \
		fi; \
		if test x"$(dspam_home_group)" != xnone; then \
			chgrp "$(dspam_home_group)" $(DESTDIR)$(logdir); \
		fi; \
		if test x"$(dspam_home_mode)" != xnone; then \
			chmod "$(dspam_home_mode)" $(DESTDIR)$(logdir); \
		fi; \
	fi

ACLOCAL_AMFLAGS = -I m4

sed_substitute_variables = \
        dspam_transformed=`echo dspam | sed '$(transform)'`; \
        sed -e 's,@dspam_home\@,$(dspam_home),g' \
            -e 's,@delivery_agent\@,$(delivery_agent),g' 

dspam_conf: dspam.conf.in Makefile
	${sed_substitute_variables} < $(srcdir)/dspam.conf.in > dspam.conf

