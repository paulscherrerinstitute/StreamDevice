#!/bin/sh
exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

# no autoconnect for web servers (see regexp.proto)
drvAsynIPPortConfigure web epics.web.psi.ch:80 0 1

dbLoadRecords regexp.db

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit
#var streamDebug 1

#let's see the title of the PSI EPICS web site
epicsThreadSleep 1
dbgf DZ:regexp
