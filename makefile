first: stream.dbd build
include /ioc/tools/driver.makefile
EXCLUDE_VERSIONS = 3.13.2
PROJECT=stream2
BUILDCLASSES += Linux

#DOCUDIR = doc

DBDS = stream.dbd

BUSSES  += AsynDriver
BUSSES  += Dummy

FORMATS += Enum
FORMATS += BCD
FORMATS += Raw
FORMATS += RawFloat
FORMATS += Binary
FORMATS += Checksum
FORMATS += Regexp
FORMATS += MantissaExponent
FORMATS += Timestamp

RECORDTYPES += aai aao
RECORDTYPES += ao ai
RECORDTYPES += bo bi
RECORDTYPES += mbbo mbbi
RECORDTYPES += mbboDirect mbbiDirect
RECORDTYPES += longout longin
RECORDTYPES += stringout stringin
RECORDTYPES += waveform

SOURCES += $(RECORDTYPES:%=src/dev%Stream.c)
SOURCES += $(FORMATS:%=src/%Converter.cc)
SOURCES += $(BUSSES:%=src/%Interface.cc)
SOURCES += $(wildcard src/Stream*.cc)
SOURCES += src/StreamVersion.c
SOURCES_3.14 += src/devcalcoutStream.c

HEADERS += StreamFormat.h
HEADERS += StreamFormatConverter.h
HEADERS += StreamBuffer.h
HEADERS += StreamError.h

ifeq (${EPICS_BASETYPE},3.13)
USR_INCLUDES += -include $(INSTALL_INCLUDE)/compat3_13.h
endif

StreamCore.o: streamReferences

streamReferences:
	perl ../src/makeref.pl Interface $(BUSSES) > $@
	perl ../src/makeref.pl Converter $(FORMATS) >> $@

stream.dbd:
	perl src/makedbd.pl $(RECORDTYPES) > $@

clean::
	rm -rf stream.dbd

