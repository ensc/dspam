# $Id: split_configuration.m4,v 1.00 2009/12/22 12:25:59 sbajic Exp $
# m4/split_configuration.m4
# Hugo Monteiro <hugo.monteiro@fct.unl.pt>
#
#   DS_SPLIT_CONFIG()
#
#   Activate split configuration file support
#
AC_DEFUN([DS_SPLIT_CONFIG],
[

  AC_ARG_ENABLE(split-configuration,
      [AS_HELP_STRING(--enable-split-configuration,
                        Enable split configuration file support
                      )])
  AC_MSG_CHECKING([whether to enable split configuration file support])
  case x"$enable_split_configuration" in
      xyes)   # split configuration file enabled explicity
              ;;
      xno)    # split configuration file disabled explicity
              ;;
      x)      # split configuration file disabled by default
              enable_split_configuration=no
              ;;
      *)      AC_MSG_ERROR([unexpected value $enable_split_configuration for --{enable,disable}-split-configuration configure option])
              ;;
  esac
  if test x"$enable_split_configuration" != xyes
  then
      enable_split_configuration=no
  else
      enable_split_configuration=yes    # overkill, but convenient
      AC_DEFINE(SPLIT_CONFIG, 1, [Defined if split configuration is enabled])
  fi
  AC_MSG_RESULT([$enable_split_configuration])
])
