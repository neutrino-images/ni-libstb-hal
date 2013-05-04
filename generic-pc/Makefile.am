INCLUDES = \
	-I$(top_srcdir)/common

noinst_LTLIBRARIES = libgeneric.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

libgeneric_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback.cpp \
	pwrmngr.cpp \
	record.cpp

