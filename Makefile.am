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
if BOXMODEL_MULTIBOXSE
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
else
if BOXMODEL_OSMINI4K
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
else
if BOXMODEL_OSMIO4K
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
else
if BOXMODEL_OSMIO4KPLUS
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
else
#libstb_hal_test_LDADD += -lasound
SUBDIRS += libarmbox libdvbci
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la \
	libdvbci/libdvbci.la
endif
endif
endif
endif
endif
if !ENABLE_GSTREAMER
SUBDIRS += libeplayer3
libstb_hal_la_LIBADD += \
	libeplayer3/libeplayer3.la
endif
endif

if BOXTYPE_MIPSBOX
SUBDIRS += libmipsbox libdvbci
libstb_hal_la_LIBADD += \
	libmipsbox/libmipsbox.la \
	libdvbci/libdvbci.la
SUBDIRS += libeplayer3
libstb_hal_la_LIBADD += \
	libeplayer3/libeplayer3.la
endif
