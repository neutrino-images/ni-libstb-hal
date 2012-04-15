noinst_LIBRARIES = libstb-hal.a
libstb_hal_a_SOURCES =
SUBDIRS = common tools
bin_PROGRAMS = libstb-hal-test

libstb_hal_a_LIBADD = \
	common/lt_debug.o

libstb_hal_test_SOURCES = libtest.cpp
libstb_hal_test_LDADD = \
	libstb-hal.a \
	-lpthread

if BOXTYPE_TRIPLE
libstb_hal_test_LDADD += \
	@DIRECTFB_LIBS@
endif

# there has to be a better way to do this...
if BOXTYPE_TRIPLE
SUBDIRS += libtriple
libstb_hal_a_LIBADD += \
	libtriple/audio_td.o \
	libtriple/ca.o \
	libtriple/dmx_td.o \
	libtriple/init_td.o \
	libtriple/lt_dfbinput.o \
	libtriple/playback_td.o \
	libtriple/pwrmngr.o \
	libtriple/record_td.o \
	libtriple/video_td.o
endif
if BOXTYPE_SPARK
SUBDIRS += libspark libeplayer3
libstb_hal_a_LIBADD += \
	libspark/audio.o \
	libspark/ca.o \
	libspark/dmx.o \
	libspark/init.o \
	libspark/irmp.o \
	libspark/lirmp_input.o \
	libspark/playback_libeplayer3.o \
	libspark/pwrmngr.o \
	libspark/record.o \
	libspark/video.o

# this is a hack to build the libeplayer3 objs statically into
# libstb-hal...
libstb_hal_a_LIBADD += \
	libeplayer3/container.o \
	libeplayer3/container_ffmpeg.o \
	libeplayer3/text_srt.o \
	libeplayer3/text_ssa.o \
	libeplayer3/container_ass.o \
	libeplayer3/audio.o \
	libeplayer3/manager.o \
	libeplayer3/subtitle.o \
	libeplayer3/video.o \
	libeplayer3/output_subtitle.o \
	libeplayer3/linuxdvb.o \
	libeplayer3/output.o \
	libeplayer3/playback.o \
	libeplayer3/writer.o \
	libeplayer3/aac.o \
	libeplayer3/wmv.o \
	libeplayer3/ac3.o \
	libeplayer3/divx.o \
	libeplayer3/wma.o \
	libeplayer3/pes.o \
	libeplayer3/dts.o \
	libeplayer3/mpeg2.o \
	libeplayer3/mp3.o \
	libeplayer3/misc.o \
	libeplayer3/h264.o \
	libeplayer3/h263.o \
	libeplayer3/vc1.o \
	libeplayer3/framebuffer.o \
	libeplayer3/vorbis.o \
	libeplayer3/flac.o \
	libeplayer3/pcm.o
endif
