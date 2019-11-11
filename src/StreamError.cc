/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is error and debug message handling of StreamDevice.    *
* Please refer to the HTML files in ../docs/ for a detailed    *
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
#ifdef _WIN32
#include <windows.h>
#endif
#include <string.h>
#include <time.h>
#include <stdio.h>

int streamDebug = 0;
int streamError = 1;
StreamErrorEngine* pErrEngine = NULL;

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

#ifdef _WIN32
#define localtime_r(timet,tm) localtime_s(tm,timet)

/* this may not be defined if using older Windows SDKs */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

/* Enable ANSI colors in Windows console */
static int win_console_init() {
	DWORD dwMode = 0;
    HANDLE hCons = GetStdHandle(STD_ERROR_HANDLE);
	GetConsoleMode(hCons, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hCons, dwMode);
	return 0;
}
static int s = win_console_init();

#endif

/* You can globally change the printTimestamp function
   by setting the StreamPrintTimestampFunction variable
   to your own function.
*/
static void printTimestamp(char* buffer, size_t size)
{
    time_t t;
    struct tm tm;
    time(&t);
    localtime_r(&t, &tm);
    strftime(buffer, size, "%Y/%m/%d %H:%M:%S", &tm);
}

void (*StreamPrintTimestampFunction)(char* buffer, size_t size) = printTimestamp;

void StreamError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(0, NULL, CAT_NONE, fmt, args);
    va_end(args);
}

void StreamError(int line, const char* file, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(line, file, CAT_NONE, fmt, args);
    va_end(args);
}

// Need to place a useless bool parameter to avoid overload ambiguity
void StreamError(bool useless, ErrorCategory category, 
                 const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(0, NULL, category, fmt, args);
    va_end(args);
}

// Need to place a useless bool parameter to avoid overload ambiguity
void StreamError(bool useless, int line, const char* file,
                 ErrorCategory category, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    StreamVError(line, file, category, fmt, args);
    va_end(args);
}

void StreamVError(int line, const char* file, 
                  ErrorCategory category, const char* fmt, va_list args)
{
    char timestamp[40];
    char buffer1[500];
    char buffer2[500];
    char bufferAux[500];

    if (!(streamError || streamDebug)) return; // Error logging disabled
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
    // I'm using an easy way to protect against buffer overflow: I use snprintf
    // alternating buffer1 and buffer2, limiting by the size of the data to the
    // buffers.
    snprintf(buffer1, sizeof(buffer1), "\033[31;1m");
    snprintf(buffer2, sizeof(buffer2), "%s%s ", buffer1, timestamp);
    if (file)
    {
        snprintf(buffer1, sizeof(buffer1), "%s%s line %d: ", buffer2, file, line);
    }
    else
    {
        snprintf(buffer1, sizeof(buffer1), "%s", buffer2);
    }
    // Copy only a number of characters that will not overflow buffer, with
    // additional space for the final required "\033[0m" and \0 (5 characters).
    vsnprintf(bufferAux, sizeof(bufferAux) - strlen(buffer1) - 5, fmt, args);

    snprintf(buffer2, sizeof(buffer2), "%s%s\033[0m", buffer1, bufferAux);

    if (category == CAT_NONE || pErrEngine == NULL || 
                    pErrEngine->getTimeout() <= 0)
    {
        // We don't want to use the message engine
        fprintf(stderr, buffer2);
    }
    else
    {
        pErrEngine->callError(category, buffer2);
    }
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
