AC_INIT(libstb-hal,0.1.1)
AM_INIT_AUTOMAKE(libstb-hal,0.1.1)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_MACRO_DIR([m4])

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
AC_DISABLE_STATIC
AC_SYS_LARGEFILE
AM_PROG_LIBTOOL

if test x"$BOXTYPE" = x"tripledragon"; then
	TUXBOX_APPS_LIB_PKGCONFIG(DIRECTFB, directfb)
fi

AC_OUTPUT([
Makefile
common/Makefile
libeplayer3/Makefile
libtriple/Makefile
libspark/Makefile
tools/Makefile
])

