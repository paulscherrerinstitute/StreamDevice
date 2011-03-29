for (@ARGV) {
     print "device($_,INST_IO,dev${_}Stream,\"stream\")\n";
}
print "driver(stream)\n";
print "variable(streamDebug, int)\n";
print "registrar(streamRegistrar)\n";
