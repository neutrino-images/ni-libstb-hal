INCLUDES = \
	-I$(top_srcdir)/common

# this library is not used for linking, so call it libdummy...
noinst_LIBRARIES = libdummy.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = -lpthread

libdummy_a_SOURCES = \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback.cpp \
	pwrmngr.cpp \
	record.cpp

