# $Id: ora_drv.m4,v 1.1 2004/10/24 20:48:37 jonz Exp $
# Autuconf macroses for checking for Oracle
# Jonathan Zdziarski <jonathan@nuclearelephant.com>
# Andrew W. Nosenko <awn@bcs.zp.ua>
#
#   Public available macro:
#       DS_ORACLE([ora_cppflags_out],
#                 [ora_ldflags_out],
#                 [ora_libs_out],
#                 [additional-action-if-found],
#                 [additional-action-if-not-found]
#                 )
#
#   Another macros are considered as private for implementation and
#   sould not be used in the configure.ac.  At this time these are:
#       DS_ORACLE_HEADERS()
#       DS_ORACLE_LIBS()
#

#   DS_ORACLE_HEADERS([ora_cppflags_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_ORACLE_HEADERS],
[
ora_headers_save_CPPFLAGS="$CPPFLAGS"
ora_headers_CPPFLAGS=''
ora_headers_success=yes

#
#   virtual users
#
AC_ARG_ENABLE(virtual-users,
    [AS_HELP_STRING([--enable-virtual-users],
                    [Cause ora_drv to generate virtual uids for each user])])
AC_MSG_CHECKING([whether to enable virtual users])
case x"$enable_virtual_users" in
    xyes)   # enabled explicity
            ;;
    xno)    # disabled explicity
            ;;
    x)      # disabled by default
            enable_virtual_users=no
            ;;
    *)      AC_MSG_ERROR([unexpected value $enable_virtual_users for --{enable,disable}-virtual-users configure option])
            ;;
esac
if test x"$enable_virtual_users" != xyes
then
    enable_virtual_users=no
else
    enable_virtual_users=yes    # overkill, but convenient
    AC_DEFINE(VIRTUAL_USERS, 1, [Defined if homedir dotfiles is enabled])
fi
AC_MSG_RESULT([$enable_virtual_users])

#
#    oracle home
#
AC_ARG_WITH(oracle-home,
    [AS_HELP_STRING([--with-oracle-home=DIR],
                    [Where to find Oracle Home])])
AC_MSG_CHECKING([where to find Oracle Home])
if test x"$with_oracle_home" = x
then
    AC_MSG_RESULT([compiler default paths])
else
    AC_MSG_RESULT([$with_oracle_home])
    if test -d "$with_oracle_home"
    then
        :
    else
        AC_MSG_ERROR([required oracle home path for $with_oracle_home is not a directory])
    fi
    ora_headers_CPPFLAGS="-I$with_oracle_home/rdbms/demo -I$with_oracle_home/rdbms/public"
    CPPFLAGS="$ora_headers_CPPFLAGS $CPPFLAGS"
    ora_headers_CFLAGS="-I$with_oracle_home/rdbms/demo -I$with_oracle-home/rdbms/public"
    CFLAGS="$ora_headers_CFLAGS $CFLAGS"
    with_ora_include="$with_oracle_home/rdbms/demo"
fi

if test x"$ora_headers_success" = xyes
then
    AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
    #include <oci.h>
    #ifdef OCI_ORACLE
    /* Success */
    #else
    #error Unsupported version of Oracle 
    #endif
            ]])],
            [],
            [
                AC_MSG_FAILURE([Unsupported version of Oracle (no OCI_ORACLE defined)])
                ora_headers_success=no
            ])
fi

CPPFLAGS="$ora_headers_save_CPPFLAGS"
if test x"$ora_headers_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ora_headers_CPPFLAGS"])
    ifelse([$2], [], [:], [$2])
else
    ifelse([$3], [], [:], [$3])
fi
])

#
#   DS_ORACLE_LIBS([ora_ldflags_out],
#                  [ora_libs_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_ORACLE_LIBS],
[
ora_libs_save_LIBS="$LIBS"
ora_libs_save_LDFLAGS="$LDFLAGS"
ora_libs_LIBS=''
ora_libs_LDFLAGS="-L$with_oracle_home/lib" 
ora_libs_success=no

#
#    oracle version
#
AC_ARG_WITH(oracle-version,
    [AS_HELP_STRING([--with-oracle-version=MAJOR],
                    [Oracle's Major Version Number])])
AC_MSG_CHECKING([Oracle Major Version Number])
if test x"$with_oracle_version" = x
then
    AC_MSG_RESULT([default])
else
    AC_MSG_RESULT([$with_oracle_version])
fi

AC_MSG_CHECKING([for OCI Function calls in -lclntsh])

ora_libs_LIBS='-lclntsh'
if test x"$with_oracle_version" = x10
then
  ora_libs_LIBS='-lclntsh -lnnz10'
fi
LIBS="$ora_libs_LIBS $ora_libs_save_LIBS"
LDFLAGS="$ora_libs_LDFLAGS $ora_libs_save_LDFLAGS"

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdlib.h>
#include <oci.h>
    ]],
    [[
    OCIEnv *envhp;
    OCIError *errhp;
    OCIInitialize(OCI_DEFAULT, NULL, NULL, NULL, NULL);
    OCIEnvInit(&envhp, OCI_DEFAULT, 0, NULL);
    OCIHandleAlloc(envhp, (void**)&errhp, OCI_HTYPE_ERROR, 0, NULL);
    return 0;
    ]])],
    [ ora_libs_success=yes ],
    [ ora_libs_success=no ]
    )
AC_MSG_RESULT($ora_libs_success)

LIBS="$ora_libs_save_LIBS"
LDFLAGS="$ora_libs_save_LDFLAGS"
if test x"$ora_libs_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ora_libs_LDFLAGS"])
    ifelse([$2], [], [:], [$2="$ora_libs_LIBS"])
    ifelse([$3], [], [:], [$3])
else
    ifelse([$4], [], [:], [$4])
fi
])

#
#   DS_ORACLE([db_cppflags_out], [db_ldflags_out], [db_libs_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_ORACLE],
[
ora_save_CPPFLAGS="$CPPFLAGS"
ora_save_LDFLAGS="$LDFLAGS"
ora_save_LIBS="$LIBS"

ora_CPPFLAGS=''
ora_LDFLAGS=''
ora_LIBS=''

ora_success=yes

DS_ORACLE_HEADERS([ora_CPPFLAGS], [], [ora_success=no])

if test x"$ora_success" = xyes
then
    CPPFLAGS="$ora_CPPFLAGS $CPPFLAGS"
    DS_ORACLE_LIBS([ora_LDFLAGS], [ora_LIBS], [], [ora_success=no])
fi

CPPFLAGS="$ora_save_CPPFLAGS"
LDFLAGS="$ora_save_LDFLAGS"
LIBS="$ora_save_LIBS"

if test x"$ora_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ora_CPPFLAGS"])
    ifelse([$2], [], [:], [$2="$ora_LDFLAGS"])
    ifelse([$3], [], [:], [$3="$ora_LIBS"])
    ifelse([$4], [], [:], [$4])
else
    ifelse([$5], [], [:], [$5])
fi
])
