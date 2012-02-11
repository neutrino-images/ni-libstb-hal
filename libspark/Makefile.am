INCLUDES = \
	-I$(top_srcdir)/common \
	@DIRECTFB_CFLAGS@

noinst_LIBRARIES = libspark.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

libspark_a_SOURCES = \
	lt_dfbinput.cpp \
	dmx.cpp \
	ca.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback.cpp \
	pwrmngr.cpp \
	record.cpp
