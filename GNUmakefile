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

HEADERS += src/devStream.h
HEADERS += src/StreamFormat.h
HEADERS += src/StreamFormatConverter.h
HEADERS += src/StreamBuffer.h
HEADERS += src/StreamError.h
HEADERS += src/StreamVersion.h

CPPFLAGS += -DSTREAM_INTERNAL

# Update version string (contains __DATE__ and __TIME__)
# each time anything changed.
StreamVersion$(OBJ): StreamVersion.h $(filter-out StreamVersion$(OBJ) stream_exportAddress$(OBJ),$(LIBOBJS) $(LIBRARY_OBJS))

MAKE_FIRST=src/StreamVersion.h
src/StreamVersion.h: $(SOURCES) $(filter-out %StreamVersion.h, $(HEADERS))
	@echo Creating $@ from git tag
	$(PERL) src/makeStreamVersion.pl > $@

StreamCore$(OBJ) StreamCore$(DEP): streamReferences
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
