noinst_LTLIBRARIES = libraspi.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += \
	-I/opt/vc/include \
	-I/opt/vc/include/interface/vcos/pthreads/ \
	-I/opt/vc/include/interface/vmcs_host/linux \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	-L/opt/vc/lib/ -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt \
	-lOpenThreads

libraspi_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	glfb.cpp \
	init.cpp \
	playback.cpp \
	record.cpp
