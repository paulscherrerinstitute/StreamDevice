ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(info If you are not using the PSI build environment, GNUmakefile can be removed.)
include Makefile
else
include /ioc/tools/driver.makefile
EXCLUDE_VERSIONS = 3.13.2
PROJECT=stream
BUILDCLASSES += Linux

DOCUDIR = documentation

ifdef EPICSVERSION
ifndef RECORDTYPES
PCRE=1
ASYN=1
include src/CONFIG_STREAM
export RECORDTYPES BUSSES FORMATS
endif
endif

SOURCES += $(RECORDTYPES:%=src/dev%Stream.c)
SOURCES += $(FORMATS:%=src/%Converter.cc)
SOURCES += $(BUSSES:%=src/%Interface.cc)
SOURCES += $(STREAM_SRCS:%=src/%)

HEADERS += devStream.h
HEADERS += StreamFormat.h
HEADERS += StreamFormatConverter.h
HEADERS += StreamBuffer.h
HEADERS += StreamError.h

StreamCore.o StreamCore.d: streamReferences

# Update version string (contains __DATE__ and __TIME__)
# each time make runs.
StreamVersion.o: FORCE
FORCE:

streamReferences:
	$(PERL) ../src/makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../src/makeref.pl Converter $(FORMATS) >> $@

export DBDFILES = streamSup.dbd
streamSup.dbd:
	@echo Creating $@ from $(RECORDTYPES)
	$(PERL) ../src/makedbd.pl $(RECORDTYPES) > $@

endif
