# $Id: external_lookup.m4,v 1.4 2011/02/09 15:50:28 sbajic Exp $
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
      if test x"$ac_cv_header_ldap_h" = "xyes" -a x"$ac_cv_header_lber_h" = "xyes"
	  then
          AC_CHECK_LIB(lber, ber_alloc,AC_DEFINE([HAVE_LIBLBER], [1], [Define if you have liblber]))
          AC_CHECK_LIB(ldap, ldap_init,AC_DEFINE([HAVE_LIBLDAP], [1], [Define if you have libldap]))
      fi
      if test x"$ac_cv_lib_lber_ber_alloc" = "xyes" -a x"$ac_cv_lib_ldap_ldap_init" = "xyes"
      then
          AC_MSG_CHECKING([for OpenLDAP version >= 2.2.0])
          AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[
              #include <lber.h>
              #include <ldap.h>
            ]],[[
              LDAPAPIInfo info;
              #ifdef LDAP_API_INFO_VERSION
              info.ldapai_info_version = LDAP_API_INFO_VERSION;
              #else
              info.ldapai_info_version = 1;
              #endif
              if(ldap_get_option(NULL, LDAP_OPT_API_INFO, &info) != LDAP_SUCCESS)
                return 1;
              if(info.ldapai_vendor_version != LDAP_VENDOR_VERSION || LDAP_VENDOR_VERSION < 20204)
                return 1;
              return 0;
            ]])
          ],[
            AC_MSG_RESULT([yes])
            have_ldap_version=yes
          ],[
            AC_MSG_RESULT([no])
            have_ldap_version=no
          ],[ # cross-compilation
            AC_MSG_ERROR([cross-compilation is unsupported, sorry])
            have_ldap_version=no
          ])
      fi
      AC_MSG_CHECKING([whether to enable LDAP support in external lookup])
      if test x"$have_ldap_version" != "xyes" ; then
          AC_MSG_RESULT([no])
      else
          AC_MSG_RESULT([yes])
          external_lookup_libs="-lldap -llber"
          AC_SUBST(external_lookup_libs)
          AC_DEFINE(USE_LDAP, 1, [Defined if LDAP is found])
      fi

  fi
])
