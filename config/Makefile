##########################################################################
# This is an EPICS Makefile for StreamDevice.
# Normally it should not be necessary to modify this file.
# All configuration can be done in CONFIG_STREAM
#
# (C) 2018 Dirk Zimoch (dirk.zimoch@psi.ch)
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

TOP=..
include $(TOP)/config/CONFIG_APP
ifneq ($(wildcard $(EPICS_BASE)/config),)
include $(EPICS_BASE)/config/RULES_ARCHS
else
build install clean realclean:
endif
