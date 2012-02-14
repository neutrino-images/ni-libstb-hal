noinst_LIBRARIES = libstb-hal.a
libstb_hal_a_SOURCES =
SUBDIRS = common
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
	libeplayer3/.libs/container.o \
	libeplayer3/.libs/container_ffmpeg.o \
	libeplayer3/.libs/text_srt.o \
	libeplayer3/.libs/text_ssa.o \
	libeplayer3/.libs/container_ass.o \
	libeplayer3/.libs/audio.o \
	libeplayer3/.libs/manager.o \
	libeplayer3/.libs/subtitle.o \
	libeplayer3/.libs/video.o \
	libeplayer3/.libs/output_subtitle.o \
	libeplayer3/.libs/linuxdvb.o \
	libeplayer3/.libs/output.o \
	libeplayer3/.libs/playback.o \
	libeplayer3/.libs/writer.o \
	libeplayer3/.libs/aac.o \
	libeplayer3/.libs/wmv.o \
	libeplayer3/.libs/ac3.o \
	libeplayer3/.libs/divx.o \
	libeplayer3/.libs/wma.o \
	libeplayer3/.libs/pes.o \
	libeplayer3/.libs/dts.o \
	libeplayer3/.libs/mpeg2.o \
	libeplayer3/.libs/mp3.o \
	libeplayer3/.libs/misc.o \
	libeplayer3/.libs/h264.o \
	libeplayer3/.libs/h263.o \
	libeplayer3/.libs/vc1.o \
	libeplayer3/.libs/framebuffer.o \
	libeplayer3/.libs/vorbis.o \
	libeplayer3/.libs/flac.o \
	libeplayer3/.libs/pcm.o
endif
