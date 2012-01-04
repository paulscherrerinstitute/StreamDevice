/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is error and debug message handling of StreamDevice.    *
* Please refer to the HTML files in ../doc/ for a detailed     *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#ifndef StreamError_h
#define StreamError_h

#include <stdarg.h>
#include <stddef.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

extern int streamDebug;
extern void (*StreamPrintTimestampFunction)(char* buffer, int size);

void StreamError(int line, const char* file, const char* fmt, ...)
__attribute__ ((format(printf,3,4)));

void StreamVError(int line, const char* file, const char* fmt, va_list args)
__attribute__ ((format(printf,3,0)));

void StreamError(const char* fmt, ...)
__attribute__ ((format(printf,1,2)));

inline void StreamVError(const char* fmt, va_list args)
{
    StreamVError(0, NULL, fmt, args); 
}

class StreamDebugClass
{
    const char* file;
    int line;
public:
    StreamDebugClass(const char* file, int line) :
        file(file), line(line) {}
    int print(const char* fmt, ...)
        __attribute__ ((format(printf,2,3)));
};

inline StreamDebugClass
StreamDebugObject(const char* file, int line)
{ return StreamDebugClass(file, line); }

#define error StreamError
#define debug (!streamDebug)?0:StreamDebugObject(__FILE__,__LINE__).print

#if (__GNUC__ == 2 && __GNUC_MINOR__ == 7)
/* Bug in cygnus-2.7.2 compiler: temporary objects crash the compiler */
#define NO_TEMPORARY
#endif

#endif
