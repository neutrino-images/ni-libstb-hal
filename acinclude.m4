AC_DEFUN([TUXBOX_APPS], [
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_GNU_SOURCE

AC_ARG_WITH(target,
	AS_HELP_STRING([--with-target=TARGET], [target for compilation [[native,cdk]]]),
	[TARGET="$withval"],
	[TARGET="native"])

AC_ARG_WITH(targetprefix,
	AS_HELP_STRING([--with-targetprefix=PATH], [prefix relative to target root (only applicable in cdk mode)]),
	[TARGET_PREFIX="$withval"],
	[TARGET_PREFIX=""])

AC_ARG_WITH(debug,
	AS_HELP_STRING([--without-debug], [disable debugging code @<:@default=no@:>@]),
	[DEBUG="$withval"],
	[DEBUG="yes"])

if test "$DEBUG" = "yes"; then
	DEBUG_CFLAGS="-g3 -ggdb"
	AC_DEFINE(DEBUG, 1, [enable debugging code])
fi

AC_MSG_CHECKING(target)

if test "$TARGET" = "native"; then
	AC_MSG_RESULT(native)

	if test "$CFLAGS" = "" -a "$CXXFLAGS" = ""; then
		CFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
		CXXFLAGS="-Wall -O2 -pipe $DEBUG_CFLAGS"
	fi
	if test "$prefix" = "NONE"; then
		prefix=/usr/local
	fi
	targetprefix=$prefix
	TARGET_PREFIX=$prefix
	AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX", [The targets prefix])
elif test "$TARGET" = "cdk"; then
	AC_MSG_RESULT(cdk)

	if test "$CC" = "" -a "$CXX" = ""; then
		AC_MSG_ERROR([you need to specify variables CC or CXX in cdk])
	fi
	if test "$CFLAGS" = "" -o "$CXXFLAGS" = ""; then
		AC_MSG_ERROR([you need to specify variables CFLAGS and CXXFLAGS in cdk])
	fi
	if test "$prefix" = "NONE"; then
		AC_MSG_ERROR([invalid prefix, you need to specify one in cdk mode])
	fi
	if test "$TARGET_PREFIX" != "NONE"; then
		AC_DEFINE_UNQUOTED(TARGET_PREFIX, "$TARGET_PREFIX", [The targets prefix])
	fi
	if test "$TARGET_PREFIX" = "NONE"; then
		AC_MSG_ERROR([invalid targetprefix, you need to specify one in cdk mode])
		TARGET_PREFIX=""
	fi
else
	AC_MSG_RESULT(none)
	AC_MSG_ERROR([invalid target $TARGET, choose on from native,cdk]);
fi

if test "$exec_prefix" = "NONE"; then
	exec_prefix=$prefix
fi

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_SYS_LARGEFILE

])

AC_DEFUN([TUXBOX_BOXTYPE], [
AC_ARG_WITH(boxtype,
	AS_HELP_STRING([--with-boxtype], [valid values: generic, armbox, mipsbox]),
	[case "${withval}" in
		generic|armbox|mipsbox)
			BOXTYPE="$withval"
		;;
		*)
			AC_MSG_ERROR([bad value $withval for --with-boxtype])
		;;
	esac],
	[BOXTYPE="generic"])

AC_ARG_WITH(boxmodel,
	AS_HELP_STRING([--with-boxmodel], [valid for generic: generic, raspi])
AS_HELP_STRING([], [valid for armbox: hd60, hd61, multiboxse, hd51, bre2ze4k, h7, osmini4k, osmio4k, osmio4kplus, vusolo4k, vuduo4k, vuduo4kse, vuultimo4k, vuuno4k, vuuno4kse, vuzero4k])
AS_HELP_STRING([], [valid for mipsbox: vuduo, vuduo2, gb800se, osnino, osninoplus, osninopro]),
	[case "${withval}" in
		generic|raspi)
			if test "$BOXTYPE" = "generic"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		hd60|hd61|multiboxse|hd51|bre2ze4k|h7|osmini4k|osmio4k|osmio4kplus|vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k)
			if test "$BOXTYPE" = "armbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		vuduo|vuduo2|gb800se|osnino|osninoplus|osninopro)
			if test "$BOXTYPE" = "mipsbox"; then
				BOXMODEL="$withval"
			else
				AC_MSG_ERROR([unknown model $withval for boxtype $BOXTYPE])
			fi
		;;
		*)
			AC_MSG_ERROR([unsupported value $withval for --with-boxmodel])
		;;
	esac],
	[BOXMODEL="generic"])

AC_SUBST(BOXTYPE)
AC_SUBST(BOXMODEL)

AM_CONDITIONAL(BOXTYPE_GENERIC, test "$BOXTYPE" = "generic")
AM_CONDITIONAL(BOXTYPE_ARMBOX, test "$BOXTYPE" = "armbox")
AM_CONDITIONAL(BOXTYPE_MIPSBOX, test "$BOXTYPE" = "mipsbox")

# generic
AM_CONDITIONAL(BOXMODEL_GENERIC, test "$BOXMODEL" = "generic")
AM_CONDITIONAL(BOXMODEL_RASPI, test "$BOXMODEL" = "raspi")

# armbox
AM_CONDITIONAL(BOXMODEL_HD60, test "$BOXMODEL" = "hd60")
AM_CONDITIONAL(BOXMODEL_HD61, test "$BOXMODEL" = "hd61")
AM_CONDITIONAL(BOXMODEL_MULTIBOXSE, test "$BOXMODEL" = "multiboxse")

AM_CONDITIONAL(BOXMODEL_HD51, test "$BOXMODEL" = "hd51")
AM_CONDITIONAL(BOXMODEL_BRE2ZE4K, test "$BOXMODEL" = "bre2ze4k")
AM_CONDITIONAL(BOXMODEL_H7, test "$BOXMODEL" = "h7")

AM_CONDITIONAL(BOXMODEL_OSMINI4K, test "$BOXMODEL" = "osmini4k")
AM_CONDITIONAL(BOXMODEL_OSMIO4K, test "$BOXMODEL" = "osmio4k")
AM_CONDITIONAL(BOXMODEL_OSMIO4KPLUS, test "$BOXMODEL" = "osmio4kplus")

AM_CONDITIONAL(BOXMODEL_VUSOLO4K, test "$BOXMODEL" = "vusolo4k")
AM_CONDITIONAL(BOXMODEL_VUDUO4K, test "$BOXMODEL" = "vuduo4k")
AM_CONDITIONAL(BOXMODEL_VUDUO4KSE, test "$BOXMODEL" = "vuduo4kse")
AM_CONDITIONAL(BOXMODEL_VUULTIMO4K, test "$BOXMODEL" = "vuultimo4k")
AM_CONDITIONAL(BOXMODEL_VUUNO4K, test "$BOXMODEL" = "vuuno4k")
AM_CONDITIONAL(BOXMODEL_VUUNO4KSE, test "$BOXMODEL" = "vuuno4kse")
AM_CONDITIONAL(BOXMODEL_VUZERO4K, test "$BOXMODEL" = "vuzero4k")

# mipsbox
AM_CONDITIONAL(BOXMODEL_VUDUO, test "$BOXMODEL" = "vuduo")
AM_CONDITIONAL(BOXMODEL_VUDUO2, test "$BOXMODEL" = "vuduo2")

AM_CONDITIONAL(BOXMODEL_GB800SE, test "$BOXMODEL" = "gb800se")

AM_CONDITIONAL(BOXMODEL_OSNINO, test "$BOXMODEL" = "osnino")
AM_CONDITIONAL(BOXMODEL_OSNINOPLUS, test "$BOXMODEL" = "osninoplus")
AM_CONDITIONAL(BOXMODEL_OSNINOPRO, test "$BOXMODEL" = "osninopro")

if test "$BOXTYPE" = "generic"; then
	AC_DEFINE(HAVE_GENERIC_HARDWARE, 1, [building for a generic device like a standard PC])
elif test "$BOXTYPE" = "armbox"; then
	AC_DEFINE(HAVE_ARM_HARDWARE, 1, [building for an armbox])
elif test "$BOXTYPE" = "mipsbox"; then
	AC_DEFINE(HAVE_MIPS_HARDWARE, 1, [building for an mipsbox])
fi

# generic
if test "$BOXMODEL" = "generic"; then
	AC_DEFINE(BOXMODEL_GENERIC, 1, [generic pc])
elif test "$BOXMODEL" = "raspi"; then
	AC_DEFINE(BOXMODEL_RASPI, 1, [raspberry pi])

# armbox
elif test "$BOXMODEL" = "hd60"; then
	AC_DEFINE(BOXMODEL_HD60, 1, [hd60])
elif test "$BOXMODEL" = "hd61"; then
	AC_DEFINE(BOXMODEL_HD61, 1, [hd61])
elif test "$BOXMODEL" = "multiboxse"; then
	AC_DEFINE(BOXMODEL_MULTIBOXSE, 1, [multiboxse])

elif test "$BOXMODEL" = "hd51"; then
	AC_DEFINE(BOXMODEL_HD51, 1, [hd51])
elif test "$BOXMODEL" = "bre2ze4k"; then
	AC_DEFINE(BOXMODEL_BRE2ZE4K, 1, [bre2ze4k])
elif test "$BOXMODEL" = "h7"; then
	AC_DEFINE(BOXMODEL_H7, 1, [h7])

elif test "$BOXMODEL" = "osmini4k"; then
	AC_DEFINE(BOXMODEL_OSMINI4K, 1, [osmini4k])
elif test "$BOXMODEL" = "osmio4k"; then
	AC_DEFINE(BOXMODEL_OSMIO4K, 1, [osmio4k])
elif test "$BOXMODEL" = "osmio4kplus"; then
	AC_DEFINE(BOXMODEL_OSMIO4KPLUS, 1, [osmio4kplus])

elif test "$BOXMODEL" = "vusolo4k"; then
	AC_DEFINE(BOXMODEL_VUSOLO4K, 1, [vusolo4k])
elif test "$BOXMODEL" = "vuduo4k"; then
	AC_DEFINE(BOXMODEL_VUDUO4K, 1, [vuduo4k])
elif test "$BOXMODEL" = "vuduo4kse"; then
	AC_DEFINE(BOXMODEL_VUDUO4KSE, 1, [vuduo4kse])
elif test "$BOXMODEL" = "vuultimo4k"; then
	AC_DEFINE(BOXMODEL_VUULTIMO4K, 1, [vuultimo4k])
elif test "$BOXMODEL" = "vuuno4k"; then
	AC_DEFINE(BOXMODEL_VUUNO4K, 1, [vuuno4k])
elif test "$BOXMODEL" = "vuuno4kse"; then
	AC_DEFINE(BOXMODEL_VUUNO4KSE, 1, [vuuno4kse])
elif test "$BOXMODEL" = "vuzero4k"; then
	AC_DEFINE(BOXMODEL_VUZERO4K, 1, [vuzero4k])

# mipsbox
elif test "$BOXMODEL" = "vuduo"; then
	AC_DEFINE(BOXMODEL_VUDUO, 1, [vuduo])
elif test "$BOXMODEL" = "vuduo2"; then
	AC_DEFINE(BOXMODEL_VUDUO2, 1, [vuduo2])

elif test "$BOXMODEL" = "gb800se"; then
	AC_DEFINE(BOXMODEL_GB800SE, 1, [gb800se])

elif test "$BOXMODEL" = "osnino"; then
	AC_DEFINE(BOXMODEL_OSNINO, 1, [osnino])
elif test "$BOXMODEL" = "osninoplus"; then
	AC_DEFINE(BOXMODEL_OSNINOPLUS, 1, [osninoplus])
elif test "$BOXMODEL" = "osninopro"; then
	AC_DEFINE(BOXMODEL_OSNINOPRO, 1, [osninopro])
fi

# all vuplus BOXMODELs
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k|vuduo|vuduo2)
		AC_DEFINE(BOXMODEL_VUPLUS_ALL, 1, [vuplus_all])
		vuplus_all=true
	;;
	*)
		vuplus_all=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_ALL, test "$vuplus_all" = "true")

# all vuplus arm BOXMODELs
case "$BOXMODEL" in
	vusolo4k|vuduo4k|vuduo4kse|vuultimo4k|vuuno4k|vuuno4kse|vuzero4k)
		AC_DEFINE(BOXMODEL_VUPLUS_ARM, 1, [vuplus_arm])
		vuplus_arm=true
	;;
	*)
		vuplus_arm=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_ARM, test "$vuplus_arm" = "true")

# all vuplus mips BOXMODELs
case "$BOXMODEL" in
	vuduo|vuduo2)
		AC_DEFINE(BOXMODEL_VUPLUS_MIPS, 1, [vuplus_mips])
		vuplus_mips=true
	;;
	*)
		vuplus_mips=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_VUPLUS_MIPS, test "$vuplus_mips" = "true")

# all hisilicon BOXMODELs
case "$BOXMODEL" in
	hd60|hd61|multiboxse)
		AC_DEFINE(BOXMODEL_HISILICON, 1, [hisilicon])
		hisilicon=true
	;;
	*)
		hisilicon=false
	;;
esac
AM_CONDITIONAL(BOXMODEL_HISILICON, test "$hisilicon" = "true")
])

dnl backward compatiblity
AC_DEFUN([AC_GNU_SOURCE], [
AH_VERBATIM([_GNU_SOURCE], [
/* Enable GNU extensions on systems that have them. */
#ifndef _GNU_SOURCE
# undef _GNU_SOURCE
#endif
])dnl
AC_BEFORE([$0], [AC_COMPILE_IFELSE])dnl
AC_BEFORE([$0], [AC_RUN_IFELSE])dnl
AC_DEFINE([_GNU_SOURCE])
])

AC_DEFUN([AC_PROG_EGREP], [
AC_CACHE_CHECK([for egrep], [ac_cv_prog_egrep], [
if echo a | (grep -E '(a|b)') >/dev/null 2>&1; then
	ac_cv_prog_egrep='grep -E'
else
	ac_cv_prog_egrep='egrep'
fi
])
EGREP=$ac_cv_prog_egrep
AC_SUBST([EGREP])
])
