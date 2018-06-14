#!/bin/sh
exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

epicsEnvSet "STREAM_PROTOCOL_PATH", "."

drvAsynIPPortConfigure "terminal", "localhost:40000"

dbLoadRecords "test.db","P=TEST"

#log debug output to file
#streamSetLogfile StreamDebug.log

iocInit

#var streamDebug 1
