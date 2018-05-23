ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(warning It seems you do not have the PSI build environment. Remove GNUmakefile.)
include Makefile
else
include /ioc/tools/driver.makefile
EXCLUDE_VERSIONS = 3.13.2
PROJECT=stream
BUILDCLASSES += Linux

DOCUDIR = documentation

PCRE=1
ASYN=1
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

streamReferences:
	$(PERL) ../src/makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../src/makeref.pl Converter $(FORMATS) >> $@

export DBDFILES = streamSup.dbd
streamSup.dbd:
	@echo Creating $@
	$(PERL) ../src/makedbd.pl $(RECORDTYPES) > $@

endif
