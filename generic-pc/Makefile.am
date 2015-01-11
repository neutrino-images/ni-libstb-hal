noinst_LTLIBRARIES = libgeneric.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += \
	-I$(top_srcdir)/common

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	-lglut -lGL -lGLU -lGLEW -lao \
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
	pwrmngr.cpp \
	record.cpp

if ENABLE_GSTREAMER
libgeneric_la_SOURCES += \
	playback_gst.cpp
AM_LDFLAGS += \
	-lgstreamer-0.10 \
	-lgstinterfaces-0.10
else
libgeneric_la_SOURCES += \
	playback.cpp
endif

