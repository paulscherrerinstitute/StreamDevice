##########################################################################
# This is an EPICS 3.14+ Makefile for StreamDevice.
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

PROD = streamApp
DBD  = streamApp.dbd

streamApp_DBD += base.dbd

# In 3.14.12 aaiRecord.dbd and aaoRecord.dbd are part of base.dbd
# In earlier versions, these records are buggy and not included by default
ifndef BASE_3_15
streamApp_DBD += aaiRecord.dbd aaoRecord.dbd
endif

PROD_SRCS += streamApp_registerRecordDeviceDriver.cpp
PROD_SRCS_DEFAULT = streamAppMain.cc
PROD_SRCS_vxWorks = -nil-

PROD_LIBS = stream

ifdef ASYN
streamApp_DBD += asyn.dbd
streamApp_DBD += drvAsynIPPort.dbd
streamApp_DBD += drvAsynSerialPort.dbd
# vxi11 support is optional in recent asyn versions
#streamApp_DBD += drvVxi11.dbd

PROD_LIBS += asyn
# cygwin needs separate RPC library for asyn
PROD_SYS_LIBS_cygwin32 += $(CYGWIN_RPC_LIB)
endif

ifneq ($(words $(CALC) $(SYNAPPS)), 0)
# With synApps scalcout record
streamApp_DBD += calcSupport.dbd
PROD_LIBS += calc
ifneq ($(words $(SSCAN) $(SYNAPPS)), 0)
PROD_LIBS += sscan
endif
endif

ifneq ($(words $(SNCSEQ) $(SYNAPPS)), 0)
PROD_LIBS += seq pv
endif

streamApp_DBD += stream.dbd

ifdef PCRE
PROD_LIBS += pcre2-8
else
ifneq ($(words $(PCRE_LIB) $(PCRE_INCLUDE)),0)
PROD_SYS_LIBS_DEFAULT += pcre2-8
PROD_SYS_LIBS_WIN32 += $(PCRE_LIB)\\pcre2-8
SHRLIB_DEPLIB_DIRS += $(PCRE_LIB)
endif
endif

PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

# Some linux systems moved RPC related symbols to libtirpc
# Define TIRPC in configure/CONFIG_SITE in this case
ifeq ($(TIRPC),YES)
  USR_INCLUDES_Linux += -I/usr/include/tirpc
  PROD_SYS_LIBS_DEFAULT += tirpc
endif

# switch off annoying rset warnings in 3.16+
CPPFLAGS += -DUSE_TYPED_RSET

-include $(TOP)/configure/RULES

ifdef BASE_3_15
clean:
else
clean::
endif
	$(RM) core* *.log tests/test.* tests/*log
