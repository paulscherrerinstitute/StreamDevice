if (@ARGV[0] eq "--with-asyn") {
    shift;
    $asyn = 1;
}
if (@ARGV[0] eq "-3.13") {
    shift;
} else {
    print "variable(streamDebug, int)\n";
    print "variable(streamError, int)\n";
    print "registrar(streamRegistrar)\n";
    if ($asyn) { print "registrar(AsynDriverInterfaceRegistrar)\n"; }
}
print "driver(stream)\n";
for (@ARGV) {
     print "device($_,INST_IO,dev${_}Stream,\"stream\")\n";
}
