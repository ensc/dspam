# $Id: libdb3_drv.m4,v 1.1 2004/10/24 20:48:37 jonz Exp $
# Autuconf macros for checking for Berkeley DB
# Andrew W. Nosenko <awn@bcs.zp.ua>
#
#   Public available macro:
#       DS_BERKELEY_DB3([db_cppflags_out],
#                       [db_ldflags_out],
#                       [db_libs_out],
#                       [db_version_major_out], 
#                       [db_version_minor_out], 
#                       [db_version_patchlevel_out], 
#                       [additional-action-if-found],
#                       [additional-action-if-not-found]
#                       )

#
#   DS_BERKELEY_DB3([db_cppflags_out],
#                   [db_ldflags_out],
#                   [db_libs_out],
#                   [db_version_major_out], 
#                   [db_version_minor_out], 
#                   [db_version_patchlevel_out], 
#                   [additional-action-if-found],
#                   [additional-action-if-not-found]
#                   )
AC_DEFUN([DS_BERKELEY_DB3],
[
ds_db3_save_CPPFLAGS="$CPPFLAGS"
ds_db3_save_LDFLAGS="$LDFLAGS"
ds_db3_save_LIBS="$LIBS"

ds_db3_own_CPPFLAGS=''
ds_db3_own_LDFLAGS=''

ds_db3_CPPFLAGS=''
ds_db3_LIBS=''
ds_db3_LDFLAGS=''

AC_ARG_WITH(db3-includes,
    [AS_HELP_STRING([--with-db3-includes=DIR],
                    [
                        Where to find Berkeley DB headers (compatibility
                        alias for --with-db-includes)
                    ])])
if test x"$with_db3_includes" != x
then
    if test -d "$with_db3_includes"
    then
        :
    else
        AC_MSG_ERROR([required include path for db3 headers ($with_db3_includes) is not a directory])
    fi
    ds_db3_own_CPPFLAGS="-I$with_db3_includes"
    CPPFLAGS="$ds_db3_own_CPPFLAGS $CPPFLAGS"
fi

AC_ARG_WITH(db3-libraries,
    [AS_HELP_STRING([--with-db3-libraries=DIR],
                    [
                        Where to find Berkeley DB libraries
                        (compatibility alias for --with-db-libraries)
                    ])])
if test x"$with_db3_libraries" != x
then
    if test -d "$with_db3_libraries"
    then
        :
    else
        AC_MSG_ERROR([required path for db3 libraries ($with_db3_libraries) is not a directory])
    fi
    ds_db3_own_LDFLAGS="-L$with_db3_libraries"
    LDFLAGS="$ds_db3_own_LDFLAGS $LDFLAGS"
fi

DS_BERKELEY_DB([ds_db3_CPPFLAGS], [ds_db3_LDFLAGS], [ds_db3_LIBS],
               [$4], [$5], [$6],
               [ds_db3_success=yes],
               [ds_db3_success=no])

CPPFLAGS="$ds_db3_save_CPPFLAGS"
LDFLAGS="$ds_db3_save_LDFLAGS"
LIBS="$ds_db3_save_LIBS"

if test x"$ds_db3_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ds_db3_CPPFLAGS $ds_db3_own_CPPFLAGS"])
    ifelse([$2], [], [:], [$2="$ds_db3_LDFLAGS $ds_db3_own_LDFLAGS"])
    ifelse([$3], [], [:], [$3="$ds_db3_LIBS"])
    ifelse([$7], [], [:], [$7])
    :
else
    ifelse([$8], [], [:], [$8])
    :
fi
])
