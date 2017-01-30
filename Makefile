TOP = ..

# Look if we have EPICS R3.13 or R3.14
ifneq ($(wildcard $(TOP)/configure),)
  # EPICS R3.14
  DIRS = configure
  include $(TOP)/configure/CONFIG
else ifneq ($(wildcard $(TOP)/config),)
  # EPICS R3.13
  CONFIG = $(TOP)/config
  DIRS = config
  include $(TOP)/config/CONFIG_APP
else
  TOP = .
  DIRS = configure
  include $(TOP)/configure/CONFIG
endif

DIRS += src
src_DEPEND_DIRS = configure
DIRS += streamApp
streamApp_DEPEND_DIRS = src

include $(CONFIG)/RULES_DIRS
