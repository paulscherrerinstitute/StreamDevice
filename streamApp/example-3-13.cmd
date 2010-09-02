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
