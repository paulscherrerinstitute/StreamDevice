#!/bin/sh
exec O.$EPICS_HOST_ARCH/streamApp $0
dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

#where can protocols be located?
epicsEnvSet "STREAM_PROTOCOL_PATH", ".:protocols:../protocols/"

#setup the busses

#example serial port setup
#drvAsynSerialPortConfigure "COM2", "/dev/ttyS1"
#asynOctetSetInputEos "COM2",0,"\r\n"
#asynOctetSetOutputEos "COM2",0,"\r\n"
#asynSetOption ("COM2", 0, "baud", "9600")
#asynSetOption ("COM2", 0, "bits", "8")
#asynSetOption ("COM2", 0, "parity", "none")
#asynSetOption ("COM2", 0, "stop", "1")
#asynSetOption ("COM2", 0, "clocal", "Y")
#asynSetOption ("COM2", 0, "crtscts", "N")

#example telnet style IP port setup
drvAsynIPPortConfigure "terminal", "localhost:40000"

# Either set terminators here or in the protocol
asynOctetSetInputEos "terminal",0,"\r\n"
asynOctetSetOutputEos "terminal",0,"\r\n"

#example VXI11 (GPIB via IP) port setup
#vxi11Configure "GPIB","ins023",1,5.0,"hpib"

#load the records
dbLoadRecords "example.db","PREFIX=DZ"

#log debug output to file
#streamSetLogfile StreamDebug.log

#lots(!) of debug output before iocInit
#var streamDebug 1

iocInit

#enable debug output
#var streamDebug 1
