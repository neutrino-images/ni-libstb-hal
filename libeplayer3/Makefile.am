noinst_LTLIBRARIES = libeplayer3.la

AM_CPPFLAGS = -I$(srcdir)/include
AM_CPPFLAGS += -Wall
AM_CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
AM_CPPFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
AM_CPPFLAGS += -fno-rtti
#AM_CPPFLAGS += -ggdb

libeplayer3_la_SOURCES = \
	input.cpp output.cpp manager.cpp player.cpp \
	writer/writer.cpp writer/wmv.cpp writer/ac3.cpp writer/divx.cpp writer/pes.cpp \
	writer/dts.cpp writer/mpeg2.cpp writer/mp3.cpp writer/misc.cpp writer/h264.cpp \
	writer/h263.cpp writer/vc1.cpp writer/flac.cpp writer/pcm.cpp

LIBEPLAYER3_LIBS = libeplayer3.la -lpthread -lavformat -lavcodec -lavutil -lswresample -lm

