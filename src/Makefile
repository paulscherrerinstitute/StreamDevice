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

TOP = ../..
ifneq ($(wildcard ../../configure),)
  include $(TOP)/configure/CONFIG
else ifneq ($(wildcard ../../config),)
  include $(TOP)/config/CONFIG_APP
  include $(TOP)/config/RULES_ARCHS
else
  TOP= ..
  include $(TOP)/configure/CONFIG
endif

-include CONFIG_STREAM
-include ../CONFIG_STREAM

LIBRARY_DEFAULT = stream

DBD += $(LIBRARY_DEFAULT).dbd

ifdef ASYN
LIB_LIBS += asyn
else
$(warning Asyn not included! Didn't you set ASYN in your RELEASE file?)
endif

ifeq ($(LOADABLE_MODULE),YES)
SRCS += $(LIBRARY_DEFAULT)_registerRecordDeviceDriver.cpp
endif 
SRCS += $(BUSSES:%=%Interface.cc)
SRCS += $(FORMATS:%=%Converter.cc)
SRCS += $(RECORDTYPES:%=dev%Stream.c)
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

# switch off annoying rset warnings in 3.16+
CPPFLAGS += -DUSE_TYPED_RSET

-include $(TOP)/configure/RULES

# Update version string whenever something changes
StreamVersion$(OBJ): $(filter-out StreamVersion.o,$(LIBOBJS)$(LIBRARY_OBJS))

# Add references to all registrars to main file to avoid
# missing initialization.
StreamCore$(OBJ) StreamCore$(DEP): streamReferences

streamReferences: ../CONFIG_STREAM
	$(PERL) ../makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../makeref.pl Converter $(FORMATS) >> $@

# create stream.dbd from all RECORDTYPES
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(if $(ASYN),--with-asyn) $(if $(BASE_3_14),,-3.13) $(RECORDTYPES) > $@

$(LIBRARY_DEFAULT).dbd$(DEP): ../CONFIG_STREAM
	echo $(LIBRARY_DEFAULT).dbd: $< > $@
