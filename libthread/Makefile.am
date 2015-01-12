noinst_LTLIBRARIES = libthread.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += \
	-I$(top_srcdir)/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = -lpthread -lrt

libthread_la_SOURCES = \
	condition_abstraction.cpp \
	reentrant_mutex.cpp \
	scoped_lock.cpp \
	thread_abstraction.cpp \
	mutex_abstraction.cpp
