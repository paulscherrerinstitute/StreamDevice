TOP = ..
ifneq ($(wildcard ../configure),)
  # We are in an EPICS R3.14+ <TOP> location
  include $(TOP)/configure/CONFIG
else ifneq ($(wildcard ../config),)
  # We are in an EPICS R3.13 <TOP> location
  CONFIG = $(TOP)/config
  include $(TOP)/config/CONFIG_APP
else
  # Using our own local configuration
  TOP = .
  DIRS = config configure 
  src_DEPEND_DIRS := $(DIRS)
  include $(TOP)/configure/CONFIG
endif

DIRS += src
DIRS += streamApp
streamApp_DEPEND_DIRS = src

include $(CONFIG)/RULES_TOP

docs/stream.pdf: docs/*.html docs/*.css docs/*.png
	cd docs; makepdf

pdf: docs/stream.pdf
