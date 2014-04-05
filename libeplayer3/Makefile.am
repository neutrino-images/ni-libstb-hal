noinst_LTLIBRARIES = libeplayer3.la

CXXFLAGS = -Wall

AM_CPPFLAGS = -I$(srcdir)/include
AM_CPPFLAGS += -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
AM_CPPFLAGS += -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS
AM_CPPFLAGS += -ggdb

libeplayer3_la_SOURCES = \
	container/container.cpp container/container_ffmpeg.cpp \
	manager/audio.cpp manager/manager.cpp manager/subtitle.cpp manager/video.cpp \
	manager/teletext.cpp manager/chapter.cpp \
	output/linuxdvb.cpp output/output.cpp \
	playback/playback.cpp output/writer/writer.cpp output/writer/wmv.cpp \
	output/writer/ac3.cpp output/writer/divx.cpp output/writer/pes.cpp \
	output/writer/dts.cpp output/writer/mpeg2.cpp output/writer/mp3.cpp output/writer/misc.cpp \
	output/writer/h264.cpp output/writer/h263.cpp output/writer/vc1.cpp \
	output/writer/flac.cpp output/writer/pcm.cpp

LIBEPLAYER3_LIBS = libeplayer3.la -lpthread -lavformat -lavcodec -lavutil -lswresample -lm

