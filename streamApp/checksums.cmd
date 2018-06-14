#!/bin/sh
exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

drvAsynIPPortConfigure "terminal", "localhost:40000"

dbLoadRecords checksums.db

#log debug output to file
#streamSetLogfile StreamDebug.log
var streamError 1

iocInit
#var streamDebug 1
