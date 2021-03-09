/*************************************************************************
* This is the header for StreamDevice interfaces to EPICS.
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

#ifndef devStream_h
#define devStream_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef STREAM_INTERNAL
#include "StreamVersion.h"
#endif

#ifndef OK
#define OK 0
#endif

#ifndef ERROR
#define ERROR -1
#endif

#define DO_NOT_CONVERT 2
#define INIT_RUN (!interruptAccept)

#ifdef epicsExportSharedSymbols
#   define devStream_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "epicsVersion.h"
#ifdef BASE_VERSION
#define EPICS_3_13
/* EPICS 3.13 include files are not C++ ready. */
#ifdef __cplusplus
extern "C" {
#endif
#endif

#include "dbCommon.h"
#include "dbScan.h"
#include "devSup.h"
#include "dbAccess.h"
#include "errlog.h"
#include "alarm.h"
#include "recGbl.h"
#include "dbEvent.h"
#include "epicsMath.h"

#ifdef EPICS_3_13
#ifdef __cplusplus
}
#endif
#else
#include "epicsStdioRedirect.h"
#endif

#ifdef devStream_epicsExportSharedSymbols
#   undef devStream_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#ifdef _WIN32
typedef ptrdiff_t ssize_t;
#endif

typedef const struct format_s {
    unsigned char type;
    const struct StreamFormat* priv;
} format_t;

extern FILE* StreamDebugFile;

typedef long (*streamIoFunction) (dbCommon*, format_t*);

#ifdef __cplusplus
extern "C" {
#endif

extern const char StreamVersion [];

long streamInit(int after);
long streamInitRecord(dbCommon *record,
    const struct link *ioLink,
    streamIoFunction readData, streamIoFunction writeData);
long streamReport(int interest);
long streamReadWrite(dbCommon *record);
long streamGetIointInfo(int cmd,
    dbCommon *record, IOSCANPVT *ppvt);
long streamPrintf(dbCommon *record, format_t *format, ...);
ssize_t streamScanfN(dbCommon *record, format_t *format,
    void*, size_t maxStringSize);

#ifdef __cplusplus
}
#endif

/* backward compatibility stuff */
#define streamScanf(record, format, value) \
    streamScanfN(record, format, value, MAX_STRING_SIZE)
#define streamRead streamReadWrite
#define streamWrite streamReadWrite
#define streamReport NULL

#ifdef EPICS_3_13
#define epicsExportAddress(a,b) extern int dummy
#else
#include "epicsExport.h"
#endif

#endif
