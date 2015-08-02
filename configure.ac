#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([AutoClimber], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/autoclimber.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])


AM_INIT_AUTOMAKE([subdir-objects foreign])

# dnl Checks for lib
AC_DISABLE_STATIC
AC_DISABLE_SHARED
LT_INIT([dlopen])

# Checks for programs.
AC_PROG_CC

#dnl Checks for crypto library
AC_ARG_WITH(
  [crypto-library],
  [AS_HELP_STRING([--with-crypto-library=library], [build with the given crypto library, TYPE=openssl|polarssl|mbedtls @<:@default=openssl@:>@])],
  [
    case "${withval}" in
      openssl|polarssl|mbedtls) ;;
      *) AC_MSG_ERROR([bad value ${withval} for --with-crypto-library]) ;;
    esac
  ],
  [with_crypto_library="openssl"]
)

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for libev
m4_include([libev/libev.m4])

# Checks for library functions.
AC_CONFIG_FILES([ Makefile
		libev/Makefile
		src/Makefile])

AC_CONFIG_SUBDIRS([libev])
AC_OUTPUT
