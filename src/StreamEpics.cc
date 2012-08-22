/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to EPICS for StreamDevice.             *
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

#include "devStream.h"
#include "StreamCore.h"
#include "StreamError.h"

#include <epicsVersion.h>
#ifdef BASE_VERSION
#define EPICS_3_13
#endif

#ifdef EPICS_3_13
extern "C" {
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dbStaticLib.h>
#include <drvSup.h>
#include <recSup.h>
#include <recGbl.h>
#include <devLib.h>
#define epicsAlarmGLOBAL
#include <alarm.h>
#include <callback.h>

#ifdef EPICS_3_13

#include <semLib.h>
#include <wdLib.h>
#include <taskLib.h>

extern DBBASE *pdbbase;

} // extern "C"

#else

#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <registryFunction.h>
#include <iocsh.h>

#if EPICS_MODIFICATION<9
extern "C" {
// iocshCmd() is missing in iocsh.h (up to R3.14.8.2)
// To build with win32-x86, you MUST fix iocsh.h.
// Move the declaration below to iocsh.h and rebuild base.
epicsShareFunc int epicsShareAPI iocshCmd(const char *command);
}
#endif

#include <epicsExport.h>

#endif

#if defined(__vxworks) || defined(vxWorks)
#include <symLib.h>
#include <sysSymTbl.h>
#endif

enum MoreFlags {
    // 0x00FFFFFF used by StreamCore
    InDestructor  = 0x0100000,
    ValueReceived = 0x0200000,
    Aborted       = 0x0400000
};

extern "C" void streamExecuteCommand(CALLBACK *pcallback);
extern "C" void streamRecordProcessCallback(CALLBACK *pcallback);
extern "C" long streamReload(char* recordname);

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
    StreamBuffer fieldBuffer;
    int status;
    int convert;
    long currentValueLength;
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
    void protocolStartHook();
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
    friend void streamExecuteCommand(CALLBACK *pcallback);
    friend void streamRecordProcessCallback(CALLBACK *pcallback);

// Stream Epics methods
    Stream(dbCommon* record, const struct link *ioLink,
        streamIoFunction readData, streamIoFunction writeData);
    ~Stream();
    long parseLink(const struct link *ioLink, char* filename, char* protocol,
        char* busname, int* addr, char* busparam);
    long initRecord(const char* filename, const char* protocol,
        const char* busname, int addr, const char* busparam);
    bool print(format_t *format, va_list ap);
    bool scan(format_t *format, void* pvalue, size_t maxStringSize);
    bool process();

// device support functions
    friend long streamInitRecord(dbCommon *record, const struct link *ioLink,
        streamIoFunction readData, streamIoFunction writeData);
    friend long streamReadWrite(dbCommon *record);
    friend long streamGetIointInfo(int cmd, dbCommon *record,
        IOSCANPVT *ppvt);
    friend long streamPrintf(dbCommon *record, format_t *format, ...);
    friend long streamScanfN(dbCommon *record, format_t *format,
        void*, size_t maxStringSize);
    friend long streamReload(char* recordname);

public:
    long priority() { return record->prio; };
    static long report(int interest);
    static long drvInit();
};


// shell functions ///////////////////////////////////////////////////////
#ifndef EPICS_3_13
extern "C" {
epicsExportAddress(int, streamDebug);
}
#endif

// for subroutine record
extern "C" long streamReloadSub()
{
    return streamReload(NULL);
}

extern "C" long streamReload(char* recordname)
{
    DBENTRY	dbentry;
    dbCommon*   record;
    long status;

    if(!pdbbase) {
        error("No database has been loaded\n");
        return ERROR;
    }
    debug("streamReload(%s)\n", recordname);
    dbInitEntry(pdbbase,&dbentry);
    for (status = dbFirstRecordType(&dbentry); status == OK;
        status = dbNextRecordType(&dbentry))
    {
        for (status = dbFirstRecord(&dbentry); status == OK;
            status = dbNextRecord(&dbentry))
        {
            char* value;
            if (dbFindField(&dbentry, "DTYP") != OK)
                continue;
            if ((value = dbGetString(&dbentry)) == NULL)
                continue;
            if (strcmp(value, "stream") != 0)
                continue;
            record=(dbCommon*)dbentry.precnode->precord;
            if (recordname && strcmp(recordname, record->name) != 0)
                continue;

            // This cancels any running protocol and reloads
            // the protocol file
            status = record->dset->init_record(record);
            if (status == OK || status == DO_NOT_CONVERT)
            {
                printf("%s: Protocol reloaded\n", record->name);
            }
            else
            {
                error("%s: Protocol reload failed\n", record->name);
            }
        }
    }
    dbFinishEntry(&dbentry);
    StreamProtocolParser::free();
    return OK;
}

#ifndef EPICS_3_13
static const iocshArg streamReloadArg0 =
    { "recordname", iocshArgString };
static const iocshArg * const streamReloadArgs[] =
    { &streamReloadArg0 };
static const iocshFuncDef reloadDef =
    { "streamReload", 1, streamReloadArgs };

extern "C" void streamReloadFunc (const iocshArgBuf *args)
{
    streamReload(args[0].sval);
}

static void streamRegistrar ()
{
    iocshRegister(&reloadDef, streamReloadFunc);
    // make streamReload available for subroutine records
    registryFunctionAdd("streamReload",
        (REGISTRYFUNCTION)streamReloadSub);
    registryFunctionAdd("streamReloadSub",
        (REGISTRYFUNCTION)streamReloadSub);
}

extern "C" {
epicsExportRegistrar(streamRegistrar);
}
#endif

// driver support ////////////////////////////////////////////////////////

struct stream_drvsup {
    long number;
    long (*report)(int);
    DRVSUPFUN init;
} stream = {
    2,
    Stream::report,
    Stream::drvInit
};

#ifndef EPICS_3_13
extern "C" {
epicsExportAddress(drvet, stream);
}
#endif

#ifdef EPICS_3_13
void streamEpicsPrintTimestamp(char* buffer, int size)
{
    int tlen;
    char* c;
    TS_STAMP tm;
    tsLocalTime (&tm);
    tsStampToText(&tm, TS_TEXT_MMDDYY, buffer);
    c = strchr(buffer,'.');
    if (c) {
        c[4] = 0;
    }
    tlen = strlen(buffer);
    sprintf(buffer+tlen, " %.*s", size-tlen-2, taskName(0));
}
#else
void streamEpicsPrintTimestamp(char* buffer, int size)
{
    int tlen;
    epicsTime tm = epicsTime::getCurrent();
    tlen = tm.strftime(buffer, size, "%Y/%m/%d %H:%M:%S.%06f");
    sprintf(buffer+tlen, " %.*s", size-tlen-2, epicsThreadGetNameSelf());
}
#endif

long Stream::
report(int interest)
{
    debug("Stream::report(interest=%d)\n", interest);
    printf("  %s\n", StreamVersion);

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

    Stream* pstream;
    printf("  connected records:\n");
    for (pstream = static_cast<Stream*>(first); pstream;
        pstream = static_cast<Stream*>(pstream->next))
    {
        if (interest == 2)
        {
            printf("\n%s: %s\n", pstream->name(),
                pstream->ioLink->value.instio.string);
            pstream->printProtocol();
        }
        else
        {
            printf("    %s: %s\n", pstream->name(),
                pstream->ioLink->value.instio.string);
        }
    }
    return OK;
}

long Stream::
drvInit()
{
    char* path;
    debug("drvStreamInit()\n");
    path = getenv("STREAM_PROTOCOL_PATH");
#if defined(__vxworks) || defined(vxWorks)
    // for compatibility reasons look for global symbols
    if (!path)
    {
        char* symbol;
        SYM_TYPE type;
        if (symFindByName(sysSymTbl,
            "STREAM_PROTOCOL_PATH", &symbol, &type) == OK)
        {
            path = *(char**)symbol;
        }
        else
        if (symFindByName(sysSymTbl,
            "STREAM_PROTOCOL_DIR", &symbol, &type) == OK)
        {
            path = *(char**)symbol;
        }
    }
#endif
    if (!path)
    {
        fprintf(stderr,
            "drvStreamInit: Warning! STREAM_PROTOCOL_PATH not set. "
            "Defaults to \"%s\"\n", StreamProtocolParser::path);
    }
    else
    {
        StreamProtocolParser::path = path;
    }
    debug("StreamProtocolParser::path = %s\n",
        StreamProtocolParser::path);
    StreamPrintTimestampFunction = streamEpicsPrintTimestamp;
    return OK;
}

// device support (C interface) //////////////////////////////////////////

long streamInit(int after)
{
    if (after)
    {
        StreamProtocolParser::free();
    }
    return OK;
}

long streamInitRecord(dbCommon* record, const struct link *ioLink,
    streamIoFunction readData, streamIoFunction writeData)
{
    char filename[80];
    char protocol[80];
    char busname[80];
    int addr = -1;
    char busparam[80];
    memset(busparam, 0 ,sizeof(busparam));

    debug("streamInitRecord(%s): SEVR=%d\n", record->name, record->sevr);
    Stream* pstream = (Stream*)record->dpvt;
    if (!pstream)
    {
        // initialize the first time
        pstream = new Stream(record, ioLink, readData, writeData);
        record->dpvt = pstream;
    } else {
        // stop any running protocol
        pstream->finishProtocol(Stream::Abort);
    }
    // scan the i/o link
    pstream->parseLink(ioLink, filename, protocol,
        busname, &addr, busparam);
    // (re)initialize bus and protocol
    long status = pstream->initRecord(filename, protocol,
        busname, addr, busparam);
    if (status != OK && status != DO_NOT_CONVERT)
    {
        error("%s: Record initialization failed\n", record->name);
    }
    if (!pstream->ioscanpvt)
    {
        scanIoInit(&pstream->ioscanpvt);
    }
    return status;
}

long streamReadWrite(dbCommon *record)
{
    Stream* pstream = (Stream*)record->dpvt;
    if (!pstream || pstream->status == ERROR)
    {
        (void) recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        error("%s: Record not initialised correctly\n", record->name);
        return ERROR;
    }
    return pstream->process() ? pstream->convert : ERROR;
}

long streamGetIointInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    Stream* pstream = (Stream*)record->dpvt;
    debug("streamGetIointInfo(%s,cmd=%d): pstream=%p, ioscanpvt=%p\n",
        record->name, cmd,
        (void*)pstream, pstream ? pstream->ioscanpvt : NULL);
    if (!pstream)
    {
        error("streamGetIointInfo called without stream instance\n");
        return ERROR;
    }
    *ppvt = pstream->ioscanpvt;
    if (cmd == 0)
    {
        debug("streamGetIointInfo: starting protocol\n");
        // SCAN has been set to "I/O Intr"
        if (!pstream->startProtocol(Stream::StartAsync))
        {
            error("%s: Can't start \"I/O Intr\" protocol\n",
                record->name);
        }
    }
    else
    {
        // SCAN is no longer "I/O Intr"
        pstream->finishProtocol(Stream::Abort);
    }
    return OK;
}

long streamPrintf(dbCommon *record, format_t *format, ...)
{
    debug("streamPrintf(%s,format=%%%c)\n",
        record->name, format->priv->conv);
    Stream* pstream = (Stream*)record->dpvt;
    if (!pstream) return ERROR;
    va_list ap;
    va_start(ap, format);
    bool success = pstream->print(format, ap);
    va_end(ap);
    return success ? OK : ERROR;
}

long streamScanfN(dbCommon* record, format_t *format,
    void* value, size_t maxStringSize)
{
    debug("streamScanfN(%s,format=%%%c,maxStringSize=%ld)\n",
        record->name, format->priv->conv, (long)maxStringSize);
    Stream* pstream = (Stream*)record->dpvt;
    if (!pstream) return ERROR;
    if (!pstream->scan(format, value, maxStringSize))
    {
        return ERROR;
    }
#ifndef NO_TEMPORARY
    debug("streamScanfN(%s) success, value=\"%s\"\n",
        record->name, StreamBuffer((char*)value).expand()());
#endif
    return OK;
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
    callbackSetCallback(streamExecuteCommand, &commandCallback);
    callbackSetUser(this, &commandCallback);
    callbackSetCallback(streamRecordProcessCallback, &processCallback);
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
parseLink(const struct link *ioLink, char* filename,
    char* protocol, char* busname, int* addr, char* busparam)
{
    // parse link parameters: filename protocol busname addr busparam
    int n;
    if (ioLink->type != INST_IO)
    {
        error("%s: Wrong I/O link type %s\n", name(),
            pamaplinkType[ioLink->type].strvalue);
        return S_dev_badInitRet;
    }
    int items = sscanf(ioLink->value.instio.string, "%s%s%s%n%i%n",
        filename, protocol, busname, &n, addr, &n);
    if (items <= 0)
    {
        error("%s: Empty I/O link. "
            "Forgot the leading '@' or confused INP with OUT ?\n",
            name());
        return S_dev_badInitRet;
    }
    if (items < 3)
    {
        error("%s: Wrong I/O link format\n"
            "  expect \"@file protocol bus addr params\"\n"
            "  in \"@%s\"\n", name(),
            ioLink->value.instio.string);
        return S_dev_badInitRet;
    }
    while (isspace((unsigned char)ioLink->value.instio.string[n])) n++;
    strcpy (busparam, ioLink->value.constantStr+n);
    return OK;
}

long Stream::
initRecord(const char* filename, const char* protocol,
    const char* busname, int addr, const char* busparam)
{
    // It is safe to call this function again with different arguments

    // attach to bus interface
    if (!attachBus(busname, addr, busparam))
    {
        error("%s: Can't attach to bus %s %d\n",
            name(), busname, addr);
        return S_dev_noDevice;
    }

    // parse protocol file
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
        debug("Stream::initRecord %s: initialize after streamReload\n",
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
        return OK;
    }

    debug("Stream::initRecord %s: initialize the first time\n",
        name());

    if (!onInit) return DO_NOT_CONVERT; // no @init handler, keep DOL

    // initialize the record from hardware
    if (!startProtocol(StartInit))
    {
        error("%s: Can't start init run\n",
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
    debug("Stream::initRecord %s: initDone\n",
        name());

    // init run has set status and convert
    if (status != NO_ALARM)
    {
        record->stat = status;
        error("%s: @init handler failed\n",
            name());
        return ERROR;
    }
    debug("Stream::initRecord %s: initialized. convert=%d\n",
        name(), convert);
    return convert;
}

bool Stream::
process()
{
    MutexLock lock(this);
    if (record->pact || record->scan == SCAN_IO_EVENT)
    {
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
            name(), convert==2 ?
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
    record->pact = true;
    if (!startProtocol(StreamCore::StartNormal))
    {
        debug("Stream::process(%s): could not start, status=%d\n",
            name(), status);
        (void) recGblSetSevr(record, status, INVALID_ALARM);
        record->pact = false;
        return false;
    }
    debug("Stream::process(%s): protocol started\n", name());
    return true;
}

bool Stream::
print(format_t *format, va_list ap)
{
    long lval;
    double dval;
    char* sval;
    switch (format->type)
    {
        case DBF_ENUM:
        case DBF_LONG:
            lval = va_arg(ap, long);
            return printValue(*format->priv, lval);
        case DBF_DOUBLE:
            dval = va_arg(ap, double);
            return printValue(*format->priv, dval);
        case DBF_STRING:
            sval = va_arg(ap, char*);
            return printValue(*format->priv, sval);
    }
    error("INTERNAL ERROR (%s): Illegal format type\n", name());
    return false;
}

bool Stream::
scan(format_t *format, void* value, size_t maxStringSize)
{
    // called by streamScanfN
    long* lptr;
    double* dptr;
    char* sptr;

    // first remove old value from inputLine (if we are scanning arrays)
    consumedInput += currentValueLength;
    currentValueLength = 0;
    switch (format->type)
    {
        case DBF_LONG:
        case DBF_ENUM:
            lptr = (long*)value;
            currentValueLength = scanValue(*format->priv, *lptr);
            break;
        case DBF_DOUBLE:
            dptr = (double*)value;
            currentValueLength = scanValue(*format->priv, *dptr);
            break;
        case DBF_STRING:
            sptr = (char*)value;
            currentValueLength  = scanValue(*format->priv, sptr,
                maxStringSize);
            break;
        default:
            error("INTERNAL ERROR (%s): Illegal format type\n", name());
            return false;
    }
    if (currentValueLength < 0) 
    {
        currentValueLength = 0;
        return false;
    }
    // Don't remove scanned value from inputLine yet, because
    // we might need the string in a later error message.
    return true;
}

// epicsTimerNotify virtual method ///////////////////////////////////////

#ifdef EPICS_3_13
void Stream::
expire(CALLBACK *pcallback)
{
    Stream* pstream = static_cast<Stream*>(pcallback->user);
    pstream->timerCallback();
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
protocolStartHook()
{
    flags &= ~Aborted;
}

void Stream::
protocolFinishHook(ProtocolResult result)
{
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
        case Abort:
            flags |= Aborted;
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
    if (flags & InitRun)
    {
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

void streamRecordProcessCallback(CALLBACK *pcallback)
{
    Stream* pstream = static_cast<Stream*>(pcallback->user);
    dbCommon* record = pstream->record;

    // process record
    // This will call streamReadWrite.
    debug("streamRecordProcessCallback(%s) processing record\n",
            pstream->name());
    dbScanLock(record);
    ((DEVSUPFUN)record->rset->process)(record);
    dbScanUnlock(record);
    debug("streamRecordProcessCallback(%s) processing record done\n",
            pstream->name());

    if (record->scan == SCAN_IO_EVENT && !(pstream->flags & Aborted))
    {
        // restart protocol for next turn
        debug("streamRecordProcessCallback(%s) restart async protocol\n",
            pstream->name());
        if (!pstream->startProtocol(Stream::StartAsync))
        {
            error("%s: Can't restart \"I/O Intr\" protocol\n",
                pstream->name());
        }
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
        char fullname[PVNAME_SZ + 1];
        sprintf(fullname, "%s.%s", name(), fieldname);
        if (dbNameToAddr(fullname, &dbaddr) != OK)
        {
            // VAL in other record
            sprintf(fullname, "%s.VAL", fieldname);
            if (dbNameToAddr(fullname, &dbaddr) != OK) return false;
        }
    }
    address.append(&dbaddr, sizeof(dbaddr));
    return true;
}

static const unsigned char dbfMapping[] =
    {0, DBF_LONG, DBF_ENUM, DBF_DOUBLE, DBF_STRING};
static const short typeSize[] =
    {0, sizeof(epicsInt32), sizeof(epicsUInt16),
        sizeof(epicsFloat64), MAX_STRING_SIZE};

bool Stream::
formatValue(const StreamFormat& format, const void* fieldaddress)
{
    debug("Stream::formatValue(%s, format=%%%c, fieldaddr=%p\n",
        name(), format.conv, fieldaddress);

// --  TO DO: If SCAN is "I/O Intr" and record has not been processed,  --
// --  do it now to get the latest value (only for output records?)     --

    if (fieldaddress)
    {
        // Format like "%([record.]field)..." has requested to get value
        // from field of this or other record.
        DBADDR* pdbaddr = (DBADDR*)fieldaddress;
        long i;
        long nelem = pdbaddr->no_elements;
        size_t size = nelem * typeSize[format.type];
        char* buffer = fieldBuffer.clear().reserve(size);
        
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
            time = pdbaddr->precord->time.secPastEpoch +
                631152000u + pdbaddr->precord->time.nsec * 1e-9;
            debug("Stream::formatValue(%s): read %f from TIME field\n",
                name(), time);
            return printValue(format, time);
        }

        if (dbGet(pdbaddr, dbfMapping[format.type], buffer,
            NULL, &nelem, NULL) != 0)
        {
            error("%s: dbGet(%s.%s, %s) failed\n",
                name(),
                pdbaddr->precord->name,
                ((dbFldDes*)pdbaddr->pfldDes)->name,
                pamapdbfType[dbfMapping[format.type]].strvalue);
            return false;
        }
        for (i = 0; i < nelem; i++)
        {
            switch (format.type)
            {
                case enum_format:
                    if (!printValue(format,
                        (long)((epicsUInt16*)buffer)[i]))
                        return false;
                    break;
                case long_format:
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
                default:
                    error("INTERNAL ERROR %s: Illegal format.type=%d\n",
                        name(), format.type);
                    return false;
            }
        }
        return true;
    }
    format_s fmt;
    fmt.type = dbfMapping[format.type];
    fmt.priv = &format;
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
    long consumed;
    long lval;
    double dval;
    char* buffer;
    int status;
    const char* putfunc;

    if (fieldaddress)
    {
        // Format like "%([record.]field)..." has requested to put value
        // to field of this or other record.
        DBADDR* pdbaddr = (DBADDR*)fieldaddress;
        long nord;
        long nelem = pdbaddr->no_elements;
        size_t size = nelem * typeSize[format.type];
        buffer = fieldBuffer.clear().reserve(size);
        for (nord = 0; nord < nelem; nord++)
        {
            debug("Stream::matchValue(%s): buffer before: %s\n",
                name(), fieldBuffer.expand()());
            switch (format.type)
            {
                case long_format:
                {
                    consumed = scanValue(format, lval);
                    if (consumed >= 0) ((epicsInt32*)buffer)[nord] = lval;
                    debug("Stream::matchValue(%s): %s[%li] = %li\n",
                            name(), pdbaddr->precord->name, nord, lval);
                    break;
                }
                case enum_format:
                {
                    consumed = scanValue(format, lval);
                    if (consumed >= 0)
                        ((epicsUInt16*)buffer)[nord] = (epicsUInt16)lval;
                    debug("Stream::matchValue(%s): %s[%li] = %li\n",
                            name(), pdbaddr->precord->name, nord, lval);
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
                    debug("Stream::matchValue(%s): %s[%li] = %#g %#g\n",
                            name(), pdbaddr->precord->name, nord, dval,
                            ((epicsFloat64*)buffer)[nord]);
                    break;
                }
                case string_format:
                {
                    consumed = scanValue(format,
                        buffer+MAX_STRING_SIZE*nord, MAX_STRING_SIZE);
                    debug("Stream::matchValue(%s): %s[%li] = \"%.*s\"\n",
                            name(), pdbaddr->precord->name, nord,
                            MAX_STRING_SIZE, buffer+MAX_STRING_SIZE*nord);
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
            dval = dval-631152000u;
            pdbaddr->precord->time.secPastEpoch = (long)dval;
            // rouding: we don't have 9 digits precision
            // in a double of today's number of seconds
            pdbaddr->precord->time.nsec = (long)((dval-(long)dval)*1e6)*1000;
            debug("Stream::matchValue(%s): writing %i.%i to TIME field\n",
                name(),
                pdbaddr->precord->time.secPastEpoch,
                pdbaddr->precord->time.nsec);
            pdbaddr->precord->tse = epicsTimeEventDeviceTime;
            return true;
#else
            error ("%s: writing TIME field is not supported "
                "in this EPICS version\n", name());
            return false;
#endif
        }
        
        if (pdbaddr->precord == record || INIT_RUN)
        {
            // write into own record, thus don't process it
            // in @init we must not process other record
            debug("Stream::matchValue(%s): dbPut(%s.%s,%s)\n",
                name(),
                pdbaddr->precord->name,
                ((dbFldDes*)pdbaddr->pfldDes)->name,
                fieldBuffer.expand()());
            putfunc = "dbPut";
            status = dbPut(pdbaddr, dbfMapping[format.type], buffer, nord);
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
            debug("Stream::matchValue(%s): dbPutField(%s.%s,%s)\n",
                name(),
                pdbaddr->precord->name,
                ((dbFldDes*)pdbaddr->pfldDes)->name,
                fieldBuffer.expand()());
            putfunc = "dbPutField";
            status = dbPutField(pdbaddr, dbfMapping[format.type],
                buffer, nord);
        }
        if (status != 0)
        {
            flags &= ~ScanTried;
            switch (format.type)
            {
                case long_format:
                case enum_format:
                    error("%s: %s(%s.%s, %s, %li) failed\n",
                        putfunc, name(), pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[dbfMapping[format.type]].strvalue,
                        lval);
                    return false;
                case double_format:
                    error("%s: %s(%s.%s, %s, %#g) failed\n",
                        putfunc, name(), pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[dbfMapping[format.type]].strvalue,
                        dval);
                    return false;
                case string_format:
                    error("%s: %s(%s.%s, %s, \"%s\") failed\n",
                        putfunc, name(), pdbaddr->precord->name,
                        ((dbFldDes*)pdbaddr->pfldDes)->name,
                        pamapdbfType[dbfMapping[format.type]].strvalue,
                        buffer);
                    return false;
                default:
                    return false;
            }
        }
        return true;
    }
    // no fieldaddress (the "normal" case)
    format_s fmt;
    fmt.type = dbfMapping[format.type];
    fmt.priv = &format;
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
    return true;
}

#ifdef EPICS_3_13
// Pass command to vxWorks shell
extern "C" int execute(const char *cmd);

void streamExecuteCommand(CALLBACK *pcallback)
{
    Stream* pstream = static_cast<Stream*>(pcallback->user);

    if (execute(pstream->outputLine()) != OK)
    {
        pstream->execCallback(StreamIoFault);
    }
    else
    {
        pstream->execCallback(StreamIoSuccess);
    }
}
#else
// Pass command to iocsh
void streamExecuteCommand(CALLBACK *pcallback)
{
    Stream* pstream = static_cast<Stream*>(pcallback->user);

    if (iocshCmd(pstream->outputLine()) != OK)
    {
        pstream->execCallback(StreamIoFault);
    }
    else
    {
        pstream->execCallback(StreamIoSuccess);
    }
}
#endif

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
