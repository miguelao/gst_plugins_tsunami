#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])

AC_INIT([tsunami], [0.1.0])

# required versions
GST_REQUIRED=0.10.20
OPENCV_REQUIRED=2.3.0

# required version of automake
AM_INIT_AUTOMAKE([1.10])

# required version of libtool
LT_PREREQ([2.2.6])
LT_INIT

# check if we are in the correct dir
AC_CONFIG_SRCDIR([src/gstfacetracker/gstfacetracker.cpp])

AM_MAINTAINER_MODE([enable])

AC_CONFIG_MACRO_DIR([m4])

# Ruel to make config.h
AC_CONFIG_HEADERS([config.h])

# use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

# set prog lang to C++
AC_LANG([C++])

# Checks for programs.

AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

# set g++ as c compiller
CC=g++


# Create Automake conditional based on the DOXYGEN variable
#AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])
#AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/doxyfile])])

# Checks for libraries.

PKG_CHECK_MODULES(GST, [
  gstreamer-1.0 >= $GST_REQUIRED
  gstreamer-base-1.0 >= $GST_REQUIRED
  gstreamer-video-1.0 >= $GST_REQUIRED
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
    You need to install or upgrade the GStreamer development
    packages on your system. On debian-based systems these are
    libgstreamer0.10-dev and libgstreamer-plugins-base0.10-dev.
    on RPM-based systems gstreamer0.10-devel, libgstreamer0.10-devel
    or similar. The minimum version required is $GST_REQUIRED.
  ])
])

PKG_CHECK_MODULES(OPENCV, [
  opencv >= $OPENCV_REQUIRED
], [
  AC_SUBST(OPENCV_CFLAGS)
  AC_SUBST(OPENCV_LIBS)
]
)

PKG_CHECK_MODULES(LIBPNG, [
  libpng
], [
  AC_SUBST(LIBPNG_CFLAGS)
  AC_SUBST(LIBPNG_LIBS)
]
)

PKG_CHECK_MODULES(LIBCURL, [
  libcurl
], [
  AC_SUBST(LIBCURL_CFLAGS)
  AC_SUBST(LIBCURL_LIBS)
]
)


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

YUV_CHECK

# Checks for library functions.

# Extra Setup

plugindir="\$(libdir)/gstreamer-0.10"
AC_SUBST(plugindir)

GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)
# Gen. Output
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

YUV_OUTPUT


