ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(info If you are not using the PSI build environment, GNUmakefile can be removed.)
include Makefile
else
include /ioc/tools/driver.makefile
EXCLUDE_VERSIONS = 3.13.2
PROJECT=stream
BUILDCLASSES += Linux

DOCUDIR = docs

PCRE=1
ASYN=1
-include ../src/CONFIG_STREAM
-include src/CONFIG_STREAM

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
# each time anything changes.
StreamVersion.o: $(filter-out StreamVersion.o stream_exportAddress.o,$(LIBOBJS))

streamReferences:
	$(PERL) ../src/makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../src/makeref.pl Converter $(FORMATS) >> $@

export DBDFILES = streamSup.dbd
streamSup.dbd:
	@echo Creating $@ from $(RECORDTYPES)
	$(PERL) ../src/makedbd.pl $(RECORDTYPES) > $@
ifdef BASE_3_14
ifdef ASYN
	echo "registrar(AsynDriverInterfaceRegistrar)" >> $@
endif
endif

endif
