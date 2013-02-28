
#========== Initialisation ==========

AC_INIT([exio], [1.1], [bug-report@hello.example.com], [exio], [ ] )
AC_PREREQ([2.59])

# This macro causes the environment variables, $host, $build and $target to be
# defined.
AC_CANONICAL_SYSTEM

# Not: the -Wall here is for automake errors; is unrelated to gcc errors
AM_INIT_AUTOMAKE([1.10 no-define -Wall])

AC_CONFIG_HEADERS([config.h])
AC_PROG_CXX
AC_CONFIG_MACRO_DIR([m4])


# initialise libtool support
LT_INIT

#========== Checks for programs ==========

AC_PROG_CXX
AC_PROG_CC
AM_PROG_LIBTOOL
AC_PROG_INSTALL

#========== Check for third party libraries ==========

#PKG_CHECK_MODULES([DEPS], [gtkmm-2.4 >= 2.12.2 somethingelse-1.0 >= 1.0.2])

#========== Checks for header files ==========

AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h strings.h unistd.h getopt.h])


AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

#========== Generation ==========

# List the files that will be generated
AC_CONFIG_FILES([
Makefile
libcpp11/Makefile
libexio/Makefile
src/Makefile
])

# Trigger the generation of our makefiles
AC_OUTPUT


