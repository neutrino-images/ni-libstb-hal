INCLUDES = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/libeplayer3/include

noinst_LIBRARIES = libspark.a

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = -lpthread

libspark_a_SOURCES = \
	irmp.c \
	lirmp_input.cpp \
	dmx.cpp \
	ca.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback_libeplayer3.cpp \
	pwrmngr.cpp \
	record.cpp

AM_CPPFLAGS = -DF_INTERRUPTS=20000 -DIRMP_EMBED -DLIRC_IRMP
