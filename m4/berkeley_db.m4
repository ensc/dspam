# $Id: berkeley_db.m4,v 1.1 2004/10/24 20:48:37 jonz Exp $
# Autuconf macros for checking for Berkeley DB
# Andrew W. Nosenko <awn@bcs.zp.ua>
#
#   Public available macro:
#       DS_BERKELEY_DB([db_cppflags_out],
#                      [db_ldflags_out],
#                      [db_libs_out],
#                      [db_version_major_out],
#                      [db_version_minor_out],
#                      [db_version_patchlevel_out],
#                      [additional-action-if-found],
#                      [additional-action-if-not-found]
#                      )
#
#   Another macros are considered as private for implementation and
#   sould not be used in the configure.ac.  At this time these are:
#       DS_BERKELEY_DB_HEADERS()
#       DS_BERKELEY_DB_LIBS()
#

#   DS_BERKELEY_DB_HEADERS([db_cppflags_out],
#                  [db_version_major_out],
#                  [db_version_minor_out],
#                  [db_version_patchelevel_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_BERKELEY_DB_HEADERS],
[
AC_REQUIRE([AC_PROG_AWK])

ds_db_headers_save_CPPFLAGS="$CPPFLAGS"
ds_db_headers_CPPFLAGS=''
ds_db_headers_success=yes
ds_db_headers_version_major=''
ds_db_headers_version_minor=''
ds_db_headers_version_patchlevel=''

# unistd.h and errno.h are needed for header version check below.
AC_CHECK_HEADERS([unistd.h errno.h])

AC_ARG_WITH(db-includes,
            [AS_HELP_STRING([--with-db-includes=DIR],
                            [Where to find Berkeley DB headers])])
if test x"$with_db_includes" != x
then
    if test -d "$with_db_includes"
    then
        :
    else
        AC_MSG_ERROR([required include path for db headers ($with_db_includes) is not a directory])
    fi
    ds_db_headers_CPPFLAGS="-I$with_db_includes"
    CPPFLAGS="$ds_db_headers_CPPFLAGS $CPPFLAGS"
fi
AC_CHECK_HEADER([db.h],
                [],
                [ ds_db_headers_success=no ])
if test x"$ds_db_headers_success" = xyes
then
    # Determine Berkeley DB hearder version
    AC_LANG_PUSH(C)
    AC_MSG_CHECKING([Berkeley DB header version])
    AC_RUN_IFELSE([AC_LANG_SOURCE([[
        #include <db.h>
        #include <stdio.h>
        #ifdef HAVE_UNISTD_H
        #   include <unistd.h>
        #endif
        #ifdef HAVE_ERRNO_H
        #   include <errno.h>
        #endif

        #define OUTFILE "conftest.libdbver"

        int main(void)
        {
            FILE* fd;
            int rc;

            rc = unlink(OUTFILE);   /* avoid symlink attack */
            if (rc < 0 && errno != ENOENT)
            {
                fprintf(stderr, "error unlinking '%s'", OUTFILE);
                exit(1);
            }

            fd = fopen(OUTFILE, "w");
            if (!fd)
            {
                /* Don't try to investigate errno for portability reasons */
                fprintf(stderr, "error opening '%s' for writing", OUTFILE);
                exit(1);
            }

            rc = fprintf(fd, "%d.%d.%d",
                         DB_VERSION_MAJOR, DB_VERSION_MINOR, DB_VERSION_PATCH);
            if (rc < 0)
            {
                fprintf(stderr, "error writing to the '%s'", OUTFILE);
                exit(1);
            }
            exit(0);
        }
        ]])],
        [
            dnl In following AWK calls `$[1]' is used instead of `$1'
            dnl for preventing substitution by macro arguments.  Don't
            dnl worry, in final `configure' these `$[1]' will appears as
            dnl required: `$1'.

            ds_db_headers_verstr=`cat conftest.libdbver`
            ds_db_headers_version_major=`cat conftest.libdbver | $AWK -F. '{print $[1]}'`
            ds_db_headers_version_minor=`cat conftest.libdbver | $AWK -F. '{print $[2]}'`
            ds_db_headers_version_patchlevel=`cat conftest.libdbver | $AWK -F. '{print $[3]}'`

            AC_MSG_RESULT([$ds_db_headers_version_major.$ds_db_headers_version_minor.$ds_db_headers_version_patchlevel])
        ],
        [
            AC_MSG_RESULT([failure (unsupported v1.x?)])
            ds_db_headers_success=no
        ],
        [   # cross-compilation
            AC_MSG_ERROR([cross-compilation is unsupported, sorry])
            ds_db_headers_success=no
        ])
    AC_LANG_POP(C)
fi
CPPFLAGS="$ds_db_headers_save_CPPFLAGS"
if test x"$ds_db_headers_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ds_db_headers_CPPFLAGS"])
    ifelse([$2], [], [:], [$2="$ds_db_headers_version_major"])
    ifelse([$3], [], [:], [$3="$ds_db_headers_version_minor"])
    ifelse([$4], [], [:], [$4="$ds_db_headers_version_patchlevel"])
    ifelse([$5], [], [:], [$5])
    :
else
    ifelse([$6], [], [:], [$6])
    :
fi
])

#
#   DS_BERKELEY_DB_LIBS([db_ldflags_out],
#                  [db_libs_out],
#                  [db_version_major_in],
#                  [db_version_minor_in],
#                  [db_version_patchelevel_in],     # unused
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_BERKELEY_DB_LIBS],
[
ds_db_libs_save_LIBS="$LIBS"
ds_db_libs_save_LDFLAGS="$LDFLAGS"
ds_db_libs_LIBS=''
ds_db_libs_LDFLAGS=''
ds_db_libs_success=no

ds_db_libs_ver_major="${$3}"
ds_db_libs_ver_minor="${$4}"

if test x"$ds_db_libs_ver_major" = x
then
    AC_MSG_ERROR([[DS@&t@_BERKELEY_DB_LIBS: non-optional argument _ds_db_libs_version_major_in is omited]]);
fi
if test x"$ds_db_libs_ver_minor" = x
then
    AC_MSG_ERROR([[DS@&t@_BERKELEY_DB_LIBS: non-optional argument _ds_db_libs_version_minor_in is omited]]);
fi

AC_ARG_WITH(db-libraries,
            [AS_HELP_STRING([--with-db-libraries=DIR],
                            [Where to find Berkeley DB libraries])])
if test x"$with_db_libraries" != x
then
    if test -d "$with_db_libraries"
    then
        :
    else
        AC_MSG_ERROR([required path for db libraries ($with_db_libraries) is not a directory])
    fi
    ds_db_libs_LDFLAGS="-L$with_db_libraries"
fi

AC_MSG_CHECKING([how to link Berkeley DB libraries])
for ds_db_libs_tmp_libdb in \
    "-ldb-${ds_db_libs_ver_major}.${ds_db_libs_ver_minor}"  \
    "-ldb${ds_db_libs_ver_major}${ds_db_libs_ver_minor}"    \
    "-ldb-${ds_db_libs_ver_major}"  \
    "-ldb${ds_db_libs_ver_major}"
do
    LDFLAGS="$ds_db_libs_LDFLAGS $ds_db_libs_save_LDFLAGS"
    for ds_db_libs_tmp_libpth in '' '-lpthread'
    do
        ds_db_libs_LIBS="$ds_db_libs_tmp_libdb $ds_db_libs_tmp_libpth"
        LIBS="$ds_db_libs_LIBS $ds_db_libs_save_LIBS"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[
            #include <db.h>
            ]],
            [[
                DB** dummy_dbp = NULL;
                db_create(dummy_dbp, NULL, 0);
            ]])],
            [ ds_db_libs_success=yes ],
            [ ds_db_libs_success=no ]
            )

        if test x"$ds_db_libs_success" != xyes
        then
            continue
        fi

        DS_LIBTOOL_RUN_IFELSE([AC_LANG_PROGRAM([[
            #include <stdio.h>
            #include <db.h>
            ]],
            [[
                int major, minor, patchlevel;
                int is_match;
                db_version(&major, &minor, &patchlevel);
                fprintf(stderr, "db version from header: %d.%d.%d\n",
                        DB_VERSION_MAJOR,
                        DB_VERSION_MINOR,
                        DB_VERSION_PATCH
                        );
                fprintf(stderr, "db version from library: %d.%d.%d\n",
                        major,
                        minor,
                        patchlevel
                        );
                if (major == DB_VERSION_MAJOR
                    && minor == DB_VERSION_MINOR
                    && patchlevel == DB_VERSION_PATCH
                    )
                {
                    is_match = 1;
                }
                else
                {
                    is_match = 0;
                }
                return is_match ? 0 : 1;
            ]])],
            [ ds_db_libs_success=yes],
            [ ds_db_libs_success=no ],
            [ ds_db_libs_success=yes]  # Assume success for cross-compiling
            )

        if test x"$ds_db_libs_success" = xyes
        then
            break 2
        fi
    done
done

if test x"$ds_db_libs_success" = xyes
then
    AC_MSG_RESULT([$ds_db_libs_LIBS])
else
    AC_MSG_RESULT([failure])
fi

LIBS="$ds_db_libs_save_LIBS"
LDFLAGS="$ds_db_libs_save_LDFLAGS"
if test x"$ds_db_libs_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ds_db_libs_LDFLAGS"])
    ifelse([$2], [], [:], [$2="$ds_db_libs_LIBS"])
    ifelse([$6], [], [:], [$6]);
    :
else
    ifelse([$7], [], [:], [$7])
    :
fi
])

#
#   DS_BERKELEY_DB([db_cppflags_out],
#                  [db_ldflags_out],
#                  [db_libs_out],
#                  [db_version_major_out],
#                  [db_version_minor_out],
#                  [db_version_patchlevel_out],
#                  [additional-action-if-found],
#                  [additional-action-if-not-found]
#                 )
AC_DEFUN([DS_BERKELEY_DB],
[
ds_db_save_CPPFLAGS="$CPPFLAGS"
ds_db_save_LIBS="$LIBS"
ds_db_save_LDFLAGS="$LDFLAGS"

ds_db_CPPFLAGS=''
ds_db_LIBS=''
ds_db_LDFLAGS=''

ds_db_success=yes
ds_db_version_major=''
ds_db_version_minor=''
ds_db_version_patchlevel=''

DS_BERKELEY_DB_HEADERS([ds_db_CPPFLAGS],
    [ds_db_version_major],
    [ds_db_version_minor],
    [ds_db_version_patchlevel],
    [],
    [ds_db_success=no])

if test x"$ds_db_success" = xyes
then
    CPPFLAGS="$ds_db_CPPFLAGS $CPPFLAGS"
    DS_BERKELEY_DB_LIBS([ds_db_LDFLAGS],
        [ds_db_LIBS],
        [ds_db_version_major],
        [ds_db_version_minor],
        [ds_db_version_patchlevel],
        [],
        [ds_db_success=no])
fi

CPPFLAGS="$ds_db_save_CPPFLAGS"
LIBS="$ds_db_save_LIBS"
LDFLAGS="$ds_db_save_LDFLAGS"

if test x"$ds_db_success" = xyes
then
    ifelse([$1], [], [:], [$1="$ds_db_CPPFLAGS"])
    ifelse([$2], [], [:], [$2="$ds_db_LDFLAGS"])
    ifelse([$3], [], [:], [$3="$ds_db_LIBS"])

    ifelse([$4], [], [:], [$4="$ds_db_version_major"])
    ifelse([$5], [], [:], [$5="$ds_db_version_minor"])
    ifelse([$6], [], [:], [$6="$ds_db_version_patchlevel"])

    ifelse([$7], [], [:], [$7])
    :
else
    ifelse([$8], [], [:], [$8])
    :
fi
])
