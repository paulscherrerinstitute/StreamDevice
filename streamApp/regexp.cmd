dbLoadDatabase "O.Common/streamApp.dbd"
streamApp_registerRecordDeviceDriver

# no autoconnect for web servers (see regexp.proto)
drvAsynIPPortConfigure web epics.web.psi.ch:80 0 1

dbLoadRecords regexp.db

iocInit
# var streamDebug 1
