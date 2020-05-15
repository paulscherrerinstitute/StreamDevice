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

my $dir = "O.Common";
my $versionfile = "StreamVersion.h";

my $version = `git describe --tags --dirty --match "[0-9]*"`
    or die "Cannot run git.\n";

my ( $major, $minor, $patch, $dev );

$version =~ m/(\d+)\.(\d+)\.(\d+)?(.*)?/
    or die "Unexpected git tag format $version\n";

$major = $1; $minor=$2; $patch=$3; $dev=$4;

print << "EOF";
/* Generated file $versionfile */

#ifndef StreamVersion_h
#define StreamVersion_h

#define STREAM_MAJOR $major
#define STREAM_MINOR $minor
#define STREAM_PATCHLEVEL $patch
#define STREAM_DEV "$dev"

#endif /* StreamVersion_h */
EOF
