noinst_LTLIBRARIES = libspark.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libeplayer3/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = -lpthread -lasound -lass -lrt\
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	@AVCODEC_LIBS@ \
	@SWRESAMPLE_LIBS@

libspark_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	audio_mixer.cpp \
	init.cpp \
	playback_libeplayer3.cpp \
	pwrmngr.cpp \
	record.cpp

#AM_CPPFLAGS = -DF_INTERRUPTS=20000 -DIRMP_EMBED -DLIRC_IRMP
