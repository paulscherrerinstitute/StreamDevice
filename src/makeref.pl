$t=@ARGV[0];
shift;
for (@ARGV) {
     print "extern void* ref_${_}$t;\n";
     print "void* p$_ = ref_${_}$t;\n";
}
