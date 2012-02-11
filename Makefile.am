noinst_LIBRARIES = libstb-hal.a
libneutrino_hal_a_SOURCES =
SUBDIRS =

# there has to be a better way to do this...
if BOXTYPE_TRIPLE
SUBDIRS += libtriple
libstb_hal_a_LIBADD = \
	libtriple/audio_td.o \
	libtriple/ca.o \
	libtriple/dmx_td.o \
	libtriple/init_td.o \
	libtriple/lt_debug.o \
	libtriple/lt_dfbinput.o \
	libtriple/playback_td.o \
	libtriple/pwrmngr.o \
	libtriple/record_td.o \
	libtriple/video_td.o
endif
if BOXTYPE_SPARK
SUBDIRS += libspark
endif
