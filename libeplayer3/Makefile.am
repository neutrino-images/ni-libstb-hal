noinst_LTLIBRARIES = libeplayer3.la

CXXFLAGS = -Wall

AM_CPPFLAGS = \
	-I$(srcdir)/include

libeplayer3_la_SOURCES = \
	container/container.c container/container_ffmpeg.c \
	manager/audio.c manager/manager.c manager/subtitle.c manager/video.c \
	manager/teletext.c manager/chapter.c \
	output/linuxdvb.c output/output.c \
	playback/playback.c output/writer/writer.c output/writer/wmv.c \
	output/writer/ac3.c output/writer/divx.c output/writer/pes.c \
	output/writer/dts.c output/writer/mpeg2.c output/writer/mp3.c output/writer/misc.c \
	output/writer/h264.c output/writer/h263.c output/writer/vc1.c \
	output/writer/flac.c output/writer/pcm.c

AM_CFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

LIBEPLAYER3_LIBS = libeplayer3.la -lpthread -lavformat -lavcodec -lavutil -lswresample -lm

