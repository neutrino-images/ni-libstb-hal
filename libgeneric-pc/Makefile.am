noinst_LTLIBRARIES = libgeneric.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += -Wfatal-errors
AM_CPPFLAGS += \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include \
	@AVUTIL_CFLAGS@ \
	@CLUTTER_CFLAGS@

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	-lao -lrt \
	-lOpenThreads \
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	@AVCODEC_LIBS@ \
	@SWRESAMPLE_LIBS@ \
	@SWSCALE_LIBS@ \
	@CLUTTER_LIBS@

if USE_OPENGL
AM_LDFLAGS += -lglut -lGL -lGLU -lGLEW -lao
endif

libgeneric_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	record.cpp

if USE_CLUTTER
libgeneric_la_SOURCES += clutterfb.cpp
endif
if USE_OPENGL
libgeneric_la_SOURCES += glfb.cpp
endif

if ENABLE_GSTREAMER
libgeneric_la_SOURCES += \
	playback_gst.cpp

AM_LDFLAGS += \
	-lgstreamer-1.0 \
	-lgsttag-1.0 \
	-lgstmpegts-1.0
else
libgeneric_la_SOURCES += \
	playback.cpp
endif
