# $Id#
# tools.hash_drv/Makefile.am
#

EXTRA_DIST = 

AM_CPPFLAGS = \
  -DLOGDIR=\"$(logdir)\" -DCONFIG_DEFAULT=\"$(sysconfdir)/dspam.conf\" \
  -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -D_GNU_SOURCE -I$(top_srcdir)

CFLAGS = @CFLAGS@ -I$(top_srcdir)/src

bin_PROGRAMS = cssgrow cssstat cssclean

DEFS = @DEFS@ -DLOGDIR=\"$(logdir)\" \
       -DCONFIG_DEFAULT=\"$(sysconfdir)/dspam.conf\"\
       -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -D_GNU_SOURCE

cssgrow_SOURCES = cssgrow.c ../util.c ../diction.c ../nodetree.c ../hash_drv.c ../error.c ../config_shared.c
cssgrow_LDFLAGS = -static
if DYNAMIC_DRIVER
if BUILD_HASH_DRV
cssgrow_CPPFLAGS = $(libhash_drv_cppflags)
cssgrow_LDFLAGS += $(libhash_drv_libs) $(libhash_drv_ldflags)
endif
endif

cssstat_SOURCES = cssstat.c ../util.c ../diction.c ../nodetree.c ../hash_drv.c ../error.c ../config_shared.c
cssstat_LDFLAGS = -static
if DYNAMIC_DRIVER
if BUILD_HASH_DRV
cssstat_CPPFLAGS = $(libhash_drv_cppflags)
cssstat_LDFLAGS += $(libhash_drv_libs) $(libhash_drv_ldflags)
endif
endif

cssclean_SOURCES = cssclean.c ../util.c ../diction.c ../nodetree.c ../hash_drv.c ../error.c ../config_shared.c
cssclean_LDFLAGS = -static
if DYNAMIC_DRIVER
if BUILD_HASH_DRV
cssclean_CPPFLAGS = $(libhash_drv_cppflags)
cssclean_LDFLAGS += $(libhash_drv_libs) $(libhash_drv_ldflags)
endif
endif
