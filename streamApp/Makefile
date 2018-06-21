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
# edit asynRegistrars.dbd if necessary
streamApp_DBD += asynRegistrars.dbd
# add asynRecord.dbd if you like
streamApp_DBD += asynRecord.dbd
PROD_LIBS += asyn
# cygwin needs separate RPC library for asyn
PROD_SYS_LIBS_cygwin32 += $(CYGWIN_RPC_LIB)
endif

ifneq ($(words $(CALC) $(SYNAPPS)), 0)
# With synApps scalcout record
streamApp_DBD += calcSupport.dbd
PROD_LIBS += calc
# older calc versions require sscan
#PROD_LIBS += sscan
endif

streamApp_DBD += stream.dbd

ifdef PCRE
PROD_LIBS += pcre
else
ifneq ($(words $(PCRE_LIB) $(PCRE_INCLUDE)),0)
PROD_SYS_LIBS_DEFAULT += pcre
PROD_SYS_LIBS_WIN32 += $(PCRE_LIB)\\pcre
SHRLIB_DEPLIB_DIRS += $(PCRE_LIB)
endif
endif

PROD_LIBS += $(EPICS_BASE_IOC_LIBS)

# switch off annoying rset warnings in 3.16+
CPPFLAGS += -DUSE_TYPED_RSET

-include $(TOP)/configure/RULES

ifdef BASE_3_15
clean:
else
clean::
endif
	$(RM) core* *.log tests/test.* tests/*log
