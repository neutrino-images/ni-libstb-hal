noinst_LTLIBRARIES = libazbox.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = -lpthread

libazbox_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback.cpp \
	record.cpp

