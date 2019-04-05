ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libstb-hal.la
libstb_hal_la_SOURCES =
SUBDIRS = common tools

libstb_hal_la_LIBADD = \
	common/libcommon.la

#bin_PROGRAMS = libstb-hal-test
#libstb_hal_test_SOURCES = libtest.cpp
#libstb_hal_test_LDADD = libstb-hal.la

# there has to be a better way to do this...
if BOXTYPE_TRIPLE
SUBDIRS += libtriple
libstb_hal_la_LIBADD += \
	libtriple/libtriple.la
endif
if BOXTYPE_AZBOX
SUBDIRS += libazbox
libstb_hal_la_LIBADD += \
	libazbox/libazbox.la
endif
if BOXTYPE_GENERIC
if BOXMODEL_RASPI
SUBDIRS += libraspi
libstb_hal_la_LIBADD += \
	libraspi/libraspi.la
else
SUBDIRS += libgeneric-pc
libstb_hal_la_LIBADD += \
	libgeneric-pc/libgeneric.la
endif
endif
if BOXTYPE_SPARK
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libspark libeplayer3-sh4
libstb_hal_la_LIBADD += \
	libspark/libspark.la \
	libeplayer3-sh4/libeplayer3_sh4.la
endif
if BOXTYPE_DUCKBOX
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libduckbox libeplayer3-sh4 libdvbci
libstb_hal_la_LIBADD += \
	libduckbox/libduckbox.la \
	libeplayer3-sh4/libeplayer3_sh4.la \
	libdvbci/libdvbci.la
endif
if BOXTYPE_ARMBOX
if BOXMODEL_HD60
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
else
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libarmbox libdvbci
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la \
	libdvbci/libdvbci.la

if !ENABLE_GSTREAMER_10
SUBDIRS += libeplayer3-arm

libstb_hal_la_LIBADD += \
	libeplayer3-arm/libeplayer3_arm.la
endif
endif
endif
if BOXTYPE_MIPSBOX
SUBDIRS += libmipsbox libdvbci
libstb_hal_la_LIBADD += \
	libmipsbox/libmipsbox.la \
	libdvbci/libdvbci.la
SUBDIRS += libeplayer3-mips
libstb_hal_la_LIBADD += \
	libeplayer3-mips/libeplayer3_mips.la
endif

pkginclude_HEADERS = \
	include/audio_hal.h \
	include/ca.h \
	include/ca_ci.h \
	include/ca_hal.h \
	include/cs_api.h \
	include/cs_types.h \
	include/dmx_hal.h \
	include/glfb.h \
	include/hardware_caps.h \
	include/init.h \
	include/mmi.h \
	include/playback_hal.h \
	include/pwrmngr.h \
	include/record_hal.h \
	include/version_hal.h \
	include/video_hal.h
