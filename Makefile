include /ioc/tools/driver.makefile
#EXCLUDE_VERSIONS = 3.13.2
EXCLUDE_VERSIONS = 3.13
PROJECT=stream

DOCUDIR = doc
SOURCES += $(wildcard src/*.c)
SOURCES += $(wildcard src/*.cc)

DBDS = stream.dbd

BUSSES  += AsynDriver
FORMATS += Enum
FORMATS += BCD
FORMATS += Raw
FORMATS += Binary
FORMATS += Checksum
RECORDTYPES += aai aao
RECORDTYPES += ao ai
RECORDTYPES += bo bi
RECORDTYPES += mbbo mbbi
RECORDTYPES += mbboDirect mbbiDirect
RECORDTYPES += longout longin
RECORDTYPES += stringout stringin
RECORDTYPES += waveform
RECORDTYPES += calcout

StreamCore.o: streamReferences

streamReferences:
	@for i in $(BUSSES); \
	do echo "extern void* ref_$${i}Interface;"; \
           echo "void* p$$i = ref_$${i}Interface;"; \
	done > $@
	@for i in $(FORMATS); \
	do echo "extern void* ref_$${i}Converter;"; \
           echo "void* p$$i = ref_$${i}Converter;"; \
	done >> $@

stream.dbd:
	@for r in $(RECORDTYPES); \
	do echo "device($$r,INST_IO,dev$${r}Stream,\"stream\")"; \
	done > $@
	@echo "driver(stream)" >> $@
