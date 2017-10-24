ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libstb-hal.la
libstb_hal_la_SOURCES =
SUBDIRS = common tools libthread
bin_PROGRAMS = libstb-hal-test

libstb_hal_la_LIBADD = \
	common/libcommon.la \
	libthread/libthread.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/include

libstb_hal_test_SOURCES = libtest.cpp
libstb_hal_test_LDADD = libstb-hal.la

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
libstb_hal_test_LDADD += -lasound
SUBDIRS += libspark libeplayer3
libstb_hal_la_LIBADD += \
	libspark/libspark.la \
	libeplayer3/libeplayer3.la
endif
if BOXTYPE_DUCKBOX
libstb_hal_test_LDADD += -lasound
SUBDIRS += libduckbox libeplayer3 libdvbci
libstb_hal_la_LIBADD += \
	libduckbox/libduckbox.la \
	libeplayer3/libeplayer3.la \
	libdvbci/libdvbci.la
endif
if BOXTYPE_ARMBOX
libstb_hal_test_LDADD += -lasound
SUBDIRS += libarmbox
libstb_hal_la_LIBADD += \
	libarmbox/libarmbox.la
endif
