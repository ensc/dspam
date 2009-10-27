# $Id: external_lookup.m4,v 1.2 2009/10/27 23:13:09 sbajic Exp $
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
      AC_MSG_RESULT([$enable_external_lookup])
  else
      enable_external_lookup=yes    # overkill, but convenient
      AC_MSG_RESULT([$enable_external_lookup])
      AC_DEFINE(EXT_LOOKUP, 1, [Defined if external lookup is enabled])

      # Check for LDAP and LDAP version
      AC_CHECK_HEADERS([lber.h ldap.h])
      save_libs="$LIBS"
      if test x"$ac_cv_header_ldap_h" = "xyes" -a x"$ac_cv_header_lber_h" = "xyes"
	  then
          AC_CHECK_LIB(lber, ber_alloc)
          AC_CHECK_LIB(ldap, ldap_init)
      fi
      if test x"$ac_cv_lib_lber_ber_alloc" = "xyes" -a x"$ac_cv_lib_ldap_ldap_init" = "xyes"
	  then
          AC_MSG_CHECKING([for OpenLDAP version >= 2.2.0])
          AC_RUN_IFELSE([AC_LANG_PROGRAM([[#include <lber.h>
                                           #include <ldap.h>]],
                                         [[do {
                                               LDAPAPIInfo info;
                                               info.ldapai_info_version = LDAP_API_INFO_VERSION;
                                               ldap_get_option(0, LDAP_OPT_API_INFO, &info);
                                               if(info.ldapai_vendor_version != LDAP_VENDOR_VERSION || LDAP_VENDOR_VERSION < 20204)
                                                 return 1;
                                           } while(0)]])],
                                         [AC_MSG_RESULT([yes])
                                          have_ldap_version=yes],
                                          AC_MSG_RESULT([no]))
      fi
      LIBS="$save_libs"
      AC_MSG_CHECKING([whether to enable LDAP support in external lookup])
      if test x"$have_ldap_version" != "xyes" ; then
          AC_MSG_RESULT([no])
      else
          AC_MSG_RESULT([yes])
          LIBS="$LIBS -lldap -llber"
          AC_DEFINE(USE_LDAP, 1, [Defined if LDAP is found])
      fi

  fi
])
