# $Id: dspam_functions.m4,v 1.0 2009/06/10 22:00:31 sbajic Exp $
# m4/dspam_functions.m4
# Stevan Bajic <stevan@bajic.ch>
#
#   Various m4 macros
#
#

dnl DS_INCLUDES_ARPA_INET
dnl -------------------------------------------------
dnl Set up variable with list of headers that must be
dnl included when arpa/inet.h is to be included.

AC_DEFUN([DS_INCLUDES_ARPA_INET],
[
ds_includes_arpa_inet="\
/* includes start */
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
/* includes end */"
  AC_CHECK_HEADERS(
    sys/types.h sys/socket.h netinet/in.h arpa/inet.h,
    [], [], [$ds_includes_arpa_inet])
])


dnl DS_CHECK_FUNC_INET_NTOA_R
dnl -------------------------------------------------
dnl Verify if inet_ntoa_r is available, prototyped,
dnl can be compiled and the count of arguments used.

AC_DEFUN([DS_CHECK_FUNC_INET_NTOA_R],
[
  AC_REQUIRE([DS_INCLUDES_ARPA_INET])dnl
  #
  test_prototyped_inet_ntoa_r="unknown"
  test_compiles_inet_ntoa_r="unknown"
  test_number_of_arguments_inet_ntoa_r="unknown"
  #
  AC_CHECK_FUNCS(inet_ntoa_r)
  #
  if test "$ac_cv_func_inet_ntoa_r" = "yes"; then
    AC_MSG_CHECKING([if inet_ntoa_r is prototyped])
    AC_EGREP_CPP([inet_ntoa_r],[
      $ds_includes_arpa_inet
    ],[
      AC_MSG_RESULT([yes])
      test_prototyped_inet_ntoa_r="yes"
    ],[
      AC_MSG_RESULT([no])
      test_prototyped_inet_ntoa_r="no"
    ])
  fi
  #
  if test "$test_prototyped_inet_ntoa_r" = "yes"; then
    if test "$test_number_of_arguments_inet_ntoa_r" = "unknown"; then
      AC_MSG_CHECKING([if inet_ntoa_r takes 2 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $ds_includes_arpa_inet
        ]],[[
          struct in_addr addr;
          if(0 != inet_ntoa_r(addr, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        test_compiles_inet_ntoa_r="yes"
        test_number_of_arguments_inet_ntoa_r="2"
      ],[
        AC_MSG_RESULT([no])
        test_compiles_inet_ntoa_r="no"
      ])
    fi
    if test "$test_number_of_arguments_inet_ntoa_r" = "unknown"; then
      AC_MSG_CHECKING([if inet_ntoa_r takes 3 args.])
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
          $ds_includes_arpa_inet
        ]],[[
          struct in_addr addr;
          if(0 != inet_ntoa_r(addr, 0, 0))
            return 1;
        ]])
      ],[
        AC_MSG_RESULT([yes])
        test_compiles_inet_ntoa_r="yes"
        test_number_of_arguments_inet_ntoa_r="3"
      ],[
        AC_MSG_RESULT([no])
        test_compiles_inet_ntoa_r="no"
      ])
    fi
    AC_MSG_CHECKING([if inet_ntoa_r is compilable])
    if test "$test_compiles_inet_ntoa_r" = "yes"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
  #
  AC_MSG_CHECKING([if inet_ntoa_r might be used])
  if test "$test_prototyped_inet_ntoa_r" = "yes" &&
     test "$test_compiles_inet_ntoa_r" = "yes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_INET_NTOA_R, 1,
      [Define to 1 if you have the inet_ntoa_r function.])
    dnl AC_DEFINE_UNQUOTED(INET_NTOA_R_ARGS, $test_number_of_arguments_inet_ntoa_r,
    dnl   [Specifies the number of arguments to inet_ntoa_r])
    #
    if test "$test_number_of_arguments_inet_ntoa_r" -eq "2"; then
      AC_DEFINE(HAVE_INET_NTOA_R_2, 1, [inet_ntoa_r() takes 2 args])
    elif test "$test_number_of_arguments_inet_ntoa_r" -eq "3"; then
      AC_DEFINE(HAVE_INET_NTOA_R_3, 1, [inet_ntoa_r() takes 3 args])
    fi
    #
    ac_cv_func_inet_ntoa_r="yes"
  else
    AC_MSG_RESULT([no])
    ac_cv_func_inet_ntoa_r="no"
  fi
])
