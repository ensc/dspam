# $Id: external_lookup.m4,v 1.1 2008/05/07 21:58:47 mjohnson Exp $
# m4/external_lookup.m4
# Hugo Monteiro <hugo.monteiro@javali.pt>
#
#   DS_EXT_LOOKUP()
#
#   Activate external user lookup
#
AC_DEFUN([DS_EXT_LOOKUP],
[

  AC_ARG_ENABLE(external-lookup,
      [AS_HELP_STRING(--enable-external-lookup,
                        Enable external lookup support
                      )])
  AC_MSG_CHECKING([whether to enable external lookup support])
  case x"$enable_external_lookup" in
      xyes)   # external lookup enabled explicity
              ;;
      xno)    # external lookup disabled explicity
              ;;
      x)      # external lookup disabled by default
              enable_external_lookup=no
              ;;
      *)      AC_MSG_ERROR([unexpected value $enable_external_lookup for --{enable,disable}-external-lookup configure option])
              ;;
  esac
  if test x"$enable_external_lookup" != xyes
  then
      enable_external_lookup=no
  else
      enable_external_lookup=yes    # overkill, but convenient
      AC_DEFINE(EXT_LOOKUP, 1, [Defined if external lookup is enabled])

      LIBS="$LIBS -lldap -llber"
  fi
  AC_MSG_RESULT([$enable_external_lookup])
])
