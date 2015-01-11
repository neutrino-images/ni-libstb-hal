noinst_LTLIBRARIES = libcommon.la

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = -lpthread

if BOXTYPE_DUCKBOX
AM_CXXFLAGS += \
	-I $(top_srcdir)/include \
	-I $(top_srcdir)/libdvbci
endif

if BOXTYPE_DUCKBOX
libcommon_la_SOURCES = \
	ca_ci.cpp
else
libcommon_la_SOURCES = \
	ca.cpp
endif

libcommon_la_SOURCES += \
	thread_abstraction.cpp \
	mutex_abstraction.cpp \
	lt_debug.cpp \
	proc_tools.c
