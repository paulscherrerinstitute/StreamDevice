/*************************************************************************
* This is the StreamDevice interface to EPICS.
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

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(vxWorks)
#include <symLib.h>
#include <sysSymTbl.h>
#endif

#include "epicsVersion.h"
#ifdef BASE_VERSION
#define EPICS_3_13
#endif

#ifdef EPICS_3_13
#include <semLib.h>
#include <wdLib.h>
#include <taskLib.h>

extern "C" {
#endif

#define epicsAlarmGLOBAL
#include "alarm.h"
#undef epicsAlarmGLOBAL
#include "dbStaticLib.h"
#include "drvSup.h"
#include "recSup.h"
#include "recGbl.h"
#include "devLib.h"
#include "callback.h"
#include "initHooks.h"

#ifdef EPICS_3_13
static char* epicsStrDup(const char *s) { char* c = (char*)malloc(strlen(s)+1); strcpy(c, s); return c; }
extern DBBASE *pdbbase;
} // extern "C"

#else // !EPICS_3_13

#include "epicsTimer.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsString.h"
#include "registryFunction.h"
#include "iocsh.h"
#include "epicsExport.h"

#if !defined(VERSION_INT) && EPICS_MODIFICATION < 9
// iocshCmd() is missing in iocsh.h (up to R3.14.8.2)
// To build for Windows, you MUST fix iocsh.h.
// Move the declaration below to iocsh.h and rebuild base.
extern "C" epicsShareFunc int epicsShareAPI iocshCmd(const char *command);
#endif

#endif // !EPICS_3_13

#include "StreamCore.h"
#include "StreamError.h"
#include "devStream.h"

#define Z PRINTF_SIZE_T_PREFIX

#if defined(VERSION_INT) || EPICS_MODIFICATION >= 11
#define WITH_IOC_RUN
#endif

// More flags: 0x00FFFFFF used by StreamCore
const unsigned long InDestructor  = 0x0100000;
const unsigned long ValueReceived = 0x0200000;

extern "C" {
long streamReload(const char* recordname);
long streamReportRecord(const char* recordname);
}

class Stream : protected StreamCore
#ifndef EPICS_3_13
    , epicsTimerNotify
#endif
{
    dbCommon* record;
    const struct link *ioLink;
    streamIoFunction readData;
    streamIoFunction writeData;
#ifdef EPICS_3_13
    WDOG_ID timer;
    CALLBACK timeoutCallback;
    SEM_ID mutex;
    SEM_ID initDone;
#else
    epicsTimerQueueActive* timerQueue;
    epicsTimer* timer;
    epicsMutex mutex;
    epicsEvent initDone;
#endif
    int status;
    int convert;
    ssize_t currentValueLength;
    IOSCANPVT ioscanpvt;
    CALLBACK commandCallback;
    CALLBACK processCallback;


#ifdef EPICS_3_13
    static void expire(CALLBACK *pcallback);
#else
// epicsTimerNotify method
    expireStatus expire(const epicsTime&);
#endif

// StreamCore methods
    void protocolFinishHook(ProtocolResult);
    void startTimer(unsigned long timeout);
    bool getFieldAddress(const char* fieldname,
        StreamBuffer& address);
    bool formatValue(const StreamFormat&,
        const void* fieldaddress);
    bool matchValue(const StreamFormat&,
        const void* fieldaddress);
    void lockMutex();
    void releaseMutex();
    bool execute();

// need static wrappers for callbacks
    void executeCommand();
    static void executeCommand(CALLBACK *pcallback) {
        static_cast<Stream*>(pcallback->user)->executeCommand();
    }

    void recordProcessCallback();
    static void recordProcessCallback(CALLBACK *pcallback) {
        static_cast<Stream*>(pcallback->user)->recordProcessCallback();
    }

// Stream Epics methods
    Stream(dbCommon* record, const struct link *ioLink,
        streamIoFunction readData, streamIoFunction writeData);
    ~Stream();
    long initRecord(char* linkstring);
    bool print(format_t *format, va_list ap);
    ssize_t scan(format_t *format, void* pvalue, size_t maxStringSize);
    bool process();
    static void initHook(initHookState);

// device support functions
    friend long streamInitRecord(dbCommon *record, const struct link *ioLink,
        streamIoFunction readData, streamIoFunction writeData);
    friend long streamReadWrite(dbCommon *record);
    friend long streamGetIointInfo(int cmd, dbCommon *record,
        IOSCANPVT *ppvt);
    friend long streamPrintf(dbCommon *record, format_t *format, ...);
    friend ssize_t streamScanfN(dbCommon *record, format_t *format,
        void*, size_t maxStringSize);
    friend long streamReload(const char* recordname);
    friend long streamReportRecord(const char* recordname);

public:
    long priority() { return record->prio; };
    static long report(int interest);
    static long drvInit();
};


// shell functions ///////////////////////////////////////////////////////
extern "C" { // needed for Windows
epicsExportAddress(int, streamDebug);
epicsExportAddress(int, streamError);
epicsExportAddress(int, streamDebugColored);
epicsExportAddress(int, streamErrorDeadTime);
epicsExportAddress(int, streamMsgTimeStamped);
}

// for subroutine record
long streamReloadSub()
{
    return streamReload(NULL);
}

long streamReload(const char* recordname)
{
    Stream* stream;
    long status;
    int oldStreamError = streamError;
    streamError = 1;

    if (!pdbbase) {
        error("No database has been loaded\n");
        streamError = oldStreamError;
        return ERROR;
    }
    debug("streamReload(%s)\n", recordname);
    for (stream = static_cast<Stream*>(Stream::first); stream;
        stream = static_cast<Stream*>(stream->next))
    {
        if (recordname && recordname[0] &&
#ifdef EPICS_3_13
            strcmp(stream->name(), recordname) == 0)
#else
            !epicsStrGlobMatch(stream->name(), recordname))
#endif
            continue;
        // This cancels any running protocol and reloads
        // the protocol file
        status = stream->record->dset->init_record(stream->record);
        if (status == OK || status == DO_NOT_CONVERT)
            printf("%s: Protocol reloaded\n", stream->name());
        else
            error("%s: Protocol reload failed\n", stream->name());
    }
    StreamProtocolParser::free();
    streamError = oldStreamError;
    return OK;
}

long streamSetLogfile(const char* filename)
{
    FILE *oldfile, *newfile = NULL;
    if (filename)
    {
        newfile = fopen(filename, "w");
        if (!newfile)
        {
            fprintf(stderr, "Opening file %s failed: %s\n", filename, strerror(errno));
            return ERROR;
        }
    }
    oldfile = StreamDebugFile;
    StreamDebugFile = newfile;
    if (oldfile) fclose(oldfile);
    return OK;
}

#ifndef EPICS_3_13
static const iocshArg streamReloadArg0 =
    { "recordname", iocshArgString };
static const iocshArg * const streamReloadArgs[] =
    { &streamReloadArg0 };
static const iocshFuncDef streamReloadDef =
    { "streamReload", 1, streamReloadArgs };

void streamReloadFunc (const iocshArgBuf *args)
{
    streamReload(args[0].sval);
}

static const iocshArg streamReportRecordArg0 =
    { "recordname", iocshArgString };
static const iocshArg * const streamReportRecordArgs[] =
    { &streamReportRecordArg0 };
static const iocshFuncDef streamReportRecordDef =
    { "streamReportRecord", 1, streamReportRecordArgs };

void streamReportRecordFunc (const iocshArgBuf *args)
{
    streamReportRecord(args[0].sval);
}

static const iocshArg streamSetLogfileArg0 =
    { "filename", iocshArgString };
static const iocshArg * const streamSetLogfileArgs[] =
    { &streamSetLogfileArg0 };
static const iocshFuncDef streamSetLogfileDef =
    { "streamSetLogfile", 1, streamSetLogfileArgs };

void streamSetLogfileFunc (const iocshArgBuf *args)
{
    streamSetLogfile(args[0].sval);
}

static void streamRegistrar ()
{
    iocshRegister(&streamReloadDef, streamReloadFunc);
    iocshRegister(&streamReportRecordDef, streamReportRecordFunc);
    iocshRegister(&streamSetLogfileDef, streamSetLogfileFunc);
    // make streamReload available for subroutine records
    registryFunctionAdd("streamReload",
        (REGISTRYFUNCTION)streamReloadSub);
    registryFunctionAdd("streamReloadSub",
        (REGISTRYFUNCTION)streamReloadSub);
}

extern "C" {
epicsExportRegistrar(streamRegistrar);
}


#endif // !EPICS_3_13

// driver support ////////////////////////////////////////////////////////

struct drvet stream = {
    2,
    (DRVSUPFUN) Stream::report,
    (DRVSUPFUN) Stream::drvInit
};
extern "C" {
epicsExportAddress(drvet, stream);
}

#ifdef EPICS_3_13
void streamEpicsPrintTimestamp(char* buffer, size_t size)
{
    char* c;
    TS_STAMP tm;
    tsLocalTime (&tm);
    tsStampToText(&tm, TS_TEXT_MMDDYY, buffer);
    c = strchr(buffer,'.');
    if (c) {
        c[4] = 0;
    }
}

static const char* epicsThreadGetNameSelf()
{
    return taskName(0);
}
#else // !EPICS_3_13
void streamEpicsPrintTimestamp(char* buffer, size_t size)
{
    epicsTime tm = epicsTime::getCurrent();
    tm.strftime(buffer, size, "%Y/%m/%d %H:%M:%S.%06f");
}
#endif // !EPICS_3_13

long Stream::
report(int interest)
{
    debug("Stream::report(interest=%d)\n", interest);
    printf("  %s\n", StreamVersion);
    printf("  (C) 1999 Dirk Zimoch (dirk.zimoch@psi.ch)\n");
    if (interest == 100)
    {
        printf("\n%s", license());
        return OK;
    }
    printf("  Use interest level 100 for license information\n");

    printf("  registered bus interfaces:\n");
    StreamBusInterfaceClass interface;
    while (interface)
    {
        printf("    %s\n", interface.name());
        ++interface;
    }

    if (interest < 1) return OK;

    printf("  registered converters:\n");
    StreamFormatConverter* converter;
    int c;
    for (c=0; c < 256; c++)
    {
        converter = StreamFormatConverter::find(c);
        if (converter)
        {
            printf("    %%%c %s\n", c, converter->name());
        }
    }

    Stream* stream;
    printf("  connected records:\n");
    for (stream = static_cast<Stream*>(first); stream;
        stream = static_cast<Stream*>(stream->next))
    {
        if (interest == 2)
        {
            printf("\n%s: %s\n", stream->name(),
                stream->ioLink->value.instio.string);
            stream->printProtocol(stdout);
        }
        else
        {
            printf("    %s: %s\n", stream->name(),
                stream->ioLink->value.instio.string);
            if (interest == 3)
            {
                StreamBuffer buffer;
                stream->printStatus(buffer);
                printf("      %s\n", buffer());
            }
        }
    }
    return OK;
}

long streamReportRecord(const char* recordname)
{
    Stream* stream;
    for (stream = static_cast<Stream*>(Stream::first); stream;
        stream = static_cast<Stream*>(stream->next))
    {
        if (!recordname ||
#ifdef EPICS_3_13
            strcmp(stream->name(), recordname) == 0)
#else
            epicsStrGlobMatch(stream->name(), recordname))
#endif
        {
            printf("%s: %s\n", stream->name(),
                stream->ioLink->value.instio.string);
            StreamBuffer buffer;
            stream->printStatus(buffer);
            printf("%s\n", buffer());
            stream->printProtocol(stdout);
            printf("\n");
        }
    }
    return OK;
}

#if defined(_WIN32) && !defined(_WIN64)
static const char* epicsThreadGetNameSelfWrapper(void)
{
    return epicsThreadGetNameSelf();
}
#define epicsThreadGetNameSelf epicsThreadGetNameSelfWrapper
#endif

long Stream::
drvInit()
{
    char* path;
    debug("drvStreamInit()\n");
    path = getenv("STREAM_PROTOCOL_PATH");
#if defined(vxWorks)
    // for compatibility reasons look for global symbols
    if (!path)
    {
        char* symbol;
        SYM_TYPE type;
        if (symFindByName(sysSymTbl,
            "STREAM_PROTOCOL_PATH", &symbol, &type) == OK)
            path = *(char**)symbol;
        else
        if (symFindByName(sysSymTbl,
            "STREAM_PROTOCOL_DIR", &symbol, &type) == OK)
            path = *(char**)symbol;
    }
#endif
    if (!path)
        fprintf(stderr,
            "drvStreamInit: Warning! STREAM_PROTOCOL_PATH not set.\n");
    else
        StreamProtocolParser::path = path;
    debug("StreamProtocolParser::path = %s\n",
        StreamProtocolParser::path);
    StreamPrintTimestampFunction = streamEpicsPrintTimestamp;
    StreamGetThreadNameFunction = epicsThreadGetNameSelf;
    initHookRegister(initHook);

    return OK;
}

void Stream::
initHook(initHookState state)
{
    Stream* stream;

    switch (state) {
#ifdef WITH_IOC_RUN
        case initHookAtIocRun:
        {
            // re-run @init handlers after restart
            static int inIocInit = 1;
            if (inIocInit)
            {
                // We don't want to run @init twice in iocInit
                inIocInit = 0;
                return;
            }

            for (stream = static_cast<Stream*>(first); stream;
                stream = static_cast<Stream*>(stream->next))
            {
                if (!stream->onInit) continue;
                debug("%s: running @init handler\n", stream->name());
                if (!stream->startProtocol(StartInit))
                {
                    error("%s: @init handler failed.\n",
                        stream->name());
                }
                stream->initDone.wait();
            }
            break;
        }
        case initHookAtIocPause:
        {
            // stop polling I/O Intr
            for (stream = static_cast<Stream*>(first); stream;
                stream = static_cast<Stream*>(stream->next))
            {
                if (stream->record->scan == SCAN_IO_EVENT) {
                    debug("%s: stopping \"I/O Intr\"\n", stream->name());
                    stream->finishProtocol(Stream::Abort);
                }
            }
            break;
        }
        case initHookAfterIocRunning:
#else
        case initHookAfterInterruptAccept:
#endif
        {
            // start polling I/O Intr
            for (stream = static_cast<Stream*>(first); stream;
                stream = static_cast<Stream*>(stream->next))
            {
                if (stream->record->scan == SCAN_IO_EVENT) {
                    debug("%s: starting \"I/O Intr\"\n", stream->name());
                    if (!stream->startProtocol(StartAsync))
                    {
                        error("%s: Can't start \"I/O Intr\" protocol\n",
                            stream->name());
                    }
                }
            }
            break;
        }
        default:
            break;
    }
}

// device support (C interface) //////////////////////////////////////////

long streamInit(int after)
{
    static int oldStreamError;
    if (after)
    {
        static int first = 1;
        if (first)
        {
            // restore error filtering to previous setting
            streamError = oldStreamError;
            StreamProtocolParser::free();
            first = 0;
        }
    }
    else
    {
        static int first = 1;
        if (first)
        {
            // enable errors while reading protocol files
            oldStreamError = streamError;
            streamError = 1;
            first = 0;
        }
    }
    return OK;
}

long streamInitRecord(dbCommon* record, const struct link *ioLink,
    streamIoFunction readData, streamIoFunction writeData)
{
    long status;
    char* linkstring;

    debug("streamInitRecord(%s): SEVR=%d\n", record->name, record->sevr);
    Stream* stream = static_cast<Stream*>(record->dpvt);
    if (!stream)
    {
        // initialize the first time
        debug("streamInitRecord(%s): create new Stream object\n",
            record->name);
        stream = new Stream(record, ioLink, readData, writeData);
        record->dpvt = stream;
    } else {
        // stop any running protocol
        debug("streamInitRecord(%s): stop running protocol\n",
            record->name);
        stream->finishProtocol(Stream::Abort);
    }
    if (ioLink->type != INST_IO)
    {
        error("%s: Wrong I/O link type %s\n", record->name,
            pamaplinkType[ioLink->type].strvalue);
        return S_dev_badInitRet;
    }
    if (!ioLink->value.instio.string[0])
    {
        error("%s: Empty I/O link. "
            "Forgot the leading '@' or confused INP with OUT or link is too long ?\n",
            record->name);
        return S_dev_badInitRet;
    }
    // (re)initialize bus and protocol
    linkstring = epicsStrDup(ioLink->value.instio.string);
    if (!linkstring)
    {
        error("%s: Out of memory", record->name);
        return S_dev_noMemory;
    }
    debug("streamInitRecord(%s): calling initRecord\n",
        record->name);
    status = stream->initRecord(linkstring);
    free(linkstring);
    if (status != OK && status != DO_NOT_CONVERT)
    {
        error("%s: Record initialization failed\n", record->name);
    }
    else if (!stream->ioscanpvt)
    {
        scanIoInit(&stream->ioscanpvt);
    }
    debug("streamInitRecord(%s) done status=%#lx\n", record->name, status);
    return status;
}

long streamReadWrite(dbCommon *record)
{
    Stream* stream = static_cast<Stream*>(record->dpvt);
    if (!stream || stream->status == ERROR)
    {
        (void) recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        return ERROR;
    }
    return stream->process() ? stream->convert : ERROR;
}

long streamGetIointInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    Stream* stream = static_cast<Stream*>(record->dpvt);
    debug("streamGetIointInfo(%s,cmd=%d): stream=%p, ioscanpvt=%p\n",
        record->name, cmd,
        (void*)stream, stream ? stream->ioscanpvt : NULL);
    if (!stream)
    {
        error("streamGetIointInfo called without stream instance\n");
        return ERROR;
    }
    *ppvt = stream->ioscanpvt;
    if (cmd == 0)
    {
#ifdef WITH_IOC_RUN
        if (!interruptAccept) {
            // We will start polling later in initHook.
            debug("streamGetIointInfo(%s): start later...\n",
                record->name);
            return OK;
        }
#endif
        debug("streamGetIointInfo(%s): start protocol\n",
            record->name);
        // SCAN has been set to "I/O Intr"
        if (!stream->startProtocol(Stream::StartAsync))
        {
            error("%s: Can't start \"I/O Intr\" protocol\n",
                record->name);
        }
    }
    else
    {
        // SCAN is no longer "I/O Intr"
        debug("streamGetIointInfo(%s): abort protocol\n",
            record->name);
        stream->finishProtocol(Stream::Abort);
    }
    return OK;
}

long streamPrintf(dbCommon *record, format_t *format, ...)
{
    debug("streamPrintf(%s,format=%%%c)\n",
        record->name, format->priv->conv);
    Stream* stream = static_cast<Stream*>(record->dpvt);
    if (!stream) return ERROR;
    va_list ap;
    va_start(ap, format);
    bool success = stream->print(format, ap);
    va_end(ap);
    return success ? OK : ERROR;
}

ssize_t streamScanfN(dbCommon* record, format_t *format,
    void* value, size_t maxStringSize)
{
    ssize_t size;
    Stream* stream = static_cast<Stream*>(record->dpvt);
    if (!stream) return ERROR;
    size = stream->scan(format, value, maxStringSize);
    return size;
}

// Stream methods ////////////////////////////////////////////////////////

Stream::
Stream(dbCommon* _record, const struct link *ioLink,
    streamIoFunction readData, streamIoFunction writeData)
:record(_record), ioLink(ioLink), readData(readData), writeData(writeData)
{
    streamname = record->name;
#ifdef EPICS_3_13
    timer = wdCreate();
    mutex = semMCreate(SEM_INVERSION_SAFE | SEM_Q_PRIORITY);
    initDone = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
    callbackSetCallback(expire, &timeoutCallback);
    callbackSetUser(this, &timeoutCallback);
#else
    timerQueue = &epicsTimerQueueActive::allocate(true);
    timer = &timerQueue->createTimer();
#endif
    callbackSetCallback(executeCommand, &commandCallback);
    callbackSetUser(this, &commandCallback);
    callbackSetCallback(recordProcessCallback, &processCallback);
    callbackSetUser(this, &processCallback);
    status = ERROR;
    convert = DO_NOT_CONVERT;
    ioscanpvt = NULL;
}

Stream::
~Stream()
{
    lockMutex();
    flags |= InDestructor;;
    debug("~Stream(%s) %p\n", name(), (void*)this);
    if (record->dpvt)
    {
        finishProtocol(Abort);
        debug("~Stream(%s): protocol finished\n", name());
        record->dpvt = NULL;
        debug("~Stream(%s): dpvt cleared\n", name());
    }
#ifdef EPICS_3_13
    wdDelete(timer);
    debug("~Stream(%s): watchdog destroyed\n", name());
#else
    timer->destroy();
    debug("~Stream(%s): timer destroyed\n", name());
    timerQueue->release();
    debug("~Stream(%s): timer queue released\n", name());
#endif
    releaseMutex();
}

long Stream::
initRecord(char* linkstring /* modifiable copy */)
{
    char *filename;
    char *protocol;
    char *busname;
    char *busparam;
    long addr = -1;

    debug("Stream::initRecord %s: parse link string \"%s\"\n", name(), linkstring);

    while (isspace(*linkstring)) linkstring++;
    filename = linkstring;
    while (*linkstring && !isspace(*linkstring)) linkstring++;
    if (*linkstring) *linkstring++ = 0;

    while (isspace(*linkstring)) linkstring++;
    protocol = linkstring;
    while (*linkstring && !isspace(*linkstring) && *linkstring != '(') linkstring++;
    while (isspace(*linkstring)) linkstring++;
    if (*linkstring == '(') {
        int brackets = 0;
        while(*++linkstring) {
            if (*linkstring == '(') brackets++;
            else if (*linkstring == ')') brackets--;
            else if (*linkstring == '\\' && !*++linkstring) break;
            else if (isspace(*linkstring) && brackets < 0) break;
        }
    }
    else if (*linkstring) linkstring--;
    if (*linkstring) *linkstring++ = 0;

    while (isspace(*linkstring)) linkstring++;
    busname = linkstring;
    while (*linkstring && !isspace(*linkstring)) linkstring++;
    if (*linkstring) *linkstring++ = 0;

    if (linkstring) addr = strtol(linkstring, &linkstring, 0);
    while (isspace(*linkstring)) linkstring++;
    busparam = linkstring;

    debug("Stream::initRecord %s: filename=\"%s\" protocol=\"%s\" bus=\"%s\" addr=%ld params=\"%s\"\n",
        name(), filename, protocol, busname, addr, busparam);

    if (!*filename)
    {
        error("%s: Missing protocol file name\n", name());
        return S_dev_badInitRet;
    }
    if (!*protocol)
    {
        error("%s: Missing protocol name\n", name());
        return S_dev_badInitRet;
    }
    if (!*busname)
    {
        error("%s: Missing bus name\n", name());
        return S_dev_badInitRet;
    }

    // attach to bus interface
    debug("Stream::initRecord %s: attachBus(\"%s\", %ld, \"%s\")\n",
        name(), busname, addr, busparam);
    if (!attachBus(busname, addr, busparam))
    {
        error("%s: Can't attach to bus %s %ld\n",
            name(), busname, addr);
        return S_dev_noDevice;
    }

    // parse protocol file
    debug("Stream::initRecord %s: parse(\"%s\", \"%s\")\n",
        name(), filename, protocol);
    if (!parse(filename, protocol))
    {
        error("%s: Protocol parse error\n",
            name());
        return S_dev_noDevice;
    }

    // record is ready to use
    status = NO_ALARM;

    if (ioscanpvt)
    {
        // we have been called by streamReload
        debug("Stream::initRecord %s: re-initialize after streamReload\n",
            name());
        if (record->scan == SCAN_IO_EVENT)
        {
            debug("Stream::initRecord %s: "
                "restarting \"I/O Intr\" after reload\n",
                name());
            if (!startProtocol(StartAsync))
            {
                error("%s: Can't restart \"I/O Intr\" protocol\n",
                    name());
            }
        }
    }
    else
    {
        debug("Stream::initRecord %s: initialize the first time\n",
            name());
    }

    if (!onInit) return DO_NOT_CONVERT; // no @init handler, keep DOL

    // initialize the record from hardware
    if (!startProtocol(StartInit))
    {
        error("%s: Can't start @init handler\n",
            name());
        return ERROR;
    }
    debug("Stream::initRecord %s: waiting for initDone\n",
        name());
#ifdef EPICS_3_13
    semTake(initDone, WAIT_FOREVER);
#else
    initDone.wait();
#endif
    debug("Stream::initRecord %s: initDone\n", name());

    // init run has set status and convert
    if (status != NO_ALARM)
    {
        record->stat = status;
        error("%s: @init handler failed\n",
            name());
        return ERROR;
    }
    debug("Stream::initRecord %s: initialized. %s\n",
        name(), convert == DO_NOT_CONVERT ?
            "convert" : "don't convert");
    return convert;
}

bool Stream::
process()
{
    MutexLock lock(this);
    debug("Stream::process(%s)\n", name());
    if (record->pact || record->scan == SCAN_IO_EVENT)
    {
        record->proc = 0;
        if (status != NO_ALARM)
        {
            debug("Stream::process(%s) error status=%s (%d)\n",
                name(),
                status >= 0 && status < ALARM_NSTATUS ?
                    epicsAlarmConditionStrings[status] : "ERROR",
                status);
            (void) recGblSetSevr(record, status, INVALID_ALARM);
            return false;
        }
        debug("Stream::process(%s) ready. %s\n",
            name(), convert == DO_NOT_CONVERT ?
            "convert" : "don't convert");
        return true;
    }
    if (flags & InDestructor)
    {
        error("%s: Try to process while in stream destructor (try later)\n",
            name());
        (void) recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        return false;
    }
    debug("Stream::process(%s) start\n", name());
    status = NO_ALARM;
    convert = OK;
    if (!startProtocol(record->proc == 2 ? StreamCore::StartInit : StreamCore::StartNormal))
    {
        debug("Stream::process(%s): could not start %sprotocol, status=%s (%d)\n",
            name(), record->proc == 2 ? "@init " : "",
                status >= 0 && status < ALARM_NSTATUS ?
                    epicsAlarmConditionStrings[status] : "ERROR",
            status);
        (void) recGblSetSevr(record, status ? status : UDF_ALARM, INVALID_ALARM);
        record->proc = 0;
        return false;
    }
    debug("Stream::process(%s): protocol started\n", name());
    record->pact = true;
    return true;
}

bool Stream::
print(format_t *format, va_list ap)
{
    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
            return printValue(*format->priv, va_arg(ap, long));
        case DBF_DOUBLE:
            return printValue(*format->priv, va_arg(ap, double));
        case DBF_STRING:
            return printValue(*format->priv, va_arg(ap, char*));
        default:
            error("INTERNAL ERROR (%s): Illegal format type %d\n",
                name(), format->type);
            return false;
    }
}

ssize_t Stream::
scan(format_t *format, void* value, size_t maxStringSize)
{
    // called by streamScanfN

    size_t size = maxStringSize;
    // first remove old value from inputLine (in case we are scanning arrays)
    consumedInput += currentValueLength;
    currentValueLength = 0;
    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
            currentValueLength = scanValue(*format->priv, *(long*)value);
            break;
        case DBF_DOUBLE:
            currentValueLength = scanValue(*format->priv, *(double*)value);
            break;
        case DBF_STRING:
            currentValueLength = scanValue(*format->priv, (char*)value, size);
            break;
        default:
            error("INTERNAL ERROR (%s): Illegal format type %d\n",
                name(), format->type);
            return ERROR;
    }
    debug("Stream::scan() currentValueLength=%" Z "d\n", currentValueLength);
    if (currentValueLength < 0)
    {
        currentValueLength = 0; // important for arrays with less than NELM elements
        return ERROR;
    }
    // Don't remove scanned value from inputLine yet, because
    // we might need the string in a later error message.
    if (format->type == DBF_STRING) return size;
    return OK;
}

// epicsTimerNotify virtual method ///////////////////////////////////////

#ifdef EPICS_3_13
void Stream::
expire(CALLBACK *pcallback)
{
    static_cast<Stream*>(pcallback->user)->timerCallback();
}
#else
epicsTimerNotify::expireStatus Stream::
expire(const epicsTime&)
{
    timerCallback();
    return noRestart;
}
#endif

// StreamCore virtual methods ////////////////////////////////////////////

void Stream::
protocolFinishHook(ProtocolResult result)
{
    debug("Stream::protocolFinishHook(%s, %s)\n",
        name(), toStr(result));
    switch (result)
    {
        case Success:
            status = NO_ALARM;
            if (flags & ValueReceived)
            {
                record->udf = false;
                if (flags & InitRun)
                {
                    // records start with UDF/INVALID,
                    // but now this record has a value
                    record->sevr = NO_ALARM;
                    record->stat = NO_ALARM;
                }
            }
            break;
        case LockTimeout:
        case ReplyTimeout:
            status = TIMEOUT_ALARM;
            break;
        case WriteTimeout:
            status = WRITE_ALARM;
            break;
        case ReadTimeout:
            status = READ_ALARM;
            break;
        case ScanError:
        case FormatError:
            status = CALC_ALARM;
            break;
        case Offline:
            status = COMM_ALARM;
            break;
        case Abort:
        case Fault:
            status = UDF_ALARM;
            if (record->pact || record->scan == SCAN_IO_EVENT)
                error("%s: Protocol aborted\n", name());
            break;
        default:
            status = UDF_ALARM;
            error("INTERNAL ERROR (%s): Illegal protocol result\n",
                name());
            break;

    }
    if ((flags & (InitRun|Aborted)) == InitRun && record->proc != 2)
    {
        debug("Stream::protocolFinishHook %s: signalling init done\n", name());
#ifdef EPICS_3_13
        semGive(initDone);
#else
        initDone.signal();
#endif
        return;
    }

    if (result != Abort && record->scan == SCAN_IO_EVENT)
    {
        // re-enable early input
        flags |= AcceptInput;
    }

    if (record->pact || record->scan == SCAN_IO_EVENT)
    {
        // process record in callback thread to break possible recursion
        callbackSetPriority(priority(), &processCallback);
        callbackRequest(&processCallback);
    }
}

void Stream::
recordProcessCallback()
{
    // process record
    // This will call streamReadWrite.
    debug("recordProcessCallback(%s) processing record\n", name());
    dbScanLock(record);
    ((DEVSUPFUN)record->rset->process)(record);
    dbScanUnlock(record);
    debug("recordProcessCallback(%s) processing record done\n", name());

    if (record->scan == SCAN_IO_EVENT && !(flags & Aborted))
    {
        // restart protocol for next turn
        debug("recordProcessCallback(%s) restart async protocol\n", name());
        if (!startProtocol(Stream::StartAsync))
            error("%s: Can't restart \"I/O Intr\" protocol\n", name());
    }
}

void Stream::
startTimer(unsigned long timeout)
{
    debug("Stream::startTimer(stream=%s, timeout=%lu) = %f seconds\n",
        name(), timeout, timeout * 0.001);
#ifdef EPICS_3_13
    callbackSetPriority(priority(), &timeoutCallback);
    wdStart(timer, (timeout+1)*sysClkRateGet()/1000-1,
        reinterpret_cast<FUNCPTR>(callbackRequest),
        reinterpret_cast<int>(&timeoutCallback));
#else
    timer->start(*this, timeout * 0.001);
#endif
}

bool Stream::
getFieldAddress(const char* fieldname, StreamBuffer& address)
{
    DBADDR dbaddr;
    if (strchr(fieldname, '.') != NULL)
    {
        // record.FIELD (access to other record)
        if (dbNameToAddr(fieldname, &dbaddr) != OK) return false;
    }
    else
    {
        // FIELD in this record or VAL in other record
        StreamBuffer fullname;
        fullname.print("%s.%s", name(), fieldname);
        if (dbNameToAddr(fullname(), &dbaddr) != OK || strcmp(((dbFldDes*)dbaddr.pfldDes)->name, fieldname) != 0)
        {
            // VAL in other record
            fullname.clear().print("%s.VAL", fieldname);
            if (dbNameToAddr(fullname(), &dbaddr) != OK) return false;
        }
    }
    address.append(&dbaddr, sizeof(dbaddr));
    return true;
}

static const unsigned char dbfMapping[] =
    {0, DBF_ULONG, DBF_LONG, DBF_ENUM, DBF_DOUBLE, DBF_STRING};

bool Stream::
formatValue(const StreamFormat& format, const void* fieldaddress)
{
    debug("Stream::formatValue(%s, format=%%%c, fieldaddr=%p\n",
        name(), format.conv, fieldaddress);

// --  TO DO: If SCAN is "I/O Intr" and record has not been processed,  --
// --  do it now to get the latest value (only for output records?)     --

    format_s fmt;
    fmt.type = dbfMapping[format.type];
    fmt.priv = &format;
    if (fieldaddress)
    {
        // Format like "%([record.]field)..." has requested to get value
        // from field of this or other record.
        StreamBuffer fieldBuffer;
        DBADDR* pdbaddr = (DBADDR*)fieldaddress;

        /* Handle time stamps special. %T converter takes double. */
        if (strcmp(((dbFldDes*)pdbaddr->pfldDes)->name, "TIME") == 0)
        {
            double time;

            if (format.type != double_format)
            {
                error ("%s: can only read double values from TIME field\n",
                    name());
                return false;
            }
            if (pdbaddr->precord == record)
            {
                /* if getting time from own record, update timestamp first */
                recGblGetTimeStamp(record);
            }
            /* convert EPICS epoch (1990) to unix epoch (1970) */
            /* we are losing about 3 digits precision here */
            time = pdbaddr->precord->time.secPastEpoch +
                631152000u + pdbaddr->precord->time.nsec * 1e-9;
            debug("Stream::formatValue(%s): read %f from TIME field\n",
                name(), time);
            return printValue(format, time);
        }

        /* convert type to LONG, ENUM, DOUBLE, or STRING */
        long nelem = pdbaddr->no_elements;
        size_t size = nelem * dbValueSize(fmt.type);

        /* print (U)CHAR arrays as string */
        if (format.type == string_format &&
            (pdbaddr->field_type == DBF_CHAR || pdbaddr->field_type == DBF_UCHAR))
        {
            debug("Stream::formatValue(%s): format %s.%s array[%ld] size %d of %s as string\n",
                name(),
                pdbaddr->precord->name,
                ((dbFldDes*)pdbaddr->pfldDes)->name,
                nelem,
                pdbaddr->field_size,
                pamapdbfType[pdbaddr->field_type].strvalue);
            fmt.type = DBF_CHAR;
            size = nelem;
        }

        char* buffer = fieldBuffer.clear().reserve(size);

        if (dbGet(pdbaddr, fmt.type, buffer,
            NULL, &nelem, NULL) != 0)
        {
            error("%s: dbGet(%s.%s, %s) failed\n",
                name(),
                pdbaddr->precord->name,
                ((dbFldDes*)pdbaddr->pfldDes)->name,
                pamapdbfType[dbfMapping[format.type]].strvalue);
            return false;
        }
        debug("Stream::formatValue(%s): got %ld elements\n",
                name(),nelem);

        /* terminate CHAR array as string */
        if (fmt.type == DBF_CHAR)
        {
            if (nelem >= pdbaddr->no_elements) nelem = pdbaddr->no_elements-1;
            buffer[nelem] = 0;
            nelem = 1; /* array is only 1 string */
        }

        long i;
        for (i = 0; i < nelem; i++)
        {
            switch (format.type)
            {
                case enum_format:
                    if (!printValue(format,
                        (long)((epicsUInt16*)buffer)[i]))
                        return false;
                    break;
                case unsigned_format:
                    if (!printValue(format,
                        (long)((epicsUInt32*)buffer)[i]))
                        return false;
                    break;
                case signed_format:
                    if (!printValue(format,
                        (long)((epicsInt32*)buffer)[i]))
                        return false;
                    break;
                case double_format:
                    if (!printValue(format,
                        (double)((epicsFloat64*)buffer)[i]))
                        return false;
                    break;
                case string_format:
                    if (!printValue(format, buffer+MAX_STRING_SIZE*i))
                        return false;
                    break;
                case pseudo_format:
                    error("%s: %%(FIELD) syntax not allowed "
                        "with pseudo formats\n",
                        name());
                    return false;
                default:
                    error("INTERNAL ERROR %s: Illegal format.type=%d\n",
                        name(), format.type);
                    return false;
            }
        }
        return true;
    }
    debug("Stream::formatValue(%s) format=%%%c type=%s\n",
        name(), format.conv, pamapdbfType[fmt.type].strvalue);
    if (!writeData)
    {
        error("%s: No writeData() function provided\n", name());
        return false;
    }
    if (writeData(record, &fmt) == ERROR)
    {
        debug("Stream::formatValue(%s): writeData failed\n", name());
        return false;
    }
    return true;
}

bool Stream::
matchValue(const StreamFormat& format, const void* fieldaddress)
{
    // this function must increase consumedInput
    ssize_t consumed = 0;
    long lval;
    double dval;
    char* buffer;
    int status;
    const char* putfunc;
    format_s fmt;
    size_t stringsize = MAX_STRING_SIZE;

    fmt.type = dbfMapping[format.type];
    fmt.priv = &format;
    if (fieldaddress)
    {
        // Format like "%([record.]field)..." has requested to put value
        // to field of this or other record.
        StreamBuffer fieldBuffer;
        DBADDR* pdbaddr = (DBADDR*)fieldaddress;
        size_t size;
        size_t nord;
        size_t nelem = pdbaddr->no_elements;
        if (format.type == string_format &&
            (pdbaddr->field_type == DBF_CHAR || pdbaddr->field_type == DBF_UCHAR))
        {
            // string to char array
            size = nelem;
        }
        else
            size = nelem * dbValueSize(fmt.type);
        buffer = fieldBuffer.clear().reserve(size);  // maybe write to field directly in case types match?
        for (nord = 0; nord < nelem; nord++)
        {
            debug("Stream::matchValue(%s): buffer before: %s\n",
                name(), fieldBuffer.expand()());
            switch (format.type)
            {
                case unsigned_format:
                {
                    consumed = scanValue(format, lval);
                    if (consumed >= 0) ((epicsUInt32*)buffer)[nord] = lval;
                    debug("Stream::matchValue(%s): %s.%s[%" Z "u] = %lu\n",
                            name(), pdbaddr->precord->name,
                            ((dbFldDes*)pdbaddr->pfldDes)->name,
                            nord, lval);
                    break;
                }
                case signed_format:
                {
                    consumed = scanValue(format, lval);
                    if (consumed >= 0) ((epicsInt32*)buffer)[nord] = lval;
                    debug("Stream::matchValue(%s): %s.%s[%" Z "u] = %li\n",
                            name(), pdbaddr->precord->name,
                            ((dbFldDes*)pdbaddr->pfldDes)->name,
                            nord, lval);
                    break;
                }
                case enum_format:
                {
                    consumed = scanValue(format, lval);
                    if (consumed >= 0)
                        ((epicsUInt16*)buffer)[nord] = (epicsUInt16)lval;
                    debug("Stream::matchValue(%s): %s.%s[%" Z "u] = %li\n",
                            name(), pdbaddr->precord->name,
                            ((dbFldDes*)pdbaddr->pfldDes)->name,
                            nord, lval);
                    break;
                }
                case double_format:
                {
                    consumed = scanValue(format, dval);
                    // Direct assignment to buffer fails with
                    // gcc 3.4.3 for xscale_be
                    // Optimization bug?
                    epicsFloat64 f64=dval;
                    if (consumed >= 0)
                        memcpy(((epicsFloat64*)buffer)+nord,
                            &f64, sizeof(f64));
                    debug("Stream::matchValue(%s): %s.%s[%" Z "u] = %#g %#g\n",
                            name(), pdbaddr->precord->name,
                            ((dbFldDes*)pdbaddr->pfldDes)->name,
                            nord, dval,
                            ((epicsFloat64*)buffer)[nord]);
                    break;
                }
                case string_format:
                {
                    if (pdbaddr->field_type == DBF_CHAR ||
                        pdbaddr->field_type == DBF_UCHAR)
                    {
                        // string to char array
                        stringsize = nelem;
                        consumed = scanValue(format, buffer, stringsize);
                        debug("Stream::matchValue(%s): %s.%s = \"%.*s\"\n",
                                name(), pdbaddr->precord->name,
                                ((dbFldDes*)pdbaddr->pfldDes)->name,
                                (int)consumed, buffer);
                        nord = nelem; // this shortcuts the loop
                    }
                    else
                    {
                        stringsize = MAX_STRING_SIZE;
                        consumed = scanValue(format,
                            buffer+MAX_STRING_SIZE*nord, stringsize);
                        debug("Stream::matchValue(%s): %s.%s[%" Z "u] = \"%.*s\"\n",
                                name(), pdbaddr->precord->name,
                                ((dbFldDes*)pdbaddr->pfldDes)->name,
                                nord, (int)stringsize, buffer+MAX_STRING_SIZE*nord);
                    }
                    break;
                }
                default:
                    error("INTERNAL ERROR: Stream::matchValue %s: "
                        "Illegal format type\n", name());
                    return false;
            }
            debug("Stream::matchValue(%s): buffer after: %s\n",
                name(), fieldBuffer.expand()());
            if (consumed < 0) break;
            consumedInput += consumed;
        }
        if (!nord)
        {
            // scan error: set other record to alarm status
            if (pdbaddr->precord != record)
            {
                (void) recGblSetSevr(pdbaddr->precord,
                    CALC_ALARM, INVALID_ALARM);
                if (!INIT_RUN)
                {
                    // process other record to send alarm monitor
                    dbProcess(pdbaddr->precord);
                }
            }
            return false;
        }
        if (strcmp(((dbFldDes*)pdbaddr->pfldDes)->name, "TIME") == 0)
        {
#ifdef epicsTimeEventDeviceTime
            if (format.type != double_format)
            {
                error ("%s: can only write double values to TIME field\n",
                    name());
                return false;
            }
            /* convert from Unix epoch (1 Jan 1970) to EPICS epoch (1 Jan 1990) */
            dval = dval-631152000u;
            pdbaddr->precord->time.secPastEpoch = (long)dval;
            // rounding: we don't have 9 digits precision
            // in a double of today's number of seconds
            pdbaddr->precord->time.nsec = (long)((dval-(long)dval)*1e6)*1000;
            debug("Stream::matchValue(%s): writing %i.%i to %s.TIME field\n",
                name(),
                pdbaddr->precord->time.secPastEpoch,
                pdbaddr->precord->time.nsec,
                pdbaddr->precord->name);
            pdbaddr->precord->tse = epicsTimeEventDeviceTime;
            return true;
#else
            error ("%s: writing TIME field is not supported "
                "in this EPICS version\n", name());
            return false;
#endif
        }
        if (format.type == string_format &&
            (pdbaddr->field_type == DBF_CHAR ||
            pdbaddr->field_type == DBF_UCHAR))
        {
            /* write strings to [U]CHAR arrays */
            nord = stringsize;
            fmt.type = DBF_CHAR;
        }
        if (pdbaddr->precord == record || INIT_RUN)
        {
            // write into own record, thus don't process it
            // in @init we must not process other record
            putfunc = "dbPut";
            status = dbPut(pdbaddr, fmt.type, buffer, (long)nord);
            if (INIT_RUN && pdbaddr->precord != record)
            {
                // clean error status of other record in @init
                pdbaddr->precord->udf = false;
                pdbaddr->precord->sevr = NO_ALARM;
                pdbaddr->precord->stat = NO_ALARM;
            }
        }
        else
        {
            // write into other record, thus process it
            putfunc = "dbPutField";
            status = dbPutField(pdbaddr, fmt.type,
                buffer, (long)nord);
        }
        debug("Stream::matchValue(%s): %s(%s.%s, %s, %s) status=0x%x\n",
            name(), putfunc,
            pdbaddr->precord->name,
            ((dbFldDes*)pdbaddr->pfldDes)->name,
            pamapdbfType[fmt.type].strvalue,
            fieldBuffer.expand()(),
            status);
        if (status != 0)
        {
            flags &= ~ScanTried;
            switch (fmt.type)
            {
                case DBF_ULONG:
                case DBF_LONG:
                case DBF_ENUM:
                    error("%s: %s(%s.%s, %s, %li, %" Z "u) failed\n",
                        name(), putfunc, pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[fmt.type].strvalue,
                        lval, nord);
                    return false;
                case DBF_DOUBLE:
                    error("%s: %s(%s.%s, %s, %#g, %" Z "u) failed\n",
                        name(), putfunc, pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[fmt.type].strvalue,
                        dval, nord);
                    return false;
                case DBF_STRING:
                case DBF_CHAR:
                    error("%s: %s(%s.%s, %s, \"%.*s\", %" Z "u) failed\n",
                        name(), putfunc, pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[fmt.type].strvalue,
                        (int)consumed, buffer, nord);
                    return false;
                default:
                    return false;
            }
        }
        return true;
    }
    // no fieldaddress (the "normal" case)
    if (!readData)
    {
        error("%s: No readData() function provided\n", name());
        return false;
    }
    currentValueLength = 0;
    convert = readData(record, &fmt); // this will call scan()
    if (convert == ERROR)
    {
        debug("Stream::matchValue(%s): readData failed\n", name());
        if (currentValueLength > 0)
        {
            error("%s: Record does not accept input \"%s%s\"\n",
                name(), inputLine.expand(consumedInput, 19)(),
                inputLine.length()-consumedInput > 20 ? "..." : "");
            flags &= ~ScanTried;
        }
        return false;
    }
    flags |= ValueReceived;
    consumedInput += currentValueLength;
    debug("Stream::matchValue(%s): success, %" Z "d bytes consumed\n", name(), currentValueLength);
    return true;
}

#ifdef EPICS_3_13
// Pass command to vxWorks shell
extern "C" int execute(const char *cmd);
#endif

void Stream::
executeCommand()
{
    int status;
#ifdef EPICS_3_13
    status = ::execute(outputLine());
#else
    status = iocshCmd(outputLine());
#endif
    execCallback(status == OK ? StreamIoSuccess : StreamIoFault);
}

bool Stream::
execute()
{
    callbackSetPriority(priority(), &commandCallback);
    callbackRequest(&commandCallback);
    return true;
}

void Stream::
lockMutex()
{
#ifdef EPICS_3_13
    semTake(mutex, WAIT_FOREVER);
#else
    mutex.lock();
#endif
}

void Stream::
releaseMutex()
{
#ifdef EPICS_3_13
    semGive(mutex);
#else
    mutex.unlock();
#endif
}
