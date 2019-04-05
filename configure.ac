# explicit defines for separate revision handling
define(ver_major, 1)
define(ver_minor, 1)
define(ver_micro, 0)

# sync with current git
define(ver_git, m4_esyscmd([
	GITBRANCH=$(git rev-parse --abbrev-ref HEAD);
	GITDESCRIBE=$(git describe --always --tags --dirty);
	printf "$GITDESCRIBE $GITBRANCH"
]))

AC_PACKAGE_NAME, PACKAGE_NAME_LIBSTB_HAL
AC_INIT([Tuxbox-libstb-hal], [ver_major.ver_minor.ver_micro])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([libstb-hal-config.h:config.h.in])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_MACRO_DIR([m4])
AC_GNU_SOURCE

AC_DEFINE(PACKAGE_VERSION_MAJOR, ver_major, [Major version number])
AC_DEFINE(PACKAGE_VERSION_MINOR, ver_minor, [Minor version number])
AC_DEFINE(PACKAGE_VERSION_MICRO, ver_micro, [Micro version number])
AC_DEFINE(PACKAGE_VERSION_GIT, "ver_git", [internal vcs version info])

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
PKG_PROG_PKG_CONFIG
## both disabled => libtool still defaults to static
##                  at least the libtool I tested with ;-)
## --enable-shared => build only shared
## --enable-shared --enable-static => build both
AC_DISABLE_SHARED
AC_DISABLE_STATIC
AC_SYS_LARGEFILE
AC_PROG_LIBTOOL

AC_ARG_ENABLE(clutter,
	AS_HELP_STRING(--enable-clutter, use clutter instead of OpenGL),
	,[enable_clutter=no])

AM_CONDITIONAL(USE_CLUTTER,test "$enable_clutter" = "yes")
AM_CONDITIONAL(USE_OPENGL,test "$enable_clutter" = "no")
if test "$enable_clutter" = "yes"; then
	AC_DEFINE(USE_CLUTTER,1,[use clutter instead of opengl])
else
	AC_DEFINE(USE_OPENGL,1,[use opengl instead of clutter])
fi

if test x"$BOXTYPE" = x"tripledragon"; then
	PKG_CHECK_MODULES([DIRECTFB], [directfb])
fi

AC_ARG_ENABLE(gstreamer_01,
	AS_HELP_STRING(--enable-gstreamer_01, use gstreamer 0.10 playback),
	,[enable_gstreamer_01=no])

AM_CONDITIONAL(ENABLE_GSTREAMER_01,test "$enable_gstreamer_01" = "yes")
if test "$enable_gstreamer_01" = "yes"; then
	AC_DEFINE(ENABLE_GSTREAMER_01, 1, [use gstreamer 0.10 playback])
	PKG_CHECK_MODULES([GSTREAMER], [gstreamer-0.10])
	PKG_CHECK_MODULES([GSTREAMER_INTERFACES], [gstinterfaces-0.10])
fi

AC_ARG_ENABLE(gstreamer_10,
	AS_HELP_STRING(--enable-gstreamer_10, use gstreamer 1.0 playback),
	,[enable_gstreamer_10=no])

AM_CONDITIONAL(ENABLE_GSTREAMER_10, test "$enable_gstreamer_10" = "yes")
if test "$enable_gstreamer_10" = "yes"; then
	AC_DEFINE(ENABLE_GSTREAMER_10, 1, [use gstreamer 1.0 playback])
	PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0])
	PKG_CHECK_MODULES([GSTREAMER_AUDIO], [gstreamer-audio-1.0])
	PKG_CHECK_MODULES([GSTREAMER_VIDEO], [gstreamer-video-1.0])
fi

if test x$BOXTYPE = xarmbox -a "$enable_gstreamer_10" = "yes"; then
	PKG_CHECK_MODULES([GSTREAMER], [gstreamer-1.0])
	PKG_CHECK_MODULES([GSTREAMER_AUDIO], [gstreamer-audio-1.0])
	PKG_CHECK_MODULES([GSTREAMER_VIDEO], [gstreamer-video-1.0])
fi

if test x$BOXTYPE = xgeneric -a x$BOXMODEL != xraspi; then
	if test x"$enable_clutter" = xyes; then
		PKG_CHECK_MODULES([CLUTTER], [clutter-1.0])
	fi
	PKG_CHECK_MODULES([AVFORMAT], [libavformat >= 53.21.1])
	PKG_CHECK_MODULES([AVCODEC], [libavcodec >= 54.28.0])
	# don't know which version is exactly needed here...
	PKG_CHECK_MODULES([AVUTIL], [libavutil])
	PKG_CHECK_MODULES([SWSCALE], [libswscale])
	PKG_CHECK_MODULES([SWRESAMPLE], [libswresample])
fi

AC_ARG_ENABLE(flv2mpeg4,
	AS_HELP_STRING(--enable-flv2mpeg4, use flv2mpeg4 libeplayer3-arm),
	,[enable_flv2mpeg4=no])

AM_CONDITIONAL(ENABLE_FLV2MPEG4, test "$enable_flv2mpeg4" = "yes")
if test "$enable_flv2mpeg4" = "yes"; then
	AC_DEFINE(ENABLE_FLV2MPEG4, 1, [use flv2mpeg4 libeplayer3-arm])
fi

AC_OUTPUT([
Makefile
common/Makefile
libmipsbox/Makefile
libarmbox/Makefile
libazbox/Makefile
libduckbox/Makefile
libdvbci/Makefile
libeplayer3-mips/Makefile
libeplayer3-arm/Makefile
libeplayer3-sh4/Makefile
libgeneric-pc/Makefile
libraspi/Makefile
libspark/Makefile
libtriple/Makefile
tools/Makefile
])
