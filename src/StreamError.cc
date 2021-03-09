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

#include "StreamError.h"
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif /* _WIN32 */
#include <string.h>
#include <time.h>
#include <stdio.h>

int streamDebug = 0;
int streamError = 1;
StreamErrorEngine* pErrEngine = NULL;
FILE *StreamDebugFile = NULL;

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

/* Enable ANSI color support in Windows console */
static bool win_console_init() {
    HANDLE hCons[] = { GetStdHandle(STD_ERROR_HANDLE),
                       GetStdHandle(STD_OUTPUT_HANDLE) };
    for(int i=0; i < sizeof(hCons) / sizeof(HANDLE); ++i)
    {
        DWORD dwMode = 0;
        if (hCons[i] == NULL ||
            hCons[i] == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(hCons[i], &dwMode))
        {
            return false;
        }
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hCons[i], dwMode))
        {
            return false;
        }
    }
    return true;
}

/* do isatty() call second as always want to run win_console_init() */
int streamDebugColored = win_console_init() && _isatty(_fileno(stdout));

#else

int streamDebugColored = isatty(fileno(stdout));

#endif /* _WIN32 */

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
    snprintf(buffer2, "%s%s ", ansiEscape(ANSI_RED_BOLD), timestamp);
    if (file)
    {
        snprintf(buffer1, sizeof(buffer1), "%s%s line %d: ", buffer2, file, line);
    }
    else
    {
        snprintf(buffer1, sizeof(buffer1), "%s", buffer2);
    }
    // Copy only a number of characters that will not overflow buffer, with
    // additional space for the final required ANSI_RESET and \0.
    vsnprintf(bufferAux, sizeof(bufferAux) - strlen(buffer1) - strlen(ansiEscape(ANSI_RESET)) - 1, fmt, args);

    snprintf(buffer2, sizeof(buffer2), "%s%s%s", buffer1, bufferAux, ansiEscape(ANSI_RESET));

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

/**
 * Return an ANSI escape code if coloured debug output is enabled
 */
const char* ansiEscape(AnsiMode mode)
{
    static const char* AnsiEscapes[] = { "\033[7m", "\033[27m", "\033[47m",
                                         "\033[0m", "\033[31;1m" };
    return streamDebugColored ? AnsiEscapes[mode] : "";
}
