noinst_LTLIBRARIES = libgeneric.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += \
	-I$(top_srcdir)/common

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	-lglut -lGL -lGLU -lGLEW -lao \
	-lOpenThreads \
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	@AVCODEC_LIBS@ \
	@SWRESAMPLE_LIBS@ \
	@SWSCALE_LIBS@

libgeneric_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	glfb.cpp \
	init.cpp \
	playback.cpp \
	pwrmngr.cpp \
	record.cpp

