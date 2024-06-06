/*************************************************************************
* This is error and debug message handling of StreamDevice.
* Please see ../docs/ for detailed documentation.
*
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#ifndef StreamError_h
#define StreamError_h

#include <stdarg.h>
#include <stddef.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

extern int streamDebug;
extern int streamError;
extern int streamDebugColored;
extern int streamMsgTimeStamped;
extern void (*StreamPrintTimestampFunction)(char* buffer, size_t size);
extern const char* (*StreamGetThreadNameFunction)(void);

void StreamError(int line, const char* file, const char* fmt, ...)
__attribute__((__format__(__printf__,3,4)));

void StreamVError(int line, const char* file, const char* fmt, va_list args)
__attribute__((__format__(__printf__,3,0)));

void StreamError(const char* fmt, ...)
__attribute__((__format__(__printf__,1,2)));

inline void StreamVError(const char* fmt, va_list args)
{
    StreamVError(0, NULL, fmt, args);
}

class StreamDebugClass
{
    const char* file;
    int line;
public:
    StreamDebugClass(const char* file = NULL, int line = 0) :
        file(file), line(line) {}
    int print(const char* fmt, ...)
        __attribute__((__format__(__printf__,2,3)));
};

#define error StreamError
#define debug (!streamDebug)?0:StreamDebugClass(__FILE__,__LINE__).print
#define debug2 (streamDebug<2)?0:StreamDebugClass(__FILE__,__LINE__).print

/*
 * ANSI escape sequences for terminal output
 */
enum AnsiMode { ANSI_REVERSE_VIDEO, ANSI_NOT_REVERSE_VIDEO, ANSI_BG_WHITE,
                ANSI_RESET, ANSI_RED_BOLD };
extern const char* ansiEscape(AnsiMode mode);

#endif
