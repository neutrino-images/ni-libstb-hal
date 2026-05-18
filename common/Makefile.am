VERSION_H = version.h
$(VERSION_H):
	@rm -f $@
	@echo "#define BUILT_DATE \"$$(date)\"" > $@
	@if test -d $(top_srcdir)/.git; then \
		pushd $(top_srcdir); \
		GITTAG=$$(git tag -l "NI-*" | tail -n1); \
		GITDESCRIBE=$$(git describe --always --long --tags --match $$GITTAG | sed 's/-/./2' || echo "VCS failed"); \
		popd; \
		echo "#define VCS \"$${GITDESCRIBE}\"" >> $@; \
	fi

BUILT_SOURCES = $(VERSION_H)
noinst_HEADERS = $(VERSION_H)
.PHONY: $(VERSION_H)

noinst_LTLIBRARIES = libcommon.la

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_CXXFLAGS += \
	-I $(top_srcdir)/include

AM_LDFLAGS = -lpthread

if BOXTYPE_ARMBOX
AM_CXXFLAGS += \
	-I $(top_srcdir)/libdvbci
endif

if BOXTYPE_MIPSBOX
AM_CXXFLAGS += \
	-I $(top_srcdir)/libdvbci
endif

if BOXTYPE_MIPSBOX
libcommon_la_SOURCES = \
	ca_ci.cpp
else
if BOXTYPE_ARMBOX
if BOXMODEL_HD60
libcommon_la_SOURCES = \
	ca.cpp
else
if BOXMODEL_MULTIBOX
libcommon_la_SOURCES = \
	ca.cpp
else
if BOXMODEL_MULTIBOXSE
libcommon_la_SOURCES = \
	ca.cpp
else
if BOXMODEL_OSMIO4K
libcommon_la_SOURCES = \
	ca.cpp
else
if BOXMODEL_OSMIO4KPLUS
libcommon_la_SOURCES = \
	ca.cpp
else
libcommon_la_SOURCES = \
	ca_ci.cpp
endif
endif
endif
endif
endif
else
libcommon_la_SOURCES = \
	ca.cpp
endif
endif

libcommon_la_SOURCES += \
	hal_debug.cpp \
	proc_tools.c \
	pwrmngr.cpp \
	version_hal.cpp
