include /ioc/tools/driver.makefile
EXCLUDE_VERSIONS = 3.13.2
PROJECT=stream
BUILDCLASSES += Linux

#DOCUDIR = doc

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

HEADERS += devStream.h
HEADERS += StreamFormat.h
HEADERS += StreamFormatConverter.h
HEADERS += StreamBuffer.h
HEADERS += StreamError.h

ifeq (${EPICS_BASETYPE},3.13)
# old gcc needs full path for -include
CXXFLAGS += -include $(foreach d,${INCLUDES:-I%=%},$(wildcard $d/compat3_13.h))
else
RECORDTYPES += calcout
endif

StreamCore.o: streamReferences

streamReferences:
	perl ../src/makeref.pl Interface $(BUSSES) > $@
	perl ../src/makeref.pl Converter $(FORMATS) >> $@

# have to hack a bit to work with both versions of driver.makefile
DBDFILES = O.$${EPICSVERSION}_$${T_A}/streamSup.dbd
../O.${EPICSVERSION}_${T_A}/streamSup.dbd:
	perl ../src/makedbd.pl $(RECORDTYPES) > $@
