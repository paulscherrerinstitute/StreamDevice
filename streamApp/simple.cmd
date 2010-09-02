dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

#where can protocols be located?
epicsEnvSet "STREAM_PROTOCOL_PATH", ".:protocols:../protocols/"

#setup the busses

#drvAsynIPPortConfigure "L0", "localhost:40000"
vxi11Configure "L0","gpib-dz-1",0,0.0,"gpib0",0,0

#load the records
dbLoadRecords "simple.db","P=DZ,BUS=L0 28"

var streamDebug 1
iocInit

#enable debug output
var streamDebug 1
