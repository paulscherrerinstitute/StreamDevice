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

LIBRARY_DEFAULT = stream

DBD += $(LIBRARY_DEFAULT).dbd
DBD += $(LIBRARY_DEFAULT)-base.dbd
ifdef CALC
DBD += $(LIBRARY_DEFAULT)-scalcout.dbd
endif

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
$(COMMON_DIR)/$(LIBRARY_DEFAULT)-base.dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(if $(ASYN),--with-asyn) $(if $(BASE_3_14),,-3.13) $(filter-out scalcout, $(RECORDTYPES)) > $@

$(LIBRARY_DEFAULT)-base.dbd$(DEP): ../CONFIG_STREAM
	echo $(LIBRARY_DEFAULT)-base.dbd: $< > $@

STREAM_DBD_FILES = $(LIBRARY_DEFAULT)-base.dbd

ifdef CALC
# create stream-scalcout.dbd for scalcout record
$(COMMON_DIR)/$(LIBRARY_DEFAULT)-scalcout.dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl --rec-only scalcout > $@

$(LIBRARY_DEFAULT)-scalcout.dbd$(DEP): ../CONFIG_STREAM
	echo $(LIBRARY_DEFAULT)-scalcout.dbd: $< > $@

STREAM_DBD_FILES += $(LIBRARY_DEFAULT)-scalcout.dbd
endif

# create stream.dbd for all record types
$(COMMON_DIR)/$(LIBRARY_DEFAULT).dbd: ../CONFIG_STREAM
	$(PERL) ../makedbd.pl $(if $(ASYN),--with-asyn) $(if $(BASE_3_14),,-3.13) $(RECORDTYPES) > $@

$(LIBRARY_DEFAULT).dbd$(DEP): ../CONFIG_STREAM
	echo $(LIBRARY_DEFAULT).dbd: $< > $@
