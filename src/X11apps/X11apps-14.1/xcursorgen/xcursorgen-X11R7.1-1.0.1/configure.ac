# $XdotOrg: app/xcursorgen/configure.ac,v 1.8 2006/05/19 18:33:36 ajax Exp $

AC_PREREQ([2.57])
AC_INIT(xcursorgen, [1.0.1], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg], xcursorgen)
AM_INIT_AUTOMAKE([dist-bzip2])
AM_MAINTAINER_MODE

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

AC_TYPE_SIGNAL

# Checks for pkg-config packages
PKG_CHECK_MODULES(XCURSORGEN, x11 xcursor libpng12)
AC_SUBST(XCURSOR_CFLAGS)
AC_SUBST(XCURSOR_LIBS)

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile])
