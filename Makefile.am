ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libstb-hal.la
libstb_hal_la_SOURCES =
SUBDIRS = common tools libthread
#bin_PROGRAMS = libstb-hal-test

libstb_hal_la_LIBADD = \
	common/libcommon.la \
	libthread/libthread.la


#libstb_hal_test_SOURCES = libtest.cpp
#libstb_hal_test_LDADD = libstb-hal.la

# there has to be a better way to do this...
if BOXTYPE_TRIPLE
SUBDIRS += libtriple
libstb_hal_la_LIBADD += \
	libtriple/libtriple.la
endif
if BOXTYPE_AZBOX
SUBDIRS += azbox
libstb_hal_la_LIBADD += \
	azbox/libazbox.la
endif
if BOXTYPE_GENERIC
if BOXMODEL_RASPI
SUBDIRS += raspi
libstb_hal_la_LIBADD += \
	raspi/libraspi.la
else
SUBDIRS += generic-pc
libstb_hal_la_LIBADD += \
	generic-pc/libgeneric.la
endif
endif
if BOXTYPE_SPARK
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libspark libeplayer3
libstb_hal_la_LIBADD += \
	libspark/libspark.la \
	libeplayer3/libeplayer3.la
endif
if BOXTYPE_DUCKBOX
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libduckbox libeplayer3 libdvbci
libstb_hal_la_LIBADD += \
	libduckbox/libduckbox.la \
	libeplayer3/libeplayer3.la \
	libdvbci/libdvbci.la
endif
if BOXTYPE_ARMBOX
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

pkginclude_HEADERS = \
	include/audio_hal.h \
	include/ca_cs.h \
	include/ca.h \
	include/cs_api.h \
	include/ca_ci.h \
	include/cs_types.h \
	include/dmx_cs.h \
	include/dmx_hal.h \
	include/glfb.h \
	include/hardware_caps.h \
	include/init_cs.h \
	include/init_td.h \
	include/mmi.h \
	include/playback.h \
	include/playback_hal.h \
	include/pwrmngr.h \
	include/record_hal.h \
	include/video_cs.h \
	include/video_hal.h
