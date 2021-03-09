#!/bin/sh
##########################################################################
# This is an example and debug EPICS startup script for StreamDevice.
#
# (C) 2010 Dirk Zimoch (dirk.zimoch@psi.ch)
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

exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

#where can protocols be located?
epicsEnvSet "STREAM_PROTOCOL_PATH", ".:protocols:../protocols/"

#setup the hardware

drvAsynIPPortConfigure "L0", "localhost:40000"
#vxi11Configure "L0","gpib-hostname-or-ip",0,0.0,"gpib0"

#load the records
dbLoadRecords "simple.db","P=DZ,BUS=L0 28"

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit

#enable debug output
#var streamDebug 1
