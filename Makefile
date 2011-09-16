TOP = ..

DIRS = src
streamApp_DEPEND_DIRS  = src

# Look if we have EPICS R3.13 or R3.14
ifeq ($(wildcard $(TOP)/configure),)
  # EPICS R3.13
  include $(TOP)/config/CONFIG_APP
  CONFIG = $(TOP)/config
else
  # EPICS R3.14
  include $(TOP)/configure/CONFIG
  ifneq ($(words $(CALC) $(SYNAPPS)), 0)
    # with synApps calc module (contains scalcout)
    DIRS += srcSynApps
    srcSynApps_DEPEND_DIRS  = src
    streamApp_DEPEND_DIRS  += srcSynApps
  endif
endif

DIRS += streamApp

include $(CONFIG)/RULES_DIRS
