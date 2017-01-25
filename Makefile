TOP = ..

# Look if we have EPICS R3.13 or R3.14
ifneq ($(wildcard $(TOP)/configure),)
  # EPICS R3.14
  include $(TOP)/configure/CONFIG
else ifneq ($(wildcard $(TOP)/config),)
  # EPICS R3.13
  include $(TOP)/config/CONFIG_APP
  CONFIG = $(TOP)/config
else
  TOP = .
  include $(TOP)/configure/CONFIG
endif

DIRS = src
streamApp_DEPEND_DIRS  = src
DIRS += streamApp

include $(CONFIG)/RULES_DIRS
