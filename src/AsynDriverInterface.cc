/*************************************************************************
* This is the interface to asyn driver for StreamDevice.
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

#ifdef EPICS_3_13
#include <assert.h>
#include <wdLib.h>
#include <sysLib.h>
extern "C" {
#include "callback.h"
}
#else
#include "epicsAssert.h"
#include "epicsTime.h"
#include "epicsTimer.h"
#include "iocsh.h"
#endif

#include "asynDriver.h"
#include "asynOctet.h"
#include "asynInt32.h"
#include "asynUInt32Digital.h"
#include "asynGpibDriver.h"

#include "StreamBusInterface.h"
#include "StreamError.h"
#include "StreamBuffer.h"
#include "devStream.h"
#include "MacroMagic.h"

#define Z PRINTF_SIZE_T_PREFIX

/* How things are implemented:

synchonous io:

lockRequest()
    pasynManager->blockProcessCallback(),
    pasynCommon->connect() only if
        lockTimeout is unlimited (0) and port is not yet connected
    pasynManager->queueRequest()
    when request is handled
        lockCallback()
    if request fails
        lockCallback(StreamIoTimeout)

writeRequest()
    pasynManager->queueRequest()
    when request is handled
        pasynOctet->flush()
        pasynOctet->write()
        if write() times out
            writeCallback(StreamIoTimeout)
        if write fails otherwise
            writeCallback(StreamIoFault)
        if write succeeds and all bytes have been written
            writeCallback()
        if not all bytes can be written
            pasynManager->queueRequest() to write next part
    if request fails
        writeCallback(StreamIoTimeout)

readRequest()
    pasynManager->queueRequest()
    when request is handled
        optionally: pasynOctet->setInputEos()
        pasynOctet->read()
        if time out at the first byte
            readCallback(StreamIoNoReply)
        if time out at next bytes
            readCallback(StreamIoTimeout,buffer,received)
        if other fault
            readCallback(StreamIoFault,buffer,received)
        if success and reason is ASYN_EOM_END or ASYN_EOM_EOS
            readCallback(StreamIoEnd,buffer,received)
        if success and no end detected
            readCallback(StreamIoSuccess,buffer,received)
        if readCallback() has returned true (wants more input)
            loop back to the pasynOctet->read() call
    if request fails
        readCallback(StreamIoFault)

unlock()
    call pasynManager->unblockProcessCallback()

asynchonous input support ("I/O Intr"):

pasynOctet->registerInterruptUser(...,intrCallbackOctet,...) is called
initially. This calls intrCallbackOctet() every time input is received,
but only if someone else is doing a read. Thus, if nobody reads
something, arrange for periodical read polls.

*/

class AsynDriverInterface : StreamBusInterface
#ifndef EPICS_3_13
 , epicsTimerNotify
#endif
{
    ENUM (IoAction,
        None, Lock, Write, Read, AsyncRead, AsyncReadMore,
        ReceiveEvent, Connect, Disconnect);

    inline static const char* toStr(asynStatus status)
    {
        const char* asynStatusStr[] = {
            "asynSuccess", "asynTimeout", "asynOverflow", "asynError",
            "asynDisconnected", "asynDisabled"};
        return status > 5 ? "unknown" : asynStatusStr[status];
    };

    inline static const char* toStr(asynException exception)
    {
        const char* exceptionStr[] = {"Connect", "Enable", "AutoConnect",
            "TraceMask", "TraceIOMask", "TraceInfoMask",
            "TraceFile", "TraceIOTruncateSize"};
        return exception > 7 ? "unknown" : exceptionStr[exception];
    }

    inline static const char* eomReasonToStr(int reason)
    {
        const char* eomReasonStr[] = {
            "NONE", "CNT", "EOS", "CNT+EOS", "END",
            "CNT+END", "EOS+END", "CNT+EOS+END"};
        return eomReasonStr[reason & 7];
    }

    asynUser* pasynUser;
    asynCommon* pasynCommon;
    void* pvtCommon;
    asynOctet* pasynOctet;
    void* pvtOctet;
    void* intrPvtOctet;
    asynInt32* pasynInt32;
    void* pvtInt32;
    void* intrPvtInt32;
    asynUInt32Digital* pasynUInt32;
    void* pvtUInt32;
    void* intrPvtUInt32;
    asynGpib* pasynGpib;
    void* pvtGpib;
    int connected;
    IoAction ioAction;
    double lockTimeout;
    double writeTimeout;
    double readTimeout;
    double replyTimeout;
    ssize_t expectedLength;
    unsigned long eventMask;
    unsigned long receivedEvent;
    StreamBuffer inputBuffer;
    const char* outputBuffer;
    size_t outputSize;
    size_t peeksize;
#ifdef EPICS_3_13
    WDOG_ID timer;
    CALLBACK timeoutCallback;
#else
    epicsTimerQueueActive* timerQueue;
    epicsTimer* timer;
#endif
    asynStatus previousAsynStatus;

    AsynDriverInterface(Client* client);
    ~AsynDriverInterface();

    // StreamBusInterface methods
    bool lockRequest(unsigned long lockTimeout_ms);
    bool unlock();
    bool writeRequest(const void* output, size_t size,
        unsigned long writeTimeout_ms);
    bool readRequest(unsigned long replyTimeout_ms,
        unsigned long readTimeout_ms, ssize_t expectedLength, bool async);
    bool acceptEvent(unsigned long mask, unsigned long replytimeout_ms);
    bool supportsEvent();
    bool supportsAsyncRead();
    bool connectRequest(unsigned long connecttimeout_ms);
    bool disconnectRequest();
    void finish();

#ifdef EPICS_3_13
    static void expire(CALLBACK *pcallback);
#else
    // epicsTimerNotify methods
    epicsTimerNotify::expireStatus expire(const epicsTime &);
#endif

    // local methods
    void timerExpired();
    bool connectToBus(const char *portname, int addr);
    void lockHandler();
    void writeHandler();
    void readHandler();
    void connectHandler();
    void disconnectHandler();
    bool connectToAsynPort();
    void asynReadHandler(const char *data, size_t numchars, int eomReason);
    asynQueuePriority priority() {
        return static_cast<asynQueuePriority>
            (StreamBusInterface::priority());
    }
    void startTimer(double timeout) {
#ifdef EPICS_3_13
        callbackSetPriority(priority(), &timeoutCallback);
        wdStart(timer, (int)((timeout+1)*sysClkRateGet())-1,
            reinterpret_cast<FUNCPTR>(callbackRequest),
            reinterpret_cast<int>(&timeoutCallback));
#else
        timer->start(*this, timeout
            +epicsThreadSleepQuantum()*0.5
        );
#endif
    }
    void cancelTimer() {
#ifdef EPICS_3_13
        wdCancel(timer);
#else
        timer->cancel();
#endif
    }
    void reportAsynStatus(asynStatus status, const char *name);

    // asynUser callback functions (need some static wrappers here)
    void handleRequest();
    static void handleRequest(asynUser *pasynUser) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->handleRequest();
    }

    void handleTimeout();
    static void handleTimeout(asynUser *pasynUser) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->handleTimeout();
    }

    void intrCallbackOctet(char *data, size_t numchars, int eomReason);
    static void intrCallbackOctet(void *pvt, asynUser *pasynUser,
        char *data, size_t numchars, int eomReason) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->intrCallbackOctet(data, numchars, eomReason);
    }

    void intrCallbackInt32(epicsInt32 data);
    static void intrCallbackInt32(void *pvt, asynUser *pasynUser,
        epicsInt32 data) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->intrCallbackInt32(data);
    }

    void intrCallbackUInt32(epicsUInt32 data);
    static void intrCallbackUInt32(void *pvt, asynUser *pasynUser, epicsUInt32 data) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->intrCallbackUInt32(data);
    }

    void exceptionHandler(asynException exception);
    static void exceptionHandler(asynUser *pasynUser, asynException exception) {
        static_cast<AsynDriverInterface*>(pasynUser->userPvt)->exceptionHandler(exception);
    }

public:
    // static creator method
    static StreamBusInterface* getBusInterface(Client* client,
        const char* portname, int addr, const char* param);
};

RegisterStreamBusInterface(AsynDriverInterface);

AsynDriverInterface::
AsynDriverInterface(Client* client) : StreamBusInterface(client)
{
    debug ("AsynDriverInterface(%s)\n", client->name());
    pasynCommon = NULL;
    pasynOctet = NULL;
    intrPvtOctet = NULL;
    pasynInt32 = NULL;
    intrPvtInt32 = NULL;
    pasynUInt32 = NULL;
    intrPvtUInt32 = NULL;
    pasynGpib = NULL;
    connected = 0;
    eventMask = 0;
    receivedEvent = 0;
    peeksize = 1;
    previousAsynStatus = asynSuccess;
    debug ("AsynDriverInterface(%s) createAsynUser\n", client->name());
    pasynUser = pasynManager->createAsynUser(handleRequest,
        handleTimeout);
    assert(pasynUser);
    pasynUser->userPvt = this;
#ifdef EPICS_3_13
    debug ("AsynDriverInterface(%s) wdCreate()\n", client->name());
    timer = wdCreate();
    callbackSetCallback(expire, &timeoutCallback);
    callbackSetUser(this, &timeoutCallback);
#else
    debug ("AsynDriverInterface(%s) epicsTimerQueueActive::allocate(true)\n",
        client->name());
    timerQueue = &epicsTimerQueueActive::allocate(true);
    assert(timerQueue);
    debug ("AsynDriverInterface(%s) timerQueue->createTimer()\n", client->name());
    timer = &timerQueue->createTimer();
    assert(timer);
#endif
    debug ("AsynDriverInterface(%s) done\n", client->name());
}

AsynDriverInterface::
~AsynDriverInterface()
{
    cancelTimer();

    if (intrPvtInt32)
    {
        // Int32 event interface is connected
        pasynInt32->cancelInterruptUser(pvtInt32,
            pasynUser, intrPvtInt32);
    }
    if (intrPvtUInt32)
    {
        // UInt32 event interface is connected
        pasynUInt32->cancelInterruptUser(pvtUInt32,
            pasynUser, intrPvtUInt32);
    }
    if (pasynOctet)
    {
        // octet stream interface is connected
        int wasQueued;
        if (intrPvtOctet)
        {
            pasynOctet->cancelInterruptUser(pvtOctet,
                pasynUser, intrPvtOctet);
        }
        pasynManager->cancelRequest(pasynUser, &wasQueued);
        // does not return until running handler has finished
    }
    // Now, no handler is running any more and none will start.

#ifdef EPICS_3_13
    wdDelete(timer);
#else
    timer->destroy();
    timerQueue->release();
#endif
    pasynManager->disconnect(pasynUser);
    pasynManager->freeAsynUser(pasynUser);
    pasynUser = NULL;
}

// interface function getBusInterface():
// do we have this port/addr ?
StreamBusInterface* AsynDriverInterface::
getBusInterface(Client* client,
    const char* portname, int addr, const char*)
{
    debug ("AsynDriverInterface::getBusInterface(%s, %s, %d)\n",
        client->name(), portname, addr);
    AsynDriverInterface* interface = new AsynDriverInterface(client);
    if (interface->connectToBus(portname, addr))
    {
        debug ("AsynDriverInterface::getBusInterface(%s, %d): "
            "new interface allocated\n",
            portname, addr);
        return interface;
    }
    delete interface;
    return NULL;
}

// interface function supportsEvent():
// can we handle the StreamDevice command 'event'?
bool AsynDriverInterface::
supportsEvent()
{
    if (intrPvtInt32 || intrPvtUInt32) return true;

    // look for interfaces for events
    asynInterface* pasynInterface;

    pasynInterface = pasynManager->findInterface(pasynUser,
        asynInt32Type, true);
    if (pasynInterface)
    {
        pasynInt32 = static_cast<asynInt32*>(pasynInterface->pinterface);
        pvtInt32 = pasynInterface->drvPvt;
        pasynUser->reason = ASYN_REASON_SIGNAL; // required for GPIB
        if (pasynInt32->registerInterruptUser(pvtInt32, pasynUser,
            intrCallbackInt32, this, &intrPvtInt32) == asynSuccess)
            return true;
        pasynInt32 = NULL;
        intrPvtInt32 = NULL;
    }

    // no asynInt32 available, thus try asynUInt32
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynUInt32DigitalType, true);
    if (pasynInterface)
    {
        pasynUInt32 =
            static_cast<asynUInt32Digital*>(pasynInterface->pinterface);
        pvtUInt32 = pasynInterface->drvPvt;
        pasynUser->reason = ASYN_REASON_SIGNAL;
        if (pasynUInt32->registerInterruptUser(pvtUInt32,
            pasynUser, intrCallbackUInt32, this, 0xFFFFFFFF,
            &intrPvtUInt32) == asynSuccess)
            return true;
        pasynUInt32 = NULL;
        intrPvtUInt32 = NULL;
    }
    error("%s: port %s does not allow to register for "
        "Int32 or UInt32 interrupts: %s\n",
        clientName(), name(), pasynUser->errorMessage);

    // no event interface available
    return false;
}

bool AsynDriverInterface::
supportsAsyncRead()
{
    if (intrPvtOctet) return true;

    // hook "I/O Intr" support
    if (pasynOctet->registerInterruptUser(pvtOctet, pasynUser,
        intrCallbackOctet, this, &intrPvtOctet) != asynSuccess)
    {
        error("%s: asyn port %s does not support asynchronous input: %s\n",
            clientName(), name(), pasynUser->errorMessage);
        return false;
    }
    return true;
}

bool AsynDriverInterface::
connectToBus(const char* portname, int addr)
{
    asynStatus status = pasynManager->connectDevice(pasynUser, portname, addr);
    debug("%s: AsynDriverInterface::connectToBus(%s, %d): "
        "pasynManager->connectDevice(%p, %s, %d) = %s\n",
        clientName(), portname, addr, pasynUser, portname, addr,
            toStr(status));
    if (status != asynSuccess)
    {
        // asynDriver does not know this portname/address
        return false;
    }

    asynInterface* pasynInterface;

    // find the asynCommon interface
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynCommonType, true);
    if (!pasynInterface)
    {
        error("%s: asyn port %s does not support asynCommon interface\n",
            clientName(), portname);
        return false;
    }
    pasynCommon = static_cast<asynCommon*>(pasynInterface->pinterface);
    pvtCommon = pasynInterface->drvPvt;

    // find the asynOctet interface
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynOctetType, true);
    if (!pasynInterface)
    {
        error("%s: asyn port %s does not support asynOctet interface\n",
            clientName(), portname);
        return false;
    }
    pasynOctet = static_cast<asynOctet*>(pasynInterface->pinterface);
    pvtOctet = pasynInterface->drvPvt;

    // Check if device knows EOS
    size_t streameoslen = 0;
    if (getInTerminator(streameoslen))
    {
        char asyneos[16];
        int eoslen;
        asynStatus status = pasynOctet->getInputEos(pvtOctet,
            pasynUser, asyneos, sizeof(asyneos)-1, &eoslen);
        if (status != asynSuccess)
        {
            error("%s: warning: No input EOS support.\n",
                clientName());
        }
    }

    // is it a GPIB interface ?
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynGpibType, true);
    if (pasynInterface)
    {
        pasynGpib = static_cast<asynGpib*>(pasynInterface->pinterface);
        pvtGpib = pasynInterface->drvPvt;
        // asynGpib returns overflow error if we try to peek
        // (read only one byte first).
        peeksize = inputBuffer.capacity();
    }

    // Install callback for connect/disconnect events
    status = pasynManager->exceptionCallbackAdd(pasynUser, exceptionHandler);
    if (status != asynSuccess)
    {
        debug("%s: warning: Cannot install exception handler: %s\n",
            clientName(), pasynUser->errorMessage);
        // No problem, only @connect handler will not work
    }
    pasynManager->isConnected(pasynUser, &connected);
    debug("%s: AsynDriverInterface::connectToBus(%s, %d): device is now %s\n",
        clientName(), portname, addr, connected ? "connected" : "disconnected");

    return true;
}

void AsynDriverInterface::
reportAsynStatus(asynStatus status, const char *name)
{
    if (previousAsynStatus != status) {
        previousAsynStatus = status;
        if (status == asynSuccess) {
            error("%s %s: status returned to normal\n", clientName(), name);
        } else {
            error("%s %s: %s\n", clientName(), name, pasynUser->errorMessage);
        }
    }
}

// interface function: we want exclusive access to the device
// lockTimeout_ms=0 means "block here" (used in @init)
bool AsynDriverInterface::
lockRequest(unsigned long lockTimeout_ms)
{
    asynStatus status;

    debug("AsynDriverInterface::lockRequest(%s, %ld msec)\n",
        clientName(), lockTimeout_ms);
    lockTimeout = lockTimeout_ms ? lockTimeout_ms*0.001 : -1.0;
    ioAction = Lock;
    status = pasynManager->queueRequest(pasynUser,
        priority(), lockTimeout);
    reportAsynStatus(status, "lockRequest");
    if (status != asynSuccess)
    {
        ioAction = None;
        return false;
    }
    return true;
    // continues with:
    //    handleRequest() -> lockHandler() -> lockCallback()
    // or handleTimeout() -> lockCallback(StreamIoTimeout)
}

bool AsynDriverInterface::
connectToAsynPort()
{
    asynStatus status;

    debug("AsynDriverInterface::connectToAsynPort(%s)\n",
        clientName());
    if (!connected)
    {
        status = pasynCommon->connect(pvtCommon, pasynUser);
        reportAsynStatus(status, "connectToAsynPort");
        if (status != asynSuccess) return false;
        connected = true;
    }
    return true;
}

// now, we can have exclusive access (called by asynManager)
void AsynDriverInterface::
lockHandler()
{
    asynStatus status;

    debug("AsynDriverInterface::lockHandler(%s)\n",
        clientName());

    status = pasynManager->blockProcessCallback(pasynUser, false);
    if (status != asynSuccess)
    {
        error("%s lockHandler: pasynManager->blockProcessCallback() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        lockCallback(StreamIoFault);
        return;
    }
    lockCallback();
}

// interface function: we don't need exclusive access any more
bool AsynDriverInterface::
unlock()
{
    asynStatus status;

    debug("AsynDriverInterface::unlock(%s)\n",
        clientName());
    status = pasynManager->unblockProcessCallback(pasynUser, false);
    if (status != asynSuccess)
    {
        error("%s unlock: pasynManager->unblockProcessCallback() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    return true;
}

// interface function: we want to write something
bool AsynDriverInterface::
writeRequest(const void* output, size_t size,
    unsigned long writeTimeout_ms)
{
    debug("AsynDriverInterface::writeRequest(%s, \"%s\", %ld msec)\n",
        clientName(), StreamBuffer(output, size).expand()(),
        writeTimeout_ms);

    asynStatus status;
    outputBuffer = (char*)output;
    outputSize = size;
    writeTimeout = writeTimeout_ms*0.001;
    ioAction = Write;
    status = pasynManager->queueRequest(pasynUser, priority(),
        writeTimeout);
    reportAsynStatus(status, "writeRequest");
    return (status == asynSuccess);
    // continues with:
    //    handleRequest() -> writeHandler() -> lockCallback()
    // or handleTimeout() -> writeCallback(StreamIoTimeout)
}

// now, we can write (called by asynManager)
void AsynDriverInterface::
writeHandler()
{
    debug("AsynDriverInterface::writeHandler(%s)\n",
        clientName());
    asynStatus status;
    size_t written = 0;

    pasynUser->timeout = 0;
    if (!pasynGpib)
    {
        // discard any early input, but forward it to potential async records
        // thus do not use pasynOctet->flush()
        // unfortunately we cannot do this with GPIB because addressing a
        // device as talker when it has nothing to say is an error.
        // Also timeout=0 does not help here (would need a change in asynGPIB),
        // thus use flush() for GPIB.
        do {
            char buffer [256];
            size_t received = 0;
            int eomReason = 0;
            debug("AsynDriverInterface::writeHandler(%s): reading old input\n",
                clientName());
            status = pasynOctet->read(pvtOctet, pasynUser,
                buffer, sizeof(buffer), &received, &eomReason);
            if (status == asynError || received == 0) break;
            if (received) debug("AsynDriverInterface::writeHandler(%s): "
                "flushing %" Z "u bytes: \"%s\"\n",
                clientName(), received, StreamBuffer(buffer, received).expand()());
        } while (status == asynSuccess);
    }
    else
    {
        debug("AsynDriverInterface::writeHandler(%s): flushing old input\n",
            clientName());
         pasynOctet->flush(pvtOctet, pasynUser);
    }

    // discard any early events
    receivedEvent = 0;

    pasynUser->timeout = writeTimeout;

    // has stream already added a terminator or should
    // asyn do so?

    size_t streameoslen;
    const char* streameos = getOutTerminator(streameoslen);
    int oldeoslen = -1;
    char oldeos[16];
    if (streameos) // stream has already added eos, don't do it again in asyn
    {
        // clear terminator for asyn
        status = pasynOctet->getOutputEos(pvtOctet,
            pasynUser, oldeos, sizeof(oldeos)-1, &oldeoslen);
        if (status != asynSuccess)
        {
            oldeoslen = -1;
            // No EOS support?
        }
        pasynOctet->setOutputEos(pvtOctet, pasynUser,
            NULL, 0);
    }
    int writeTry = 0;
    do {
        pasynUser->errorMessage[0] = 0;
        status = pasynOctet->write(pvtOctet, pasynUser,
            outputBuffer, outputSize, &written);

        debug("AsynDriverInterface::writeHandler(%s): "
            "write(..., \"%s\", outputSize=%" Z "u, written=%" Z "u) "
            "[timeout=%g sec] = %s %s%s\n",
            clientName(),
            StreamBuffer(outputBuffer, outputSize).expand()(),
            outputSize, written,
            pasynUser->timeout, toStr(status),
            pasynUser->errorMessage,
            status ? writeTry ? " failed twice" : " try to connect" : "");

        // When devices goes offline, we get an error only at next write.
        // Maybe the device has re-connected meanwhile?
        // Let's try once more.
    } while (status == asynError && writeTry++ == 0 && connectToAsynPort());

    if (oldeoslen >= 0) // restore asyn terminator
    {
        pasynOctet->setOutputEos(pvtOctet, pasynUser,
            oldeos, oldeoslen);
    }

    switch (status)
    {
        case asynSuccess:
            outputBuffer += written;
            outputSize -= written;
            if (outputSize > 0)
            {
                status = pasynManager->queueRequest(pasynUser,
                    priority(), lockTimeout);
                reportAsynStatus(status, "writeHandler");
                if (status != asynSuccess)
                {
                    writeCallback(StreamIoFault);
                }
                // continues with:
                //    handleRequest() -> writeHandler() -> writeCallback()
                // or handleTimeout() -> writeCallback(StreamIoTimeout)
                return;
            }
            writeCallback();
            return;
        case asynTimeout:
            error("%s: asynTimeout (%g sec) in write: %s\n",
                clientName(), pasynUser->timeout, pasynUser->errorMessage);
            writeCallback(StreamIoTimeout);
            return;
        case asynOverflow:
            error("%s: asynOverflow in write: %s\n",
                clientName(), pasynUser->errorMessage);
            writeCallback(StreamIoFault);
            return;
        case asynError:
            if (!connected)
            {
                error("%s: device %s disconnected\n",
                    clientName(), name());
                disconnectCallback();
            }
            else
            {
                error("%s: asynError in write: %s\n",
                    clientName(), pasynUser->errorMessage);
                writeCallback(StreamIoFault);
            }
            return;
#ifdef ASYN_VERSION // asyn >= 4.14
        case asynDisconnected:
            error("%s: asynDisconnected in write: %s\n",
                clientName(), pasynUser->errorMessage);
            disconnectCallback();
            return;
        case asynDisabled:
            error("%s: asynDisabled in write: %s\n",
                clientName(), pasynUser->errorMessage);
            writeCallback(StreamIoFault);
            return;
#endif
        default:
            error("%s: unknown asyn error in write: %s\n",
                clientName(), pasynUser->errorMessage);
            writeCallback(StreamIoFault);
            return;
    }
}

// interface function: we want to read something
bool AsynDriverInterface::
readRequest(unsigned long replyTimeout_ms, unsigned long readTimeout_ms,
    ssize_t _expectedLength, bool async)
{
    debug("AsynDriverInterface::readRequest(%s, %ld msec reply, "
        "%ld msec read, expect %" Z "u bytes, async=%s)\n",
        clientName(), replyTimeout_ms, readTimeout_ms,
        _expectedLength, async?"yes":"no");

    asynStatus status;
    readTimeout = readTimeout_ms*0.001;
    replyTimeout = replyTimeout_ms*0.001;
    double queueTimeout;
    expectedLength = _expectedLength;

    if (async)
    {
        ioAction = AsyncRead;
        queueTimeout = -1.0;
        // First poll for input (no timeout),
        // later poll periodically if no other input arrives
        // from intrCallbackOctet()
    }
    else {
        ioAction = Read;
        queueTimeout = replyTimeout;
    }
    status = pasynManager->queueRequest(pasynUser,
        priority(), queueTimeout);
    debug("AsynDriverInterface::readRequest %s: "
        "queueRequest(..., priority=%d, queueTimeout=%g sec) = %s [async=%s] %s\n",
        clientName(), priority(), queueTimeout,
        toStr(status), async? "true" : "false",
        status!=asynSuccess ? pasynUser->errorMessage : "");
    if (!async)
    {
        reportAsynStatus(status, "readRequest");
    }
    if (status != asynSuccess)
    {
        // Not queued for some reason (e.g. disconnected / already queued)
        if (async)
        {
            // silently try again later
            startTimer(replyTimeout);
            return true;
        }
        return false;
    }
    // continues with:
    //    handleRequest() -> readHandler() -> readCallback()
    // or handleTimeout() -> readCallback(StreamIoTimeout)
    return true;
}

// now, we can read (called by asynManager)
void AsynDriverInterface::
readHandler()
{
    size_t streameoslen, deveoslen;
    const char* streameos;
    const char* deveos;
    int oldeoslen = -1;
    char oldeos[16];

    // Setup eos if required.
    streameos = getInTerminator(streameoslen);
    deveos = streameos;
    deveoslen = streameoslen;
    if (streameos) // streameos == NULL means: don't change eos
    {
        asynStatus status;
        status = pasynOctet->getInputEos(pvtOctet,
            pasynUser, oldeos, sizeof(oldeos)-1, &oldeoslen);
        if (status != asynSuccess)
            oldeoslen = -1;
        else do {
            // device (e.g. GPIB) might not accept full eos length
            if (deveoslen == (size_t)oldeoslen && strcmp(deveos, oldeos) == 0)
            {
                // nothing to do: old and new eos are the same
                break;
            }
            if (pasynOctet->setInputEos(pvtOctet, pasynUser,
                deveos, (int)deveoslen) == asynSuccess)
            {
                debug2("AsynDriverInterface::readHandler(%s) "
                    "input EOS changed from \"%s\" to \"%s\"\n",
                    clientName(),
                    StreamBuffer(oldeos, oldeoslen).expand()(),
                    StreamBuffer(deveos, deveoslen).expand()());
                break;
            }
            deveos++; deveoslen--;
            if (!deveoslen)
            {
                error("%s: warning: pasynOctet->setInputEos() failed: %s\n",
                    clientName(), pasynUser->errorMessage);
            }
        } while (deveoslen);
    }

    size_t bytesToRead = peeksize;
    size_t buffersize;

    if (expectedLength > 0)
    {
        buffersize = expectedLength;
        if (peeksize > 1)
        {
            /* we can't peek, try to read whole message */
            bytesToRead = expectedLength;
        }
    }
    else
    {
        buffersize = inputBuffer.capacity();
    }
    char* buffer = inputBuffer.clear().reserve(buffersize);

    if (ioAction == AsyncRead)
    {
        // In AsyncRead mode just poll
        // and read as much as possible
        pasynUser->timeout = 0.0;
        bytesToRead = buffersize;
    }
    else
    {
        pasynUser->timeout = replyTimeout;
    }
    bool waitForReply = true;
    size_t received;
    int eomReason;
    asynStatus status;
    ssize_t readMore;

    while (1)
    {
        readMore = 0;
        received = 0;
        eomReason = 0;
        pasynUser->errorMessage[0] = 0;

        status = pasynOctet->read(pvtOctet, pasynUser,
            buffer, bytesToRead, &received, &eomReason);
        // Even though received is size_t I have seen (size_t)-1 here
        // in case half a terminator had been read last time!
        if (!(status == asynTimeout && pasynUser->timeout == 0 && received == 0))
            debug("AsynDriverInterface::readHandler(%s): ioAction=%s "
                "read(%" Z "u bytes, timeout=%g sec) returned status %s: received=%" Z "d bytes, "
                "eomReason=%s, buffer=\"%s\"\n",
                clientName(), toStr(ioAction),
                bytesToRead, pasynUser->timeout, toStr(status), received,
                eomReasonToStr(eomReason), StreamBuffer(buffer, received).expand()());

        // asyn 4.16 sets reason to ASYN_EOM_END when device disconnects.
        // What about earlier versions?
        if (!connected) eomReason |= ASYN_EOM_END;

        if (status == asynTimeout &&
            pasynUser->timeout == 0.0 &&
            received > 0)
        {
            // Jens Eden (PTB) pointed out that polling asynInterposeEos
            // with timeout = 0.0 returns asynTimeout even when bytes
            // have been received, but not yet the terminator.
            status = asynSuccess;
        }

        switch (status)
        {
            case asynSuccess:
                if (ioAction != Read)
                {
                    debug("AsynDriverInterface::readHandler(%s): "
                        "AsyncRead poll: received %" Z "d of %" Z "u bytes \"%s\" "
                        "eomReason=%s [data ignored]\n",
                        clientName(), received, bytesToRead,
                        StreamBuffer(buffer, received).expand()(),
                        eomReasonToStr(eomReason));
                    // ignore what we got from here.
                    // input was already handeled by asynReadHandler()

                    // read until no more input is available
                    readMore = -1;
                    break;
                }
                debug("AsynDriverInterface::readHandler(%s): "
                        "received %" Z "d of %" Z "u bytes \"%s\" "
                        "eomReason=%s\n",
                    clientName(), received, bytesToRead,
                    StreamBuffer(buffer, received).expand()(),
                    eomReasonToStr(eomReason));
                // asynOctet->read() cuts off terminator, but:
                // If stream has set a terminator which is longer
                // than what the device (e.g. GPIB) can handle,
                // only the last part is cut. If that part matches
                // but the whole terminator does not, it is falsely cut.
                // So what to do?
                // Restore complete terminator and leave it to StreamCore to
                // find out if this was really the end of the input.
                // Warning: received can be < 0 if message was read in parts
                // and a multi-byte terminator was partially read with last
                // call.

                if (deveoslen < streameoslen && (eomReason & ASYN_EOM_EOS))
                {
                    size_t i;
                    for (i = 0; i < deveoslen; i++, received++)
                    {
                        if ((ssize_t)received >= 0) buffer[received] = deveos[i];
                        // It is safe to add to buffer here, because
                        // the terminator was already there before
                        // asynOctet->read() had cut it.
                        // Just take care of received < 0
                    }
                    eomReason &= ~ASYN_EOM_EOS;
                }

                readMore = readCallback(
                    eomReason & (ASYN_EOM_END|ASYN_EOM_EOS) ?
                    StreamIoEnd : StreamIoSuccess,
                    buffer, received);
                break;
            case asynTimeout:
                if (received == 0 && waitForReply)
                {
                    // reply timeout
                    if (ioAction == AsyncRead)
                    {
                        debug2("AsynDriverInterface::readHandler(%s): "
                            "no async input, retry in in %g seconds\n",
                            clientName(), replyTimeout);
                        // start next poll after timer expires
                        if (replyTimeout != 0.0) startTimer(replyTimeout);
                        // continues with:
                        //    timerExpired() -> queueRequest() ->
                        //                 handleRequest() -> readHandler()
                        // or intrCallbackOctet() -> asynReadHandler()
                        break;
                    }
                    debug("AsynDriverInterface::readHandler(%s): "
                        "no reply\n",
                        clientName());
                    readMore = readCallback(StreamIoNoReply);
                    break;
                }
                // read timeout
                debug("AsynDriverInterface::readHandler(%s): "
                        "ioAction=%s, timeout [%g sec] "
                        "after %" Z "d of %" Z "u bytes \"%s\"\n",
                    clientName(), toStr(ioAction), pasynUser->timeout,
                    received, bytesToRead,
                    StreamBuffer(buffer, received).expand()());
                if (ioAction == AsyncRead || ioAction == AsyncReadMore)
                {
                    // we already got the data from asynReadHandler()
                    readCallback(StreamIoTimeout, NULL, 0);
                    break;
                }
                readMore = readCallback(StreamIoTimeout, buffer, received);
                break;
            case asynOverflow:
                if (bytesToRead == 1)
                {
                    // device does not support peeking
                    // try to read whole message next time
                    inputBuffer.clear().reserve(100);
                } else {
                    // buffer was still too small
                    // try larger buffer next time
                    inputBuffer.clear().reserve(inputBuffer.capacity()*2);
                }
                peeksize = inputBuffer.capacity();
                // deliver whatever we could save
                error("%s: asynOverflow in read: %s\n",
                    clientName(), pasynUser->errorMessage);
                readCallback(StreamIoFault, buffer, received);
                break;
            case asynError:
                if (!connected) {
                    error("%s: device %s disconnected\n",
                        clientName(), name());
                        disconnectCallback();
                }
                else
                {
                    error("%s: asynError in read: %s\n",
                        clientName(), pasynUser->errorMessage);
                    readCallback(StreamIoFault, buffer, received);
                }
                break;
#ifdef ASYN_VERSION // asyn >= 4.14
            case asynDisconnected:
                error("%s: asynDisconnected in read: %s\n",
                    clientName(), pasynUser->errorMessage);
                connected = false;
                disconnectCallback();
                return;
            case asynDisabled:
                error("%s: asynDisabled in read: %s\n",
                    clientName(), pasynUser->errorMessage);
                readCallback(StreamIoFault);
                return;
#endif
            default:
                error("%s: unknown asyn error in read: %s\n",
                    clientName(), pasynUser->errorMessage);
                readCallback(StreamIoFault);
                return;
        }
        if (!readMore) break;
        if (readMore > 0)
        {
            bytesToRead = readMore;
        }
        else
        {
            bytesToRead = inputBuffer.capacity();
        }
        debug("AsynDriverInterface::readHandler(%s) "
            "readMore=%" Z "d bytesToRead=%" Z "u\n",
            clientName(), readMore, bytesToRead);
        pasynUser->timeout = readTimeout;
        waitForReply = false;
    }

    // restore original EOS
    if (oldeoslen >= 0 && oldeoslen != (int)deveoslen &&
        strcmp(deveos, oldeos) != 0)
    {
        pasynOctet->setInputEos(pvtOctet, pasynUser,
            oldeos, oldeoslen);
        debug2("AsynDriverInterface::readHandler(%s) "
            "input EOS restored from \"%s\" to \"%s\"\n",
            clientName(),
            StreamBuffer(deveos, deveoslen).expand()(),
            StreamBuffer(oldeos, oldeoslen).expand()());
    }
}

void AsynDriverInterface::
intrCallbackOctet(char *data, size_t numchars, int eomReason)
{
// Problems here:
// 1. We get this message too when we are the poller.
//    Thus we have to ignore what we got from polling.
// 2. eomReason=ASYN_EOM_CNT when message was too long for
//    internal buffer of asynDriver.

    if (!interruptAccept) return; // too early to process records
    asynReadHandler(data, numchars, eomReason);
}

// get asynchronous input
void AsynDriverInterface::
asynReadHandler(const char *buffer, size_t received, int eomReason)
{
    // Due to multithreading, timerExpired() might come at any time.
    // It queues the next poll request which is now useless because
    // we got asynchronous input.
    // Unfortunately, we  must be very careful not to block in this
    // function. That means, we must not call cancelRequest() from here!
    // We must also not call cancelRequest() in any other method that
    // might have been called in port thread context, like finish().
    // Luckily, that request cannot be active right now, because we are
    // inside the read handler of some other asynUser in the port thread.
    // Thus, it is sufficient to mark the request as obsolete by
    // setting ioAction=None. See handleRequest().

    debug("AsynDriverInterface::asynReadHandler(%s, buffer=\"%s\", "
            "received=%ld eomReason=%s) ioAction=%s\n",
        clientName(), StreamBuffer(buffer, received).expand()(),
        (long)received, eomReasonToStr(eomReason), toStr(ioAction));

    ioAction = None;
    ssize_t readMore = 1;
    if (received)
    {
        // At the moment, it seems that asynDriver does not cut off
        // terminators for interrupt users and never sets eomReason.
        // This may change in future releases of asynDriver.

        asynStatus status;
        const char* streameos;
        size_t streameoslen;
        streameos = getInTerminator(streameoslen);
        char deveos[16]; // I guess that is sufficient
        int deveoslen;

        if (eomReason & ASYN_EOM_EOS)
        {
            // Terminator was cut off.
            // If terminator is handled by stream, then we must
            // restore the terminator, because the "real" terminator
            // might be longer than what the octet driver supports.

            if (!streameos)  // stream has not defined eos
            {
                // Just handle eos as normal end condition
                // and don't try to add it.
                eomReason |= ASYN_EOM_END;
            }
            else
            {
                // Try to add terminator
                status = pasynOctet->getInputEos(pvtOctet,
                    pasynUser, deveos, sizeof(deveos)-1, &deveoslen);
                if (status == asynSuccess)
                {
                    // We can't just append terminator to buffer, because
                    // we don't own that piece of memory.
                    // First process received data with cut-off terminator
                    readCallback(StreamIoSuccess, buffer, received);
                    // then add terminator
                    buffer = deveos;
                    received = deveoslen;
                }
            }
        }
        else if (!streameos)
        {
            // If terminator was not cut off and terminator was not
            // set by stream, cut it off now.
            status = pasynOctet->getInputEos(pvtOctet,
                pasynUser, deveos, sizeof(deveos)-1, &deveoslen);
            if (status == asynSuccess && (long)received >= (long)deveoslen)
            {
                int i;
                for (i = 1; i <= deveoslen; i++)
                {
                    if (buffer[received - i] != deveos[deveoslen - i]) break;
                }
                if (i > deveoslen) // terminator found
                {
                    received -= deveoslen;
                    eomReason |= ASYN_EOM_END;
                }
            }
        }
        readMore = readCallback(
            eomReason & ASYN_EOM_END ?
            StreamIoEnd : StreamIoSuccess,
            buffer, received);
    }
    if (readMore)
    {
        // wait for more input
        ioAction = AsyncReadMore;
        startTimer(readTimeout);
    }
    debug("AsynDriverInterface::asynReadHandler(%s) readMore=%" Z "d, ioAction=%s \n",
        clientName(), readMore, toStr(ioAction));
}

// interface function: we want to receive an event
bool AsynDriverInterface::
acceptEvent(unsigned long mask, unsigned long replytimeout_ms)
{
    if (receivedEvent & mask)
    {
        // handle early events
        receivedEvent = 0;
        eventCallback();
        return true;
    }
    eventMask = mask;
    ioAction = ReceiveEvent;
    if (replytimeout_ms) startTimer(replytimeout_ms*0.001);
    return true;
}

void AsynDriverInterface::
intrCallbackInt32(epicsInt32 data)
{
    debug("AsynDriverInterface::intrCallbackInt32 (%s, %ld)\n",
        clientName(), (long int) data);
    if (eventMask)
    {
        if (data & eventMask)
        {
            eventMask = 0;
            eventCallback();
        }
        return;
    }
    // store early events
    receivedEvent = data;
}

void AsynDriverInterface::
intrCallbackUInt32(epicsUInt32 data)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pasynUser->userPvt);
    debug("AsynDriverInterface::intrCallbackUInt32 (%s, %ld)\n",
        interface->clientName(), (long int) data);
    if (interface->eventMask)
    {
        if (data & interface->eventMask)
        {
            interface->eventMask = 0;
            interface->eventCallback();
        }
        return;
    }
    // store early events
    interface->receivedEvent = data;
}

void AsynDriverInterface::
exceptionHandler(asynException exception)
{
    debug("AsynDriverInterface::exceptionHandler(%s, %s)\n",
        clientName(), toStr(exception));

    if (exception == asynExceptionConnect)
    {
        pasynManager->isConnected(pasynUser, &connected);
        debug("AsynDriverInterface::exceptionHandler(%s) %s %s. ioAction: %s\n",
            clientName(), name(),
            connected ? "connected" : "disconnected",
            toStr(ioAction));
        if (connected && ioAction == None)
            connectCallback();
    }
}

void AsynDriverInterface::
timerExpired()
{
    int autoconnect;
    switch (ioAction)
    {
        case None:
            // Timeout of async poll crossed with parasitic input
            return;
        case ReceiveEvent:
            // timeout while waiting for event
            ioAction = None;
            eventCallback(StreamIoTimeout);
            return;
        case AsyncReadMore:
            // timeout after reading some async data
            // Deliver what we have.
            readCallback(StreamIoTimeout);
            // readCallback() may have started a new poll
            return;
        case AsyncRead:
            // No async input for some time, thus let's poll.
            // Due to multithreading, asynReadHandler() might be active
            // at the moment if another asynUser got input right now.
            // queueRequest might fail if another request was just queued
            pasynManager->isAutoConnect(pasynUser, &autoconnect);
            if (autoconnect && !connected)
            {
                // has explicitely been disconnected
                // a poll would autoConnect which is not what we want
                // just retry later
                startTimer(replyTimeout);
            }
            else
            {
                // queue for read poll (no timeout)
                asynStatus status = pasynManager->queueRequest(pasynUser,
                    asynQueuePriorityLow, -1.0);
                // if this fails, we are already queued by another thread
                debug2("AsynDriverInterface::timerExpired %s: "
                    "queueRequest(..., priority=Low, queueTimeout=-1) = %s %s\n",
                    clientName(), toStr(status),
                    status!=asynSuccess ? pasynUser->errorMessage : "");
                if (status != asynSuccess) startTimer(replyTimeout);
                // continues with:
                //    handleRequest() -> readHandler() -> readCallback()
            }
            return;
        default:
            error("INTERNAL ERROR (%s): timerExpired() unexpected ioAction %s\n",
                clientName(), toStr(ioAction));
            return;
    }
}

#ifdef EPICS_3_13
void AsynDriverInterface::
expire(CALLBACK *pcallback)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pcallback->user);
    interface->timerExpired();
}
#else
epicsTimerNotify::expireStatus AsynDriverInterface::
expire(const epicsTime &)
{
    timerExpired();
    return noRestart;
}
#endif

bool AsynDriverInterface::
connectRequest(unsigned long connecttimeout_ms)
{
    double queueTimeout = connecttimeout_ms*0.001;
    asynStatus status;
    ioAction = Connect;

    debug("AsynDriverInterface::connectRequest %s\n",
        clientName());
    status = pasynManager->queueRequest(pasynUser,
        asynQueuePriorityConnect, queueTimeout);
    reportAsynStatus(status, "connectRequest");
    return (status == asynSuccess);
    // continues with:
    //    handleRequest() -> connectHandler() -> connectCallback()
    // or handleTimeout() -> connectCallback(StreamIoTimeout)
}

void AsynDriverInterface::
connectHandler()
{
    connectCallback(connectToAsynPort() ? StreamIoSuccess : StreamIoFault);
}

bool AsynDriverInterface::
disconnectRequest()
{
    asynStatus status;
    ioAction = Disconnect;

    debug("AsynDriverInterface::disconnectRequest %s\n",
        clientName());
    status = pasynManager->queueRequest(pasynUser,
        asynQueuePriorityConnect, 0.0);
    reportAsynStatus(status, "disconnectRequest");
    return (status == asynSuccess);
    // continues with:
    //    handleRequest() -> disconnectHandler()
}

void AsynDriverInterface::
disconnectHandler()
{
    asynStatus status;

    debug("AsynDriverInterface::disconnectHandler %s is %s disconnected\n",
        clientName(), !connected ? "already" : "not yet");
    if (connected)
    {
        status = pasynCommon->disconnect(pvtCommon, pasynUser);
        if (status != asynSuccess)
        {
            error("%s connectRequest: pasynCommon->disconnect() failed: %s\n",
                clientName(), pasynUser->errorMessage);
            disconnectCallback(StreamIoFault);
            return;
        }
        connected = false;
    }
    disconnectCallback();
}

void AsynDriverInterface::
finish()
{
    debug("AsynDriverInterface::finish(%s) start\n",
        clientName());
    cancelTimer();
    ioAction = None;
//     if (pasynGpib)
//     {
//         // Release GPIB device the the end of the protocol
//         // to re-enable local buttons.
//         pasynGpib->ren(pvtGpib, pasynUser, 0);
//     }
    debug("AsynDriverInterface::finish(%s) done\n",
        clientName());
}

// asynUser callbacks to pasynManager->queueRequest()

void AsynDriverInterface::
handleRequest()
{
    cancelTimer();
    debug2("AsynDriverInterface::handleRequest(%s) %s\n",
        clientName(), toStr(ioAction));
    switch (ioAction)
    {
        case None:
            // ignore obsolete poll request
            // see asynReadHandler()
            break;
        case Lock:
            lockHandler();
            break;
        case Write:
            writeHandler();
            break;
        case AsyncRead: // polled async input
        case AsyncReadMore:
        case Read:      // sync input
            readHandler();
            break;
        case Connect:
            connectHandler();
            break;
        case Disconnect:
            disconnectHandler();
            break;
        default:
            error("INTERNAL ERROR (%s): "
                "handleRequest() unexpected ioAction %s\n",
                clientName(), toStr(ioAction));
    }
}

void AsynDriverInterface::
handleTimeout()
{
    debug("AsynDriverInterface::handleTimeout(%s)\n",
        clientName());
    switch (ioAction)
    {
        case Lock:
            lockCallback(StreamIoTimeout);
            break;
        case Write:
            writeCallback(StreamIoTimeout);
            break;
        case Read:
            readCallback(StreamIoFault);
            break;
        case AsyncReadMore:
            readCallback(StreamIoTimeout);
            break;
        case Connect:
            connectCallback(StreamIoTimeout);
            break;
        case Disconnect:
            error("AsynDriverInterface %s: disconnect timeout\n",
                clientName());
            // should not happen because of infinite timeout
            break;
        // No AsyncRead here because we don't use timeout when polling
        default:
            error("INTERNAL ERROR (%s): handleTimeout() "
                "unexpected ioAction %s\n",
                clientName(), toStr(ioAction));
    }
}

extern "C" long streamReinit(const char* portname, int addr)
{
    if (!portname)
    {
        fprintf(stderr, "Usage: streamReinit \"portname\", [addr]\n");
        return -1;
    }
    asynUser* pasynUser = pasynManager->createAsynUser(NULL, NULL);
    if (!pasynUser)
    {
        fprintf(stderr, "Can't create asynUser\n");
        return -1;
    }
    asynStatus status = pasynManager->connectDevice(pasynUser, portname, addr);
    if (status == asynSuccess)
        status = pasynManager->exceptionDisconnect(pasynUser);
    if (status == asynSuccess)
        status = pasynManager->exceptionConnect(pasynUser);
    if (status != asynSuccess)
        fprintf(stderr, "%s\n", pasynUser->errorMessage);
    pasynManager->disconnect(pasynUser);
    pasynManager->freeAsynUser(pasynUser);
    return status;
}

#ifndef EPICS_3_13
static const iocshArg streamReinitArg0 =
    { "portname", iocshArgString };
static const iocshArg streamReinitArg1 =
    { "[addr]", iocshArgInt };
static const iocshArg * const streamReinitArgs[] =
    { &streamReinitArg0, &streamReinitArg1 };
static const iocshFuncDef streamReinitDef =
    { "streamReinit", 2, streamReinitArgs };

void streamReinitFunc(const iocshArgBuf *args)
{
    streamReinit(args[0].sval, args[1].ival);
}
static void AsynDriverInterfaceRegistrar ()
{
     iocshRegister(&streamReinitDef, streamReinitFunc);
}

extern "C" {
epicsExportRegistrar(AsynDriverInterfaceRegistrar);
}

#endif
