TOP=..
include $(TOP)/config/CONFIG_APP
ifneq ($(wildcard $(EPICS_BASE)/config),)
include $(EPICS_BASE)/config/RULES_ARCHS
else
build install clean realclean:
endif
