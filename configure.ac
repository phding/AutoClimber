#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([AutoClimber], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/socks5.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])


AM_INIT_AUTOMAKE([subdir-objects foreign])

# dnl Checks for lib
AC_DISABLE_STATIC
AC_DISABLE_SHARED
LT_INIT([dlopen])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for libev
m4_include([libev/libev.m4])

# Checks for library functions.
AC_CONFIG_FILES([Makefile
                 src/Makefile])

AC_CONFIG_SUBDIRS([libev])
AC_OUTPUT
