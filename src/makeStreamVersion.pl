##########################################################################
# This is a helper script for StreamDevice.
# It generates a version file from git tags.
#
# (C) 2020 Dirk Zimoch (dirk.zimoch@psi.ch)
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

use strict;


my ( $major, $minor, $patch, $dev, $date, $hash );

if (my $version = `git describe --tags --abbrev=0 --dirty --match "[0-9]*" 2>/dev/null`) {
    if ($version =~ m/(\d+)\.(\d+)\.(\d+)?(.*)?/) {
        $major = $1; $minor = $2; $patch = $3; $dev = $4;
    }
    if (`git log -1 --format="%H %ci"`=~ m/([[:xdigit:]]+) (.+)/) {
        $hash = $1; $date = $2;
    }
}
if (!$major) {
    if (open(my $fh, '<', '../../.VERSION')) {
        while (my $line = <$fh>) {
            if ($line =~ m/COMMIT: *([[:xdigit:]]+)/) {
                $hash = $1;
            }
            if ($line =~ m/REFS: .*tag: *(\d+)\.(\d+)\.?(\d+)?/) {
                $major = $1; $minor = $2; $patch = $3 or $patch = 0;
            }
            if ($line =~ m/DATE: *([-0-9:+ ]*)/) {  
                $date = $1;
            }
        }
    } else {
        print "neither git repo nor .VERSION file found\n";
    }
}

open my $out, '>', @ARGV or die $!;
print $out <<EOF;
/* Generated file */

#ifndef StreamVersion_h
#define StreamVersion_h

EOF
if ($major) {
    print $out <<EOF;
#define STREAM_MAJOR $major
#define STREAM_MINOR $minor
#define STREAM_PATCHLEVEL $patch
#define STREAM_DEV "$dev"
EOF
}
if ($hash) {
    print $out "#define STREAM_COMMIT_HASH \"$hash\"\n";
}
if ($date) {
    print $out "#define STREAM_COMMIT_DATE \"$date\"\n";
}
print $out "#endif /* StreamVersion_h */\n";
close $out
