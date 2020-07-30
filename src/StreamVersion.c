/*************************************************************************
* This file provides a version string for StreamDevice.
* Please see ../docs/ for detailed documentation.
*
* (C) 1999,2005 Dirk Zimoch (dirk.zimoch@psi.ch)
*
* This file is part of StreamDevice.
*
* StreamDevice is free software: You can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* StreamDevice is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with StreamDevice. If not, see https://www.gnu.org/licenses/.
*************************************************************************/

#include "StreamVersion.h"

#define STR2(x) #x
#define STR(x) STR2(x)
const char StreamVersion [] =
    "StreamDevice"
#ifdef STREAM_MAJOR
    " " STR(STREAM_MAJOR)
    "." STR(STREAM_MINOR)
    "." STR(STREAM_PATCHLEVEL)
    STREAM_DEV
#endif
#ifdef STREAM_COMMIT_DATE
    " " STREAM_COMMIT_DATE
#endif
#ifdef STREAM_COMMIT_HASH
    "\n  commit: " STREAM_COMMIT_HASH
#endif
;
