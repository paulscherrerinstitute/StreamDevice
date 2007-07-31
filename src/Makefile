################################################################
# StreamDevice Support                                         #
#                                                              #
# (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          #
# (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)                    #
#                                                              #
# This is the EPICS 3.14 Makefile of StreamDevice.             #
# Normally it should not be necessary to modify this file.     #
# All configuration can be done in CONFIG_STREAM               #
#                                                              #
# If you do any changes in this file, you are not allowed to   #
# redistribute it any more. If there is a bug or a missing     #
# feature, send me an email and/or your patch. If I accept     #
# your changes, they will go to the next release.              #
#                                                              #
# DISCLAIMER: If this software breaks something or harms       #
# someone, it's your problem.                                  #
#                                                              #
################################################################

TOP=../..

# Look if we have EPICS R3.13 or R3.14
ifeq ($(wildcard $(TOP)/configure),)
# EPICS R3.13
include $(TOP)/config/CONFIG_APP
# The real work is in Makefile.Vx
include $(TOP)/config/RULES_ARCHS
else

# EPICS R3.14
include $(TOP)/configure/CONFIG

-include CONFIG_STREAM
-include ../CONFIG_STREAM

LIBRARY_DEFAULT = stream

DBD += $(LIBRARY_DEFAULT).dbd

ifdef ASYN
LIB_LIBS += asyn
BUSSES += AsynDriver
endif

ifdef T_A
ifndef BUSSES
$(error No bus interface defined! Didn't you set ASYN in your RELEASE file?)
endif
endif

ifeq ($(LOADABLE_MODULE),YES)
SRCS += $(LIBRARY_DEFAULT)_registerRecordDeviceDriver.cpp
endif 
SRCS += $(BUSSES:%=%Interface.cc)
SRCS += $(FORMATS:%=%Converter.cc)
SRCS += $(RECORDS:%=dev%Stream.c)
SRCS += $(STREAM_SRCS)

LIB_LIBS += Com dbIoc dbStaticIoc registryIoc iocsh

ifeq ($(USE_MEMGUARD),YES)
# memguard looks for memory leaks (gcc only)
CPPFLAGS += -include ../memguard.h
LIB_SRCS += memguard.cc
endif

INC += devStream.h

include $(TOP)/configure/RULES

# Update version string (contains __DATE__ and __TIME__)
# each time make runs.
StreamVersion$(OBJ): FORCE
FORCE:

# Add references to all registrars to main file to avoid
# missing initialization.
StreamCore$(OBJ): streamReferences

streamReferences: ../CONFIG_STREAM
	@for i in $(BUSSES); \
	do echo "extern void* ref_$${i}Interface;"; \
           echo "void* p$$i = ref_$${i}Interface;"; \
	done > $@
	@for i in $(FORMATS); \
	do echo "extern void* ref_$${i}Converter;"; \
           echo "void* p$$i = ref_$${i}Converter;"; \
	done >> $@

# create stream.dbd from all RECORDS
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../CONFIG_STREAM
	@for r in $(RECORDS); \
	do echo "device($$r,INST_IO,dev$${r}Stream,\"stream\")"; \
	done > $@
	@echo "driver(stream)" >> $@
	@echo "variable(streamDebug, int)" >> $@
	@echo "registrar(streamRegistrar)" >> $@

endif
