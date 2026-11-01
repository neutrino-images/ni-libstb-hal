# Regenerated only by "make" / "make all". Other goals ("make install", and
# the all-am pass automake runs after compiling) keep an existing one, so
# nothing that includes it is rebuilt. Not .PHONY: make would always
# rebuild its dependents.
VERSION_H = version.h
$(VERSION_H): FORCE
	@case "$(MAKECMDGOALS)" in ""|all) ;; *) test -f $@ && exit 0;; esac; \
	echo "#define BUILT_DATE \"$$(date)\"" > $@; \
	if test -d $(top_srcdir)/.git; then \
		pushd $(top_srcdir); \
		GITTAG=$$(git tag -l "NI-*" --sort=version:refname | tail -n1); \
		GITDESCRIBE=$$(git describe --always --long --tags --match $$GITTAG | sed 's/-/./2' || echo "VCS failed"); \
		popd; \
		echo "#define VCS \"$${GITDESCRIBE}\"" >> $@; \
	fi

BUILT_SOURCES = $(VERSION_H)
noinst_HEADERS = $(VERSION_H)

FORCE:

noinst_LTLIBRARIES = libcommon.la

# The include path lives in AM_CPPFLAGS so that C sources see it too
# (AM_CXXFLAGS never reaches the C compiler).
AM_CPPFLAGS = \
	-I $(top_srcdir)/include

AM_CFLAGS = -Wall

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

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
	streaminput.c \
	version_hal.cpp
