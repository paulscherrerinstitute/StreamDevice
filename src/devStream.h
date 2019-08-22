/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the header for the EPICS interface to StreamDevice.  *
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

#ifndef devStream_h
#define devStream_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define STREAM_MAJOR 2
#define STREAM_MINOR 8

#ifndef OK
#define OK 0
#endif

#ifndef ERROR
#define ERROR -1
#endif

#define DO_NOT_CONVERT 2
#define INIT_RUN (!interruptAccept)

#include "epicsVersion.h"
#ifdef BASE_VERSION
#define EPICS_3_13
/* EPICS 3.13 include files are not C++ ready. */
#ifdef __cplusplus
extern "C" {
#endif
#endif

#ifdef epicsExportSharedSymbols
#   define devStream_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#   include <shareLib.h>
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

#ifdef devStream_epicsExportSharedSymbols
#   undef devStream_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include <shareLib.h>
#endif

#ifdef EPICS_3_13
#ifdef __cplusplus
}
#endif
#else
#include "epicsStdioRedirect.h"
#endif

#ifdef _WIN32
typedef ptrdiff_t ssize_t;
#endif

typedef const struct format_s {
    unsigned char type;
    const struct StreamFormat* priv;
} format_t;

epicsShareExtern FILE* StreamDebugFile;
extern const char StreamVersion [];

typedef long (*streamIoFunction) (dbCommon*, format_t*);

#ifdef __cplusplus
extern "C" {
#endif

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
