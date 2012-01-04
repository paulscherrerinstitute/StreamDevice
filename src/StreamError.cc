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

#include "StreamError.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

int streamDebug = 0;
extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
FILE *StreamDebugFile = NULL;
}

#ifndef va_copy
#ifdef __va_copy
#define va_copy __va_copy
#endif
#endif

/* You can globally change the printTimestamp function
   by setting the StreamPrintTimestampFunction variable
   to your own function.
*/
static void printTimestamp(char* buffer, int size)
{
    time_t t;
    struct tm tm;
    time(&t);
#ifdef _WIN32
    tm = *localtime(&t);
#else    
    localtime_r(&t, &tm);
#endif
    strftime(buffer, size, "%Y/%m/%d %H:%M:%S", &tm);
}

void (*StreamPrintTimestampFunction)(char* buffer, int size) = printTimestamp;

void StreamError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(0, NULL, fmt, args);
    va_end(args);
}

void StreamError(int line, const char* file, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(line, file, fmt, args);
    va_end(args);
}

void StreamVError(int line, const char* file, const char* fmt, va_list args)
{
    char timestamp[40];
    StreamPrintTimestampFunction(timestamp, 40);
#ifdef va_copy
    if (StreamDebugFile)
    {
        va_list args2;
        va_copy(args2, args);
        fprintf(StreamDebugFile, "%s ", timestamp);
        vfprintf(StreamDebugFile, fmt, args2);
        fflush(StreamDebugFile);
        va_end(args2);
    }
#endif
    fprintf(stderr, "\033[31;1m");
    fprintf(stderr, "%s ", timestamp);
    if (file)
    {
        fprintf(stderr, "%s line %d: ", file, line);
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m");
}

int StreamDebugClass::
print(const char* fmt, ...)
{
    va_list args;
    char timestamp[40];
    StreamPrintTimestampFunction(timestamp, 40);
    va_start(args, fmt);
    const char* f = strrchr(file, '/');
    if (f) f++; else f = file;
    FILE* fp = StreamDebugFile ? StreamDebugFile : stderr;
    fprintf(fp, "%s ", timestamp);
    fprintf(fp, "%s:%d: ", f, line);
    vfprintf(fp, fmt, args);
    fflush(fp);
    va_end(args);
    return 1;
}

