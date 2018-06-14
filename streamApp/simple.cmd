#!/bin/sh
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
