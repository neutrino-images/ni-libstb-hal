AUTOMAKE_OPTIONS = subdir-objects

noinst_LTLIBRARIES = libeplayer3_arm.la

AM_CFLAGS    = -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

AM_CPPFLAGS  = -I$(srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -I$(srcdir)/external

AM_CXXFLAGS  = -fno-rtti -fno-exceptions -fno-strict-aliasing

libeplayer3_arm_la_SOURCES = \
	container/container.c \
	container/container_ffmpeg.c \
	manager/manager.c \
	manager/audio.c \
	manager/video.c \
	manager/subtitle.c \
	manager/chapter.c \
	output/linuxdvb_mipsel.c \
	output/output_subtitle.c \
	output/output.c \
	output/linuxdvb_buffering.c \
	output/writer/common/pes.c \
	output/writer/common/misc.c \
	output/writer/mipsel/writer.c \
	output/writer/mipsel/aac.c \
	output/writer/mipsel/ac3.c \
	output/writer/mipsel/mp3.c \
	output/writer/mipsel/pcm.c \
	output/writer/mipsel/lpcm.c \
	output/writer/mipsel/dts.c \
	output/writer/mipsel/amr.c \
	output/writer/mipsel/wma.c \
	output/writer/mipsel/h265.c \
	output/writer/mipsel/h264.c \
	output/writer/mipsel/h263.c \
	output/writer/mipsel/mpeg2.c \
	output/writer/mipsel/mpeg4.c \
	output/writer/mipsel/divx3.c \
	output/writer/mipsel/vp.c \
	output/writer/mipsel/wmv.c \
	output/writer/mipsel/vc1.c \
	playback/playback.c \
	external/ffmpeg/src/bitstream.c \
	external/ffmpeg/src/latmenc.c \
	external/ffmpeg/src/mpeg4audio.c


LIBEPLAYER3_LIBS = libeplayer3_arm.la -lpthread -lavformat -lavcodec -lavutil -lswresample -lz -lm

bin_PROGRAMS = eplayer3
eplayer3_SOURCES = main/exteplayer.c
eplayer3_LDADD = $(LIBEPLAYER3_LIBS)
