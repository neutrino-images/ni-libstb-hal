lib_LTLIBRARIES = libeplayer3.la

CXXFLAGS = -Wall

INCLUDES = \
	-I$(srcdir)/include

libeplayer3_la_SOURCES =  \
	container/container.c container/container_ffmpeg.c container/text_srt.c \
    container/text_ssa.c container/container_ass.c \
	manager/audio.c manager/manager.c manager/subtitle.c manager/video.c \
	output/output_subtitle.c output/linuxdvb.c output/output.c \
	playback/playback.c output/writer/writer.c output/writer/aac.c output/writer/wmv.c \
	output/writer/ac3.c output/writer/divx.c output/writer/wma.c output/writer/pes.c \
	output/writer/dts.c output/writer/mpeg2.c output/writer/mp3.c output/writer/misc.c \
	output/writer/h264.c output/writer/h263.c output/writer/vc1.c output/writer/framebuffer.c \
	output/writer/vorbis.c output/writer/flac.c output/writer/pcm.c

AM_CFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
-D_LARGEFILE64_SOURCE

libeplayer3_la_LIBADD = -lpthread -lavformat -lavcodec -lavutil -lz -lass -lm -lpng

bin_PROGRAMS = eplayer3 meta
eplayer3_SOURCES = tools/eplayer2.c
eplayer3_LDADD =  -leplayer3 -lpthread -lass -lm -lpng

meta_SOURCES = tools/meta.c
meta_LDADD =  -leplayer3 -lpthread -lass -lm -lpng
