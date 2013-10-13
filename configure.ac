AC_INIT([libstb-hal], [0.1.1])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_MACRO_DIR([m4])
AC_GNU_SOURCE

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
## both disabled => libtool still defaults to static
##                  at least the libtool I tested with ;-)
## --enable-shared => build only shared
## --enable-shared --enable-static => build both
AC_DISABLE_SHARED
AC_DISABLE_STATIC
AC_SYS_LARGEFILE
AC_PROG_LIBTOOL

if test x"$BOXTYPE" = x"tripledragon"; then
	TUXBOX_APPS_LIB_PKGCONFIG(DIRECTFB, directfb)
fi

AC_ARG_ENABLE(gstreamer,
	AS_HELP_STRING(--enable-gstreamer,use gstreamer playback),
	,[enable_gstreamer=no])

AM_CONDITIONAL(ENABLE_GSTREAMER,test "$enable_gstreamer" = "yes")
if test "$enable_gstreamer" = "yes"; then
	AC_DEFINE(ENABLE_GSTREAMER,1,[enable gstreamer])
fi

if test x$BOXTYPE = xgeneric -a x$BOXMODEL != xraspi; then
if test x$ENABLE_GSTREAMER = x1; then
	PKG_CHECK_MODULES([GSTREAMER], [gstreamer-0.10])
	PKG_CHECK_MODULES([GSTREAMER_INTERFACES], [gstinterfaces-0.10])
fi
	PKG_CHECK_MODULES([AVFORMAT], [libavformat >= 53.21.1])
	PKG_CHECK_MODULES([AVCODEC], [libavcodec >= 54.28.0])
	# don't know which version is exactly needed here...
	PKG_CHECK_MODULES([AVUTIL], [libavutil])
	PKG_CHECK_MODULES([SWSCALE], [libswscale])
	PKG_CHECK_MODULES([SWRESAMPLE], [libswresample])
fi
AC_OUTPUT([
Makefile
common/Makefile
libeplayer3/Makefile
libthread/Makefile
azbox/Makefile
generic-pc/Makefile
libduckbox/Makefile
libdvbci/Makefile
libtriple/Makefile
libspark/Makefile
raspi/Makefile
tools/Makefile
])

