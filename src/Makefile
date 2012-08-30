################################################################
# StreamDevice Support                                         #
#                                                              #
# (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          #
# (C) 2007 Dirk Zimoch (dirk.zimoch@psi.ch)                    #
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

# find system wide or local PCRE header and library
ifdef PCRE_INCLUDE
RegexpConverter_INCLUDES += -I$(PCRE_INCLUDE)
endif
ifdef PCRE
LIB_LIBS += pcre
else
ifneq ($(words $(PCRE_LIB) $(PCRE_INCLUDE)),0)
LIB_SYS_LIBS_DEFAULT += pcre
LIB_SYS_LIBS_WIN32 += $(PCRE_LIB)\\pcre
SHRLIB_DEPLIB_DIRS += $(PCRE_LIB)
endif
endif

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += devStream.h

include $(TOP)/configure/RULES

# Update version string (contains __DATE__ and __TIME__)
# each time make runs.
StreamVersion$(OBJ): ../*.c ../*.h ../*.cc ../CONFIG_STREAM ../Makefile

# Add references to all registrars to main file to avoid
# missing initialization.
StreamCore$(OBJ) StreamCore$(DEP): streamReferences

streamReferences: ../CONFIG_STREAM
	$(PERL) ../makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../makeref.pl Converter $(FORMATS) >> $@

# create stream.dbd from all RECORDS
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(RECORDS) > $@

$(LIBRARY_DEFAULT).dbd$(DEP): ../CONFIG_STREAM
	echo $(LIBRARY_DEFAULT).dbd: $< > $@

endif
