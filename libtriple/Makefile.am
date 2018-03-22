noinst_LTLIBRARIES = libtriple.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include \
	@DIRECTFB_CFLAGS@

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = \
	-lrt \
	@DIRECTFB_LIBS@

libtriple_la_SOURCES = \
	hardware_caps.c \
	lt_dfbinput.cpp \
	dmx_td.cpp \
	video_td.cpp \
	audio_td.cpp \
	init_td.cpp \
	playback_td.cpp \
	pwrmngr.cpp \
	record_td.cpp
