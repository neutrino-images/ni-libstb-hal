noinst_LIBRARIES = libstb-hal.a
libstb_hal_a_SOURCES =
SUBDIRS = common
bin_PROGRAMS = libstb-hal-test

libstb_hal_a_LIBADD = \
	common/lt_debug.o

libstb_hal_test_SOURCES = libtest.cpp
libstb_hal_test_LDADD = \
	libstb-hal.a \
	@DIRECTFB_LIBS@

# there has to be a better way to do this...
if BOXTYPE_TRIPLE
SUBDIRS += libtriple
libstb_hal_a_LIBADD += \
	libtriple/audio_td.o \
	libtriple/ca.o \
	libtriple/dmx_td.o \
	libtriple/init_td.o \
	libtriple/lt_dfbinput.o \
	libtriple/playback_td.o \
	libtriple/pwrmngr.o \
	libtriple/record_td.o \
	libtriple/video_td.o
endif
if BOXTYPE_SPARK
SUBDIRS += libspark
libstb_hal_a_LIBADD += \
	libspark/audio.o \
	libspark/ca.o \
	libspark/dmx.o \
	libspark/init.o \
	libspark/irmp.o \
	libspark/lt_dfbinput.o \
	libspark/playback.o \
	libspark/pwrmngr.o \
	libspark/record.o \
	libspark/video.o
endif
