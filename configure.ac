dnl $Id: configure.ac,v 1.15 2004/12/26 20:27:05 jonz Exp $
dnl
dnl    configure.ac
dnl    Jonathan A. Zdziarski <jonathan@nuclearelephant.com>
dnl
dnl    Original framework by Andrew W. Nosenko <awn@bcs.zp.ua>
dnl
dnl    autoconf (GNU Autoconf) 2.59
dnl    ltmain.sh (GNU libtool) 1.5.6
dnl    automake (GNU automake) 1.9.2
dnl
AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (C) 2002-2004 Network Dweebs Corporation])
AC_INIT([dspam],[CVS],[jonathan@nuclearelephant.com])
CONFIGURE_ARGS=$@
AC_CONFIG_SRCDIR(./src)

AM_INIT_AUTOMAKE([1.7 foreign])

AC_CANONICAL_HOST

#---------------------------------------------------------
#	C language relative
AC_LANG(C)
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

AC_C_INLINE

AC_DEFINE_UNQUOTED(CONFIGURE_ARGS, ["$CONFIGURE_ARGS"],
    [Configure Arguments])

# Check for various flavors of varargs macros
AC_MSG_CHECKING([for ISO C99 varargs macros in C])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
        [[
            int a(int, int, int);
#           define call_a(...) a(1,__VA_ARGS__)
        ]],
        [[ call_a(2,3); ]])
    ],  dnl AC_LANG_PROGRAM() end
    [
        # action if-found
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_ISO_VARARGS], 1,
                  [Define if you have ISO C99 vararg macros])
    ],
    [
        # action if-not-found
        AC_MSG_RESULT([no])
    ])

#---------------------------------------------------------
#   Libc features
#
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(math.h)
AC_CHECK_HEADERS(sys/time.h)
AC_HEADER_TIME
AC_CHECK_FUNCS(strlcpy)
AC_CHECK_FUNCS(strsep)
AC_CHECK_FUNCS(strtok_r)
AC_CHECK_FUNCS(getpwnam_r)
AC_CHECK_FUNCS(getpwuid_r)
AC_CHECK_FUNCS([memcpy memset])
AC_CHECK_FUNCS(localtime_r)

LIBS="-lm"

#   Daemon support
#
AC_ARG_ENABLE(daemon,
    [AS_HELP_STRING(--enable-daemon,
                        Enable support for DSPAM to run in --daemon mode
                    )])
AC_MSG_CHECKING([whether to enable daemon mode])
case x"$enable_daemon" in
    xyes)   # daemon output enabled explicity
            ;;
    xno)    # daemon output disabled explicity
            ;;
    x)      # daemon output disabled by default
            enable_daemon=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_daemon for --{enable,disable}-daemon configure option])
            ;;
esac
if test x"$enable_daemon" != xyes
then
    enable_daemon=no
else
    enable_daemon=yes    # overkill, but convenient
    AC_DEFINE(DAEMON, 1, [Defined if daemon support is enabled])
fi
AC_MSG_RESULT([$enable_daemon])

#
#   Determine how to compile with pthreads support
#
if test x"$enable_daemon" = xyes
then
  DS_PTHREADS(PT_CPPFLAGS, PT_LDFLAGS, PT_LIBS, [success=yes], [success=np])
  if test x"$success" != xyes
  then
    AC_MSG_FAILURE([unable to determine how to compile with pthreads])
  fi
fi

CPPFLAGS="$CPPFLAGS $PT_CPPFLAGS"
LDFLAGS="$LDFLAGS $PT_LDFLAGS"
LIBS="$LIBS $PT_LIBS"

#
#   Network related libraries on Solaris is another game...
#   Needed for dspam_ngstat and mysql driver.
#
AC_CHECK_HEADERS([sys/types.h sys/socket.h netinet/in.h arpa/inet.h])

DS_NETLIBS(NETLIBS, [success=yes], [success=no])
if test x"$success" != xyes
then
    AC_MSG_FAILURE([required libraries are not found or not usable])
fi
AC_SUBST(NETLIBS)

#
#   Support for NodalCore(tm) C-Series Hardware Extensions
#
DS_NCORE()

#---------------------------------------------------------
#   DSPAM features
#

#
#   DSPAM_HOME -- directory where pre-user dictionaries, signatures,
#              etc. should be stored.
#
#   This variable is AC_SUBST'ed, not AC_DEFINE'd because used not only
#   for C-code, but for Makefile rules also.
#
### dspam_home='$(prefix)/var/dspam'
dspam_home='$(prefix)/var/dspam'
AC_ARG_WITH(dspam_home,
    [AS_HELP_STRING(--with-dspam-home=DIR,
                        [Specify directory where per-user dictionaries,
                        signatures, etc. should be stored.]
                        @<:@default: /var/dspam@:>@
                    )],
            [ dspam_home="$withval" ])
AC_SUBST(dspam_home)
AC_MSG_CHECKING([for dspam_home])
AC_MSG_RESULT([$dspam_home])

if test -d /var/dspam 
then
  AC_MSG_WARN([the default location for dspam_home has changed. please be sure to update your configuration or filesystem to reflect this.])
fi

# DSPAM_HOME ownership
AC_ARG_WITH(dspam_home_mode,
    [AS_HELP_STRING(--with-dspam-home-mode=MODE,
                        Set access mode for DSPAM_HOME[,] default 770
                    )],
    [ dspam_home_mode="$withval" ],
    [ dspam_home_mode='770' ]
    )
AC_ARG_WITH(dspam_home_owner,
    [AS_HELP_STRING(--with-dspam-home-owner=OWNER,
                        Set owner for DSPAM_HOME[,] default root
                    )],
    [ dspam_home_owner="$withval" ],
    [ dspam_home_owner='root' ]
    )
AC_ARG_WITH(dspam_home_group,
    [AS_HELP_STRING(--with-dspam-home-group=GROUP,
                        Set group for DSPAM_HOME[,] default mail
                    )],
    [ dspam_home_group="$withval" ],
    [ dspam_home_group='mail' ]
    )
AC_SUBST(dspam_home_mode)
AC_SUBST(dspam_home_owner)
AC_SUBST(dspam_home_group)

# dspam binary ownership
case x"$host" in
    *-freebsd*)     default_dspam_mode=4510;;
    *)              default_dspam_mode=2510;;
esac
AC_ARG_WITH(dspam_mode,
    [AS_HELP_STRING(--with-dspam-mode=MODE,
                        Set access mode for dspam binary[,] default 2510
                    )],
    [ dspam_mode="$withval" ],
    [ dspam_mode="$default_dspam_mode" ]
    )
AC_ARG_WITH(dspam_owner,
    [AS_HELP_STRING(--with-dspam-owner=OWNER,
                        Set owner for dspam binary[,] default root
                    )],
    [ dspam_owner="$withval" ],
    [ dspam_owner='root' ]
    )
AC_ARG_WITH(dspam_group,
    [AS_HELP_STRING(--with-dspam-group=GROUP,
                        Set group for dspam binary[,] default mail
                    )],
    [ dspam_group="$withval" ],
    [ dspam_group='mail' ]
    )
AC_SUBST(dspam_mode)
AC_SUBST(dspam_owner)
AC_SUBST(dspam_group)

#
#   Local delivery Agent
#
AC_MSG_CHECKING([which delivery agent to activate in dspam.conf])
default_delivery_agent="/usr/bin/procmail"
case "$host" in
    *-linux*)       default_delivery_agent="/usr/bin/procmail";;
    *-freebsd*)     default_delivery_agent="/usr/libexec/mail.local";;
    *-solaris2*)    default_delivery_agent="/usr/bin/mail";;
    *-cygwin*)      default_delivery_agent="/usr/bin/procmail";;
esac
AC_ARG_WITH(delivery_agent,
    [AS_HELP_STRING(--with-delivery-agent=BINARY @<:@ARGS@:>@,
                        Override the default delivery agent.  This sets only
                        the default[,] which may be changed in dspam.conf.
                    )],
    [ delivery_agent="$withval" ],
    [ delivery_agent="$default_delivery_agent" ]
    )
AC_MSG_RESULT([$delivery_agent])
AC_SUBST(delivery_agent)

#
#   Trusted User Security
#
AC_ARG_ENABLE(trusted_user_security,
    [AS_HELP_STRING(--disable-trusted-user-security,Disable trusted user security (enabled by default))])
AC_MSG_CHECKING([whether to enable trusted user security])
case x"$enable_trusted_user_security" in
    xyes)   # trusted user security enabled explicity
            ;;
    xno)    # trusted user security disabled explicity
            enable_trusted_user_security=no
            ;;
    x)      # trusted user security enabled by default
            enable_trusted_user_security=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_trusted_user_security for --{enable,disable}-trusted-user-security configure option])
            ;;
esac
if test x"$enable_trusted_user_security" != xyes
then
    enable_trusted_user_security=no
else
    enable_trusted_user_security=yes    # overkill, but convenient
    AC_DEFINE(TRUSTED_USER_SECURITY, 1,
              [Defined if trusted user security are enabled])
fi
AC_MSG_RESULT([$enable_trusted_user_security])

#
#   BNR Debug
#
AC_ARG_ENABLE(bnr-debug,
    [AS_HELP_STRING(--enable-bnr-debug,
                        Activates debugging output for Bayesian Noise Reduction
                    )])
AC_MSG_CHECKING([whether to enable bnr-debug])
case x"$enable_bnr_debug" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_bnr_debug=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_bnr_debug for --{enable,disable}-bnr-debug configure option])
            ;;
esac
if test x"$enable_bnr_debug" != xyes
then
    enable_bnr_debug=no
else
    enable_bnr_debug=yes    # overkill, but convenient
    AC_DEFINE(BNR_DEBUG, 1, [Defined if BNR Debug is enabled])
fi
AC_MSG_RESULT([$enable_bnr_debug])

#
#   Debug support
#
AC_ARG_ENABLE(debug,
    [AS_HELP_STRING(--enable-debug,
                        Enable debugging support for DSPAM.
                        Don't enable this unless something needs testing!
                    )])
AC_MSG_CHECKING([whether to enable debug output])
case x"$enable_debug" in
    xyes)   # debug output enabled explicity
            ;;
    xno)    # debug output disabled explicity
            ;;
    x)      # debug output disabled by default
            enable_debug=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_debug for --{enable,disable}-debug configure option])
            ;;
esac
if test x"$enable_debug" != xyes
then
    enable_debug=no
else
    enable_debug=yes    # overkill, but convenient
    AC_DEFINE(DEBUG, 1, [Defined if debug output is enabled])
fi
AC_MSG_RESULT([$enable_debug])

#
#   Homedir support
#
AC_ARG_ENABLE(homedir,
    [AS_HELP_STRING(--enable-homedir,
                        Enable home directory filesystem storage.
                    )])
AC_MSG_CHECKING([whether to enable home directory support])
case x"$enable_homedir" in
    xyes)   # homedir enabled explicity
            ;;
    xno)    # homedir disabled explicity
            ;;
    x)      # homedir disabled by default
            enable_homedir=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_homedir for --{enable,disable}-homedir configure option])
            ;;
esac
if test x"$enable_homedir" != xyes
then
    enable_homedir=no
else
    enable_homedir=yes    # overkill, but convenient
    AC_DEFINE(HOMEDIR, 1, [Defined if home directory support is enabled])
fi
AC_MSG_RESULT([$enable_homedir])

#
#   Neural Networking
#
AC_ARG_ENABLE(neural-networking,
    [AS_HELP_STRING(--enable-neural-networking,
                        Activates DSPAM's experimental neural-networking 
                        functions.
                    )])
AC_MSG_CHECKING([whether to enable neural-networking])
case x"$enable_neural_networking" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_neural_networking=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_neural_networking for --{enable,disable}-neural-networking configure option])
            ;;
esac
if test x"$enable_neural_networking" != xyes
then
    enable_neural_networking=no
else
    enable_neural_networking=yes    # overkill, but convenient
    AC_DEFINE(NEURAL, 1, [Defined if neural-networking is enabled])
fi
AC_MSG_RESULT([$enable_neural_networking])

#
#   MySQL4 Initialization
#
AC_ARG_ENABLE(mysql4-initialization,
    [AS_HELP_STRING(--disable-mysql4-initialization,
                        Performs Initialization and Cleanup of MySQL client
                        libraries v4.1+.
                    )])
AC_MSG_CHECKING([whether to enable MySQL client initialization])
case x"$enable_mysql4_initialization" in
    xyes)   # enabled explicity
            enable_mysql4_initialization=yes
            ;;
    xno)    # disabled explicity
            enable_mysql4_initialization=no
            ;;
    x)      # disabled by default
            enable_mysql4_initialization=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_mysql4_initialization for --{enable,disable}-mysql4-initialization configure option])
            ;;
esac
if test x"$enable_mysql4_initialization" != xyes
then
    enable_mysql4_initialization=no
else
    enable_mysql4_initialization=yes    # overkill, but convenient
    AC_DEFINE(MYSQL4_INITIALIZATION, 1, [Defined if mysql4-initialization is enabled])
fi
AC_MSG_RESULT([$enable_mysql4_initialization])

#
#   Preferences Extension
#   For storage drivers with preferences extension, the driver's extension is
#   used instead of the built-in file-based function
#
AC_ARG_ENABLE(preferences-extension,
    [AS_HELP_STRING(--enable-preferences-extension,
                        Enable if storage driver supports preferences extension
                    )])
AC_MSG_CHECKING([whether to enable preferences-extension])
case x"$enable_preferences_extension" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_preferences_extension=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_preferences_extension for --{enable,disable}-preferences-extension configure option])
            ;;
esac
if test x"$enable_preferences_extension" != xyes
then
    enable_preferences_extension=no
else
    enable_preferences_extension=yes    # overkill, but convenient
    AC_DEFINE(PREFERENCES_EXTENSION, 1, [Defined if storage driver supports preferences extension])
fi
AC_MSG_RESULT([$enable_preferences_extension])

#
#   Verbose debug output
#
AC_ARG_ENABLE(verbose-debug,
    [AS_HELP_STRING(--enable-verbose-debug,
                        Cause DSPAM produce verbose debug output and
                        write them into LOGDIR/dspam.debug file.
                        Implies '--enable-debug'.
                        Never enable this for production builds!
                    )])
AC_MSG_CHECKING([whether to enable verbose debug output])
case x"$enable_verbose_debug" in
    xyes)   # debug output enabled explicity
            ;;
    xno)    # debug output disabled explicity
            ;;
    x)      # debug output disabled by default
            enable_verbose_debug=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_verbose_debug for --{enable,disable}-verbose-debug configure option])
            ;;
esac
if test x"$enable_verbose_debug" != xyes
then
    enable_verbose_debug=no
else
    enable_verbose_debug=yes    # overkill, but convenient
    AC_DEFINE(DEBUG, 1, [Defined if debug output is enabled])
    AC_DEFINE(VERBOSE, 1, [Defined if verbose debug output is enabled])
fi
AC_MSG_RESULT([$enable_verbose_debug])

#
#   Long Usernames
#
AC_ARG_ENABLE(long-usernames,
    [AS_HELP_STRING(--enable-long-usernames,
                        Cause DSPAM to support long usernames.  The default
                        is to use the same length on usernames as the operating
                        system.
                    )])
AC_MSG_CHECKING([whether to enable long usernames])
case x"$enable_long_usernames" in
    xyes)   # debug output enabled explicity
            ;;
    xno)    # debug output disabled explicity
            ;;
    x)      # debug output disabled by default
            enable_long_usernames=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_long_usernames for --{enable,disable}-long-usernames configure option])
            ;;
esac
if test x"$enable_long_usernames" != xyes
then
    enable_long_usernames=no
else
    enable_long_usernames=yes    # overkill, but convenient
    AC_DEFINE(LONG_USERNAMES, 1, [Defined if long usernames is enabled])
fi
AC_MSG_RESULT([$enable_long_usernames])

#
#   Graham-Bayesian Calculations
#   Only use this if you're a developer; end-users: set this in dspam.conf
#
AC_ARG_ENABLE(graham-bayesian,
    [AS_HELP_STRING(--disable-graham-bayesian,Disable Graham Bayesian calculations)])
AC_MSG_CHECKING([whether to enable Graham-Bayesian calculations])
case x"$enable_graham_bayesian" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            enable_graham_bayesian=no
            ;;
    x)      # enabled by default
            enable_graham_bayesian=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_graham_bayesian for --{enable,disable}-graham-bayesian configure option])
            ;;
esac
if test x"$enable_graham_bayesian" != xyes
then
    enable_graham_bayesian=no
else
    enable_graham_bayesian=yes    # overkill, but convenient
    AC_DEFINE(GRAHAM_BAYESIAN, 1,
             [Defined if graham bayesian calculation is enabled])
                                                                                
                                                                                
fi
AC_MSG_RESULT([$enable_graham_bayesian])

#
#   Burton-Bayesian Calculations
#   Only use this if you're a developer; end-users: set this in dspam.conf
#
AC_ARG_ENABLE(burton-bayesian,
    [AS_HELP_STRING(--disable-burton-bayesian,Disable burton Bayesian calculations)])
AC_MSG_CHECKING([whether to enable Burton Bayesian calculations])
case x"$enable_burton_bayesian" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            enable_burton_bayesian=no
            ;;
    x)      # disabled by default
            enable_burton_bayesian=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_burton_bayesian for --{enable,disable}-burton-bayesian configure option])
            ;;
esac
if test x"$enable_burton_bayesian" != xyes
then
    enable_burton_bayesian=no
else
    enable_burton_bayesian=yes    # overkill, but convenient
    AC_DEFINE(BURTON_BAYESIAN, 1,
             [Defined if burton bayesian calculation is enabled])
fi
AC_MSG_RESULT([$enable_burton_bayesian])

#
#   Robinson's Geometric Mean
#   Only use this if you're a developer; end-users: set this in dspam.conf
#
AC_ARG_ENABLE(robinson,
    [AS_HELP_STRING(--enable-robinson,Cause DSPAM to use the Robinson Geometric Mean calculation)])
AC_MSG_CHECKING([whether to enable Robinson's Geometric Mean calculations])
case x"$enable_robinson" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_robinson=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_robinson for --{enable,disable}-robinson configure option])
            ;;
esac
if test x"$enable_robinson" != xyes
then
    enable_robinson=no
else
    enable_robinson=yes    # overkill, but convenient
    AC_DEFINE(ROBINSON, 1, [Defined if Chi-Square calculation is enabled])
fi
AC_MSG_RESULT([$enable_robinson])

#
#   Fisher-Robinson's Inverse Chi-Square
#   Only use this if you're a developer; end-users: set this in dspam.conf
#
AC_ARG_ENABLE(chi-square,
    [AS_HELP_STRING(--enable-chi-square,Enable Fisher-Robinson Inverse Chi-Square calculations)])
AC_MSG_CHECKING([whether to enable Chi-Square calculations])
case x"$enable_chi_square" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_chi_square=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_chi_square for --{enable,disable}-chi-square configure option])
            ;;
esac
if test x"$enable_chi_square" != xyes
then
    enable_chi_square=no
else
    enable_chi_square=yes    # overkill, but convenient
    AC_DEFINE(CHI_SQ, 1,
             [Defined if Chi-Square calculation is enabled])
fi
AC_MSG_RESULT([$enable_chi_square])

if test x"$enable_graham_bayesian$enable_burton_bayesian$enable_robinson$enable_chi_square" = xnononono
then
  AC_MSG_ERROR([you must have at least one algorithm enabled])
fi

if test x"$enable_graham_bayesian$enable_chi_square" = xyesyes
then
  AC_MSG_ERROR([Bayesian and Chi-Square together can cause many false positives. If you plan on using Chi-Square, you should disable all Bayesian calculations])
fi

if test x"$enable_burton_bayesian$enable_chi_square" = xyesyes
then
  AC_MSG_ERROR([Bayesian and Chi-Square together can cause many false positives. If you plan on using Chi-Square, you should disable all Bayesian calculations])
fi

#
#   Robinson's Technique for Combining P-Values
#   Only use this if you're a developer; end-users: set this in dspam.conf
#
AC_ARG_ENABLE(robinson-pvalues,
    [AS_HELP_STRING(--enable-robinson-pvalues,Enable Robinson's technique for combining p-values)])
AC_MSG_CHECKING([whether to enable Robinson's technique])
case x"$enable_robinson_pvalues" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_robinson_pvalues=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_robinson_pvalues for --{enable,disable}-robinson-pvalues configure option])
            ;;
esac
if test x"$enable_robinson_pvalues" != xyes
then
    enable_robinson_pvalues=no
else
    enable_robinson_pvalues=yes    # overkill, but convenient
    AC_DEFINE(ROBINSON_FW, 1,
             [Defined if Robinson's technique for combining p-values is enabled])
fi
AC_MSG_RESULT([$enable_robinson_pvalues])
                                                                                
#
#   Test-Conditional Training
#   (users with > 2500 innocent messages will retrain spams + false positives
#    until test condition is met - 5 iterations max)
#
AC_ARG_ENABLE(test-conditional,
    [AS_HELP_STRING(--disable-test-conditional,Disables test-conditional (loop) training)])
AC_MSG_CHECKING([whether to enable test-conditional training])
case x"$enable_test_conditional" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            enable_test_conditional=no
            ;;
    x)      # disabled by default
            enable_test_conditional=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_test_conditional for --{enable,disable}-test-conditional configure option])
            ;;
esac
if test x"$enable_test_conditional" != xyes
then
    enable_test_conditional=no
else
    enable_test_conditional=yes    # overkill, but convenient
    AC_DEFINE(TEST_COND_TRAINING, 1, [Defined if test-conditional training is enabled])
fi
AC_MSG_RESULT([$enable_test_conditional])

#
#   Innocent Bias
#
AC_ARG_ENABLE(bias,
    [AS_HELP_STRING(--disable-bias,
                        Cause DSPAM to calculate without a bias toward
                        innocent mail
                    )])
AC_MSG_CHECKING([whether to enable bias calculations])
case x"$enable_bias" in
    xyes)   # debug output enabled explicity
            ;;
    xno)    # debug output disabled explicity
            enable_bias=no
            ;;
    x)      # debug output disabled by default
            enable_bias=yes
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_bias for --{enable,disable}-bias configure option])
            ;;
esac
if test x"$enable_bias" != xyes
then
    enable_bias=no
else
    enable_bias=yes    # overkill, but convenient
    AC_DEFINE(BIAS, 1, [Defined if bias calculations are enabled])
fi
AC_MSG_RESULT([$enable_bias])

#
#   Large scale implementation
#
AC_ARG_ENABLE(large-scale,
    [AS_HELP_STRING(--enable-large-scale,Manage file structure for a large scale implementation)])
AC_MSG_CHECKING([whether to enable large scale implementation])
case x"$enable_large_scale" in
    xyes)   # large-scale enabled explicitly
            ;;
    xno)    # large-scale disabled explicitly
            ;;
    x)      # large-scale disabled by default
            enable_large_scale=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_large_scale for --{enable,disable}-large-scale configure option])
            ;;
esac
if test x"$enable_large_scale" != xyes
then
    enable_large_scale=no
else
    enable_large_scale=yes    # overkill, but convenient
    AC_DEFINE(LARGESCALE, 1, [Defined if large_scale is enabled])
fi
AC_MSG_RESULT([$enable_large_scale])

#
#   Domain Scale Filesystem
#
AC_ARG_ENABLE(domain-scale,
    [AS_HELP_STRING(--enable-domain-scale,Manage file structure to support a domain implementation)])
AC_MSG_CHECKING([whether to enable domain structure implementation])
case x"$enable_domain_scale" in
    xyes)   # domain-scale enabled explicitly
            ;;
    xno)    # domain-scale disabled explicitly
            ;;
    x)      # domain-scale disabled by default
            enable_domain_scale=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_domain_scale for --{enable,disable}-domain-scale configure option])
            ;;
esac
if test x"$enable_domain_scale" != xyes
then
    enable_domain_scale=no
else
    enable_domain_scale=yes    # overkill, but convenient
    AC_DEFINE(DOMAINSCALE, 1, [Defined if domain_scale is enabled])
fi
AC_MSG_RESULT([$enable_domain_scale])

if test x"$enable_domain_scale$enable_large_scale" = xyesyes
then
  AC_MSG_ERROR([you may not have domain-scale and large-scale implemented simultaneously])
fi

#
#   Log Directory
#

logdir="$dspam_home/log"
AC_ARG_WITH(logdir,
    [AS_HELP_STRING(--with-logdir=DIR,
                        Specify directory where logfiles are to be written.
                        @<:@default: $dspam_home/log@:>@
                    )],
            [ logdir="$withval" ])
AC_SUBST(logdir)
AC_MSG_CHECKING([for logdir])
AC_MSG_RESULT([$logdir])

#---------------------------------------------------------
#   storage driver - storage driver selection
#              copies the selected driver to storage_drv.c and storage_drv.h
#
#   This variable is AC_SUBST'ed, not AC_DEFINE'd because used not only
#   for C-code, but for Makefile rules also.
#
storagedrv=sqlite_drv
AC_ARG_WITH(storage-driver,
    [AS_HELP_STRING(--with-storage-driver=DIR,Specify storage driver. @<:@default: sqlite_drv@:>@)],
    [ storagedrv="$withval" ])

AC_MSG_CHECKING([for storage driver])
AC_MSG_RESULT([$storagedrv])

AC_SUBST(storage_drv_objects)   # driver-specific *libtool* object files
                                # should be listed here
AC_SUBST(storage_drv_subdirs)   # driver-specific subdirectories should
                                # be listed here
is_storagedrv_handled=no

#---------------------------------------------------------
#   Libraries for storage driver
#
if test x"$storagedrv" = xlibdb4_drv
then
    db_version_major=unknown
    DS_BERKELEY_DB4(DB_CPPFLAGS, DB_LDFLAGS, DB_LIBS,
                    db_version_major, [], [],
                    [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Berkeley DB not found or not usable])
    fi
    if test x"$db_version_major" != x4
    then
        AC_MSG_ERROR([Berkeley DB v$db_version_major found but v4 is required])
    fi

    CPPFLAGS="$DB_CPPFLAGS $CPPFLAGS"
    LIBS="$DB_LIBS $LIBS"
    LDFLAGS="$DB_LDFLAGS $LDFLAGS"

    storage_drv_subdirs='tools.libdb4_drv'
    storage_drv_objects='libdb4_drv.lo'     # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi

if test x"$storagedrv" = xlibdb3_drv
then
    db_version_major=unknown
    DS_BERKELEY_DB3(DB_CPPFLAGS, DB_LDFLAGS, DB_LIBS,
                    db_version_major, [], [],
                    [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Berkeley DB not found or not usable])
    fi
    if test x"$db_version_major" != x3
    then
        AC_MSG_ERROR([Berkeley DB v$db_version_major found but v3 is required])
    fi

    CPPFLAGS="$DB_CPPFLAGS $CPPFLAGS"
    LIBS="$DB_LIBS $LIBS"
    LDFLAGS="$DB_LDFLAGS"
    storage_drv_sources='libdb3_drv.c libdb3_drv.h'
    storage_drv_subdirs='tools.libdb3_drv'
    storage_drv_objects='libdb3_drv.lo'     # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi

if test x"$storagedrv" = xmysql_drv
then
    DS_MYSQL(MYSQL_CPPFLAGS, MYSQL_LDFLAGS, MYSQL_LIBS,
             [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Required version of libmysqlclient not found])
    fi
    CPPFLAGS="$MYSQL_CPPFLAGS $CPPFLAGS"
    LIBS="$MYSQL_LIBS $LIBS"
    LDFLAGS="$MYSQL_LDFLAGS $LDFLAGS"

    storage_drv_subdirs='tools.mysql_drv'
    storage_drv_objects='mysql_drv.lo'      # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi

if test x"$storagedrv" = xpgsql_drv
then
    DS_PGSQL(PGSQL_CPPFLAGS, PGSQL_LDFLAGS, PGSQL_LIBS,
             [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Required version of libpq not found])
    fi
    CPPFLAGS="$PGSQL_CPPFLAGS $CPPFLAGS"
    LIBS="$PGSQL_LIBS $LIBS"
    LDFLAGS="$PGSQL_LDFLAGS $LDFLAGS"
                                                                                
    storage_drv_subdirs='tools.pgsql_drv'
    storage_drv_objects='pgsql_drv.lo'      # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi
                                                                                
if test x"$storagedrv" = xora_drv
then
    DS_ORACLE(ORACLE_CPPFLAGS, ORACLE_LDFLAGS, ORACLE_LIBS,
              [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Required version of libclntsh not found])
    fi
    CPPFLAGS="$ORACLE_CPPFLAGS $CPPFLAGS"
    LIBS="$ORACLE_LIBS $LIBS"
    LDFLAGS="$ORACLE_LDFLAGS $LDFLAGS"

    storage_drv_subdirs='tools.ora_drv'
    storage_drv_objects='ora_drv.lo'       # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi

if test x"$storagedrv" = xsqlite_drv
then
    DS_SQLITE(SQLITE_CPPFLAGS, SQLITE_LDFLAGS, SQLITE_LIBS,
              [sqlite_version_major], [], [],
              [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Required version of libsqlite not found])
    fi
    if test x"$sqlite_version_major" != x2
    then
        AC_MSG_ERROR([Version mismatch: sqlite_drv requires libsqlite version 2])
    fi

    CPPFLAGS="$SQLITE_CPPFLAGS $CPPFLAGS"
    LIBS="$SQLITE_LIBS $LIBS"
    LDFLAGS="$SQLITE_LDFLAGS $LDFLAGS"
                                                                                
    storage_drv_subdirs='tools.sqlite_drv'
    storage_drv_objects='sqlite_drv.lo'     # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi

if test x"$storagedrv" = xsqlite3_drv
then
    DS_SQLITE3(SQLITE3_CPPFLAGS, SQLITE3_LDFLAGS, SQLITE3_LIBS,
              [sqlite_version_major], [], [],
              [success=yes], [success=no])
    if test x"$success" != xyes
    then
        AC_MSG_ERROR([Required version of libsqlite not found])
    fi
    if test x"$sqlite_version_major" != x3
    then
        AC_MSG_ERROR([Version mismatch: sqlite3_drv requires libsqlite version 3])
    fi

    CPPFLAGS="$SQLITE3_CPPFLAGS $CPPFLAGS"
    LIBS="$SQLITE3_LIBS $LIBS"
    LDFLAGS="$SQLITE3_LDFLAGS $LDFLAGS"

    storage_drv_subdirs='tools.sqlite_drv'
    storage_drv_objects='sqlite3_drv.lo'    # Please note: because storage
                                            # drives is part of libdspam,
                                            # which builded/linked by libtool,
                                            # then object file have `.lo'
                                            # suffix.  In case for plain
                                            # objects (for executables or
                                            # non-libtool libraries)
                                            # `$(OBJEXT)' should be used
                                            # instead.
    is_storagedrv_handled=yes
fi


if test x"$is_storagedrv_handled" != xyes
then
    AC_MSG_ERROR([unknown storage driver requested: $storagedrv])
fi

if test x"$enable_daemon" = xyes && test x"$storagedrv" != xmysql_drv && test x"$storagedrv" != xpgsql_drv
then
  AC_MSG_ERROR([daemon mode requires one of the following storage drivers: mysql_drv pgsql_drv])
fi


#---------------------------------------------------------
#   Libraries (other)
#

#---------------------------------------------------------
#   Documentation generation tools
#
### AC_CHECK_PROG([HAVE_DOXYGEN], [doxygen], [yes], [no])
### AC_CHECK_PROG([HAVE_DOT], [dot], [yes], [no])
### AM_CONDITIONAL([HAVE_DOXYGEN], [ test x"$HAVE_DOXYGEN" = xyes ])

#----------------------------------------------------------
# final cut
#
GCC_BUILD_OPTIONS

AC_CONFIG_HEADERS(src/auto-config.h)
AH_TOP([
#ifndef __auto_config_h
#define __auto_config_h
])
AH_BOTTOM([
#endif /* !__auto_config_h */
])

AC_CONFIG_FILES([Makefile src/tools/Makefile cgi/Makefile m4/Makefile])
AC_CONFIG_FILES([src/tools.libdb4_drv/Makefile src/tools.libdb3_drv/Makefile])
AC_CONFIG_FILES([src/tools.mysql_drv/Makefile src/tools.ora_drv/Makefile])
AC_CONFIG_FILES([txt/Makefile src/tools.pgsql_drv/Makefile ])
AC_CONFIG_FILES([src/Makefile cgi/templates/Makefile])
AC_CONFIG_FILES([src/tools.sqlite_drv/Makefile man/Makefile])
AC_CONFIG_FILES([src/dspam.pc src/dspam-uninstalled.pc])
AC_CONFIG_FILES([src/win32/Makefile])
AC_OUTPUT

