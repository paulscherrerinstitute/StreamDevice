if (@ARGV[0] == "-3.13") {
    shift;
} else {
    print "variable(streamDebug, int)\n";
    print "registrar(streamRegistrar)\n";
}
print "driver(stream)\n";
for (@ARGV) {
     print "device($_,INST_IO,dev${_}Stream,\"stream\")\n";
}
