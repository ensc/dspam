# $Id: dclassify.m4,v 1.1 2005/01/01 02:05:05 jonz Exp $
# Autoconf macros for libdclassify module
# Jonathan Zdziarski <jonathan@nuclearelephant.com>
#
#   Public available macro:
#       DS_DCLASSIFY([dclassify_cppflags_out],
#                [dclassify_libs_out],
#                [dclassify_ldflags_out]
#                [additional-action-if-found],
#                [additional-action-if-not-found]
#                )
#
#   Another macros are considered as private for implementation and
#   sould not be used in the configure.ac.  At this time these are:
#       DS_DCLASSIFY_HEADERS()
#       DS_DCLASSIFY_LIBS()
#
AC_DEFUN([DS_DCLASSIFY_HEADERS],
[
dclassify_headers_save_CPPFLAGS="$CPPFLAGS"
dclassify_headers_CPPFLAGS=''
dclassify_headers_success=yes

AC_ARG_WITH(dclassify-includes,
    [AS_HELP_STRING([--with-dclassify-includes=DIR],
                    [Where to find dclassify headers])])
AC_MSG_CHECKING([where to find dclassify headers])
if test x"$with_dclassify_includes" = x
then
    AC_MSG_RESULT([compiler default paths])
else
    AC_MSG_RESULT([$with_dclassify_includes])
    if test -d "$with_dclassify_includes"
    then
        :
    else
        AC_MSG_ERROR([required include path for dclassify headers $with_dclassify_includes is not a directory])
    fi
    dclassify_headers_CPPFLAGS="-I$with_dclassify_includes"
    CPPFLAGS="$dclassify_headers_CPPFLAGS $CPPFLAGS"
fi
AC_CHECK_HEADER([libdclassify.h],
                [],
                [ dclassify_headers_success=no ])
if test x"$dclassify_headers_success" = xyes
then
    AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
    #include <libdclassify.h>
    #ifdef DCLASSIFY_VERSION_MAJOR
    /* Success */
    #else
    #error Unsupported version of dclassify
    #endif
            ]])],
            [],
            [
                AC_MSG_FAILURE([Unsupported version of dclassify (no DCLASSIFY_VERSION_MAJOR defined)])
                dclassify_headers_success=no
            ])
fi

CPPFLAGS="$dclassify_headers_save_CPPFLAGS"
if test x"$dclassify_headers_success" = xyes
then
    ifelse([$1], [], [:], [$1="$dclassify_headers_CPPFLAGS"])
    ifelse([$2], [], [:], [$2])
else
    ifelse([$3], [], [:], [$3])
fi
])

#
#   DS_DCLASSIFY_LIBS([dclassify_ldflags_out], [dclassify_libs_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_DCLASSIFY_LIBS],
[
dclassify_libs_save_LDFLAGS="$LDFLAGS"
dclassify_libs_save_LIBS="$LIBS"
dclassify_libs_LDFLAGS=''
dclassify_libs_LIBS=''
dclassify_libs_success=yes

AC_ARG_WITH(dclassify-libraries,
    [AS_HELP_STRING([--with-dclassify-libraries=DIR],
                    [Where to find dclassify libraries])])
AC_MSG_CHECKING([where to find dclassify libraries])
if test x"$with_dclassify_libraries" = x
then
    AC_MSG_RESULT([compiler default paths])
else
    AC_MSG_RESULT([$with_dclassify_libraries])
    if test -d "$with_dclassify_libraries"
    then
        :
    else
        AC_MSG_ERROR([required path for dclassify libraries ($with_dclassify_libraries) is not a directory])
    fi
    dclassify_libs_LDFLAGS="-L$with_dclassify_libraries"
fi

AC_MSG_CHECKING([for valid -ldclassify])
dclassify_libs_LIBS="-ldclassify"
LIBS="$dclassify_libs_LIBS $dclassify_libs_save_LIBS"
LDFLAGS="$dclassify_libs_LDFLAGS $dclassify_libs_save_LDFLAGS"

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
        #include <libdclassify.h>
        ]],
        [[
            dc_diction_t diction = dc_diction_create(1);
        ]])],
        [ dclassify_libs_success=yes ],
        [ dclassify_libs_success=no ]
        )
AC_MSG_RESULT([$dclassify_libs_success])

LIBS="$dclassify_libs_save_LIBS"
LDFLAGS="$dclassify_libs_save_LDFLAGS"
if test x"$dclassify_libs_success" = xyes
then
    ifelse([$1], [], [:], [$1="$dclassify_libs_LDFLAGS"])
    ifelse([$2], [], [:], [$2="$dclassify_libs_LIBS"])
    ifelse([$3], [], [:], [$3])
else
    ifelse([$4], [], [:], [$4])
fi
])

#
#   DS_DCLASSIFY([db_cppflags_out], [db_ldflags_out], [db_libs_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_DCLASSIFY],
[
dclassify_save_CPPFLAGS="$CPPFLAGS"
dclassify_save_LDFLAGS="$LDFLAGS"
dclassify_save_LIBS="$LIBS"

dclassify_CPPFLAGS=''
dclassify_LIBS=''
dclassify_LDFLAGS=''

dclassify_success=yes

DS_DCLASSIFY_HEADERS([dclassify_CPPFLAGS], [], [dclassify_success=no])

if test x"$dclassify_success" = xyes
then
    CPPFLAGS="$dclassify_CPPFLAGS $CPPFLAGS"
    DS_DCLASSIFY_LIBS([dclassify_LDFLAGS], [dclassify_LIBS], [], [dclassify_success=no])
fi

CPPFLAGS="$dclassify_save_CPPFLAGS"
LDFLAGS="$dclassify_save_LDFLAGS"
LIBS="$dclassify_save_LIBS"

if test x"$dclassify_success" = xyes
then
    ifelse([$1], [], [:], [$1="$dclassify_CPPFLAGS"])
    ifelse([$2], [], [:], [$2="$dclassify_LDFLAGS"])
    ifelse([$3], [], [:], [$3="$dclassify_LIBS"])
    ifelse([$4], [], [:], [$4])
else
    ifelse([$5], [], [:], [$5])
fi
])
