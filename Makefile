TOP = ..

# Look if we have EPICS R3.13 or R3.14+
ifeq ($(wildcard $(TOP)/configure),)
  # EPICS R3.13
  include $(TOP)/config/CONFIG_APP
  CONFIG = $(TOP)/config
else
  # EPICS R3.14+
  include $(TOP)/configure/CONFIG
endif

DIRS += src
DIRS += streamApp
streamApp_DEPEND_DIRS  = src

include $(CONFIG)/RULES_DIRS
