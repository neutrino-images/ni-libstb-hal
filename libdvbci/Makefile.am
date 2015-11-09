noinst_LTLIBRARIES = libdvbci.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = -lpthread -lcrypto -lssl

libdvbci_la_SOURCES = \
	dvbci_session.cpp \
	dvbci_mmi.cpp \
	dvbci_camgr.cpp \
	misc.cpp \
	descrambler.cpp \
	dh_rsa_misc.cpp \
	aes_xcbc_mac.cpp \
	dvbci_ccmgr.cpp \
	dvbci_appmgr.cpp \
	dvbci_datetimemgr.cpp \
	dvbci_resmgr.cpp

