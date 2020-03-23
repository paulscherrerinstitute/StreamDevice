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

BIN="<library directory>"
DBD="<dbd directory>"
HOME="<ioc home direcory>"

#where can protocols be located?
STREAM_PROTOCOL_PATH=".:protocols:../protocols/"

cd BIN
ld < iocCore
ld < streamApp.munch
dbLoadDatabase "streamApp.dbd",DBD

drvAsynIPPortConfigure "terminal", "xxx.xxx.xxx.xxx:40000"

#load the records
cd HOME
dbLoadRecords "example.db","PREFIX=DZ"

#lots of debug output
#streamDebug=1
iocInit
