INCLUDES = \
	-I$(top_srcdir)/common \
	@DIRECTFB_CFLAGS@

noinst_LIBRARIES = libtriple.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

libtriple_a_SOURCES = \
	lt_dfbinput.cpp \
	dmx_td.cpp \
	ca.cpp \
	video_td.cpp \
	audio_td.cpp \
	init_td.cpp \
	playback_td.cpp \
	pwrmngr.cpp \
	record_td.cpp
