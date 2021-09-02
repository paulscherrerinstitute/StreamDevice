##########################################################################
# This is the EPICS 3.14+ Makefile for StreamDevice.
# Normally it should not be necessary to modify this file.
# All configuration can be done in CONFIG_STREAM
#
# (C) 2007,2018 Dirk Zimoch (dirk.zimoch@psi.ch)
#
# This file is part of StreamDevice.
#
# StreamDevice is free software: You can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# StreamDevice is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with StreamDevice. If not, see https://www.gnu.org/licenses/.
#########################################################################/

TOP= ..
include $(TOP)/configure/CONFIG

-include CONFIG_STREAM
-include ../CONFIG_STREAM

LIBRARY_IOC = stream

DBD += stream.dbd
DBD += stream-base.dbd
ifneq ($(words $(CALC) $(SYNAPPS)), 0)
DBD += stream-scalcout.dbd
endif

ifdef ASYN
LIB_LIBS += asyn
else
$(warning Asyn not included! Didn't you set ASYN in your RELEASE file?)
endif

ifeq ($(LOADABLE_MODULE),YES)
SRCS += stream_registerRecordDeviceDriver.cpp
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
ifdef ENABLE_STATIC
CPPFLAGS += -DPCRE_STATIC
endif
endif
endif

LIB_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += devStream.h
INC += StreamFormat.h
INC += StreamFormatConverter.h
INC += StreamBuffer.h
INC += StreamError.h
INC += StreamVersion.h
INC += StreamProtocol.h
INC += StreamBusInterface.h
INC += StreamCore.h
INC += MacroMagic.h

# switch off annoying rset warnings in 3.16+
CPPFLAGS += -DUSE_TYPED_RSET

CPPFLAGS += -DSTREAM_INTERNAL

-include $(TOP)/configure/RULES

# Update version string whenever something changed.
StreamVersion$(OBJ): $(COMMON_DIR)/StreamVersion.h $(filter-out StreamVersion$(OBJ),$(LIBOBJS) $(LIBRARY_OBJS)) ../CONFIG_STREAM
$(COMMON_DIR)/StreamVersion.h: ../../.VERSION $(SRCS) $(filter-out StreamVersion.h, $(INC))
	@echo Creating $@
	$(PERL) ../makeStreamVersion.pl $@

# Add references to all registrars to main file to avoid
# missing initialization.
StreamCore$(OBJ) StreamCore$(DEP): streamReferences

streamReferences: ../CONFIG_STREAM
	$(PERL) ../makeref.pl Interface $(BUSSES) > $@
	$(PERL) ../makeref.pl Converter $(FORMATS) >> $@

# create stream-base.dbd from all RECORDTYPES except scalcout record
$(COMMON_DIR)/stream-base.dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(if $(ASYN),--with-asyn) $(if $(BASE_3_14),,-3.13) $(filter-out scalcout, $(RECORDTYPES)) > $@

stream-base.dbd$(DEP): ../CONFIG_STREAM
        echo stream-base.dbd: $< > $@

STREAM_DBD_FILES = stream-base.dbd

ifneq ($(words $(CALC) $(SYNAPPS)), 0)
# create stream-scalcout.dbd for scalcout record
$(COMMON_DIR)/stream-scalcout.dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl --rec-only scalcout > $@

stream-scalcout.dbd$(DEP): ../CONFIG_STREAM
        echo stream-scalcout.dbd: $< > $@

STREAM_DBD_FILES += stream-scalcout.dbd
endif

# create stream.dbd for all record types
$(COMMON_DIR)/stream.dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(if $(ASYN),--with-asyn) $(if $(BASE_3_14),,-3.13) $(RECORDTYPES) > $@

stream.dbd$(DEP): ../CONFIG_STREAM
        echo stream.dbd: $< > $@
