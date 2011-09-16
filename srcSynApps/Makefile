TOP=../..

include $(TOP)/configure/CONFIG

-include ../src/CONFIG_STREAM
-include ../../src/CONFIG_STREAM

LIBRARY_DEFAULT = streamSynApps

DBD += $(LIBRARY_DEFAULT).dbd

ifeq ($(LOADABLE_MODULE),YES)
SRCS += $(LIBRARY_DEFAULT)_registerRecordDeviceDriver.cpp
endif 
SRCS += $(SYNAPPS_RECORDS:%=dev%Stream.c)

LIB_LIBS += stream $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

# create streamSynApps.dbd from all SYNAPPS_RECORDS
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../../src/CONFIG_STREAM
	$(PERL) ../../src/makedbd.pl $(SYNAPPS_RECORDS) > $@
