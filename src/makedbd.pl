##########################################################################
# This is a helper script for StreamDevice.
# It generates a file that registers StreamDevice interfaces with EPICS.
#
# (C) 2011 Dirk Zimoch (dirk.zimoch@psi.ch)
#
# This file is part of StreamDevice.
#
# StreamDevice is free software: You can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# StreamDevice is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with StreamDevice. If not, see https://www.gnu.org/licenses/.
#########################################################################/

if (@ARGV[0] eq "--rec-only") {
    shift;
} else {
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
}
for (@ARGV) {
     print "device($_,INST_IO,dev${_}Stream,\"stream\")\n";
}
