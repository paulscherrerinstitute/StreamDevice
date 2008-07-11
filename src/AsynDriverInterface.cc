/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to asyn driver for StreamDevice.       *
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
#include "StreamBusInterface.h"
#include "StreamError.h"
#include "StreamBuffer.h"

#ifdef EPICS_3_14
#include <epicsAssert.h>
#include <epicsTime.h>
#include <epicsTimer.h>
#else
#include <assert.h>
#include <wdLib.h>
#include <sysLib.h>
extern "C" {
#include <callback.h>
}
#endif

#include <asynDriver.h>
#include <asynOctet.h>
#include <asynInt32.h>
#include <asynUInt32Digital.h>
#include <asynGpibDriver.h>

/* How things are implemented:

synchonous io:

lockRequest()
    pasynManager->blockProcessCallback(),
    pasynCommon->connect() only if
        lockTimeout is unlimited (0) and port is not yet connected
    pasynManager->queueRequest()
    when request is handled
        lockCallback(StreamIoSuccess)
    if request fails
        lockCallback(StreamIoTimeout)

writeRequest()
    pasynManager->queueRequest()
    when request is handled
        pasynOctet->flush()
        pasynOctet->writeRaw()
        if writeRaw() times out
            writeCallback(StreamIoTimeout)
        if writeRaw fails otherwise
            writeCallback(StreamIoFault)
        if writeRaw succeeds and all bytes have been written
            writeCallback(StreamIoSuccess)
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

extern "C" {
static void handleRequest(asynUser*);
static void handleTimeout(asynUser*);
static void intrCallbackOctet(void* pvt, asynUser *pasynUser,
    char *data, size_t numchars, int eomReason);
static void intrCallbackInt32(void* pvt, asynUser *pasynUser,
    epicsInt32 data);
static void intrCallbackUInt32(void* pvt, asynUser *pasynUser,
    epicsUInt32 data);
}

enum IoAction {
    None, Lock, Write, Read, AsyncRead, AsyncReadMore,
    ReceiveEvent, Connect, Disconnect
};

static const char* ioActionStr[] = {
    "None", "Lock", "Write", "Read",
    "AsyncRead", "AsyncReadMore",
    "ReceiveEvent", "Connect", "Disconnect"
};

static const char* asynStatusStr[] = {
    "asynSuccess", "asynTimeout", "asynOverflow", "asynError"
};

static const char* eomReasonStr[] = {
    "NONE", "CNT", "EOS", "CNT+EOS", "END", "CNT+END", "EOS+END", "CNT+EOS+END"
};

class AsynDriverInterface : StreamBusInterface
#ifdef EPICS_3_14
 , epicsTimerNotify
#endif
{
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
    IoAction ioAction;
    double lockTimeout;
    double writeTimeout;
    double readTimeout;
    double replyTimeout;
    long expectedLength;
    unsigned long eventMask;
    unsigned long receivedEvent;
    StreamBuffer inputBuffer;
    const char* outputBuffer;
    size_t outputSize;
    int peeksize;
#ifdef EPICS_3_14
    epicsTimerQueueActive* timerQueue;
    epicsTimer* timer;
#else
    WDOG_ID timer;
    CALLBACK timeoutCallback;
#endif

    AsynDriverInterface(Client* client);
    ~AsynDriverInterface();

    // StreamBusInterface methods
    bool lockRequest(unsigned long lockTimeout_ms);
    bool unlock();
    bool writeRequest(const void* output, size_t size,
        unsigned long writeTimeout_ms);
    bool readRequest(unsigned long replyTimeout_ms,
        unsigned long readTimeout_ms, long expectedLength, bool async);
    bool acceptEvent(unsigned long mask, unsigned long replytimeout_ms);
    bool supportsEvent();
    bool supportsAsyncRead();
    bool connectRequest(unsigned long connecttimeout_ms);
    bool disconnectRequest();
    void finish();

#ifdef EPICS_3_14
    // epicsTimerNotify methods
    epicsTimerNotify::expireStatus expire(const epicsTime &);
#else
    static void expire(CALLBACK *pcallback);
#endif

    // local methods
    void timerExpired();
    bool connectToBus(const char* busname, int addr);
    void lockHandler();
    void writeHandler();
    void readHandler();
    void connectHandler();
    void disconnectHandler();
    bool connectToAsynPort();
    void asynReadHandler(const char *data, int numchars, int eomReason);
    asynQueuePriority priority() {
        return static_cast<asynQueuePriority>
            (StreamBusInterface::priority());
    }
    void startTimer(double timeout) {
#ifdef EPICS_3_14
        timer->start(*this, timeout);
#else
        callbackSetPriority(priority(), &timeoutCallback);
        wdStart(timer, (int)((timeout+1)*sysClkRateGet())-1,
            reinterpret_cast<FUNCPTR>(callbackRequest),
            reinterpret_cast<int>(&timeoutCallback));
#endif
    }
    void cancelTimer() {
#ifdef EPICS_3_14
        timer->cancel();
#else
        wdCancel(timer);
#endif
    }

    // asynUser callback functions
    friend void handleRequest(asynUser*);
    friend void handleTimeout(asynUser*);
    friend void intrCallbackOctet(void* pvt, asynUser *pasynUser,
        char *data, size_t numchars, int eomReason);
    friend void intrCallbackInt32(void* pvt, asynUser *pasynUser,
        epicsInt32 data);
    friend void intrCallbackUInt32(void* pvt, asynUser *pasynUser,
        epicsUInt32 data);
public:
    // static creator method
    static StreamBusInterface* getBusInterface(Client* client,
        const char* busname, int addr, const char* param);
};

RegisterStreamBusInterface(AsynDriverInterface);

AsynDriverInterface::
AsynDriverInterface(Client* client) : StreamBusInterface(client)
{
    pasynCommon = NULL;
    pasynOctet = NULL;
    intrPvtOctet = NULL;
    pasynInt32 = NULL;
    intrPvtInt32 = NULL;
    pasynUInt32 = NULL;
    intrPvtUInt32 = NULL;
    pasynGpib = NULL;
    eventMask = 0;
    receivedEvent = 0;
    peeksize = 1;
    pasynUser = pasynManager->createAsynUser(handleRequest,
        handleTimeout);
    assert(pasynUser);
    pasynUser->userPvt = this;
#ifdef EPICS_3_14
    timerQueue = &epicsTimerQueueActive::allocate(true);
    assert(timerQueue);
    timer = &timerQueue->createTimer();
    assert(timer);
#else
    timer = wdCreate();
    callbackSetCallback(expire, &timeoutCallback);
    callbackSetUser(this, &timeoutCallback);
#endif
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

#ifdef EPICS_3_14
    timer->destroy();
    timerQueue->release();
#else
    wdDelete(timer);
#endif
    pasynManager->disconnect(pasynUser);
    pasynManager->freeAsynUser(pasynUser);
    pasynUser = NULL;
}

// interface function getBusInterface():
// do we have this bus/addr ?
StreamBusInterface* AsynDriverInterface::
getBusInterface(Client* client,
    const char* busname, int addr, const char*)
{
    AsynDriverInterface* interface = new AsynDriverInterface(client);
    if (interface->connectToBus(busname, addr))
    {
        debug ("AsynDriverInterface::getBusInterface(%s, %d): "
            "new Interface allocated\n",
            busname, addr);
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
    return (pasynInt32 != NULL) || (pasynUInt32 != NULL);
}

bool AsynDriverInterface::
supportsAsyncRead()
{
    if (intrPvtOctet) return true;

    // hook "I/O Intr" support
    if (pasynOctet->registerInterruptUser(pvtOctet, pasynUser,
        intrCallbackOctet, this, &intrPvtOctet) != asynSuccess)
    {
        error("%s: bus does not support asynchronous input: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    return true;
}

bool AsynDriverInterface::
connectToBus(const char* busname, int addr)
{
    if (pasynManager->connectDevice(pasynUser, busname, addr) !=
        asynSuccess)
    {
        // asynDriver does not know this busname/address
        return false;
    }

    asynInterface* pasynInterface;

    // find the asynCommon interface
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynCommonType, true);
    if(!pasynInterface)
    {
        error("%s: bus %s does not support asynCommon interface\n",
            clientName(), busname);
        return false;
    }
    pasynCommon = static_cast<asynCommon*>(pasynInterface->pinterface);
    pvtCommon = pasynInterface->drvPvt;

    // find the asynOctet interface
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynOctetType, true);
    if(!pasynInterface)
    {
        error("%s: bus %s does not support asynOctet interface\n",
            clientName(), busname);
        return false;
    }
    pasynOctet = static_cast<asynOctet*>(pasynInterface->pinterface);
    pvtOctet = pasynInterface->drvPvt;

    // is it a GPIB interface ?
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynGpibType, true);
    if(pasynInterface)
    {
        pasynGpib = static_cast<asynGpib*>(pasynInterface->pinterface);
        pvtGpib = pasynInterface->drvPvt;
        // asynGpib returns overflow error if we try to peek
        // (read only one byte first).
        peeksize = inputBuffer.capacity();
    }

    // look for interfaces for events
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynInt32Type, true);
    if(pasynInterface)
    {
        pasynInt32 = static_cast<asynInt32*>(pasynInterface->pinterface);
        pvtInt32 = pasynInterface->drvPvt;
        pasynUser->reason = ASYN_REASON_SIGNAL; // required for GPIB
        if (pasynInt32->registerInterruptUser(pvtInt32, pasynUser,
            intrCallbackInt32, this, &intrPvtInt32) == asynSuccess)
        {
            return true;
        }
        error("%s: bus %s does not allow to register for "
            "Int32 interrupts: %s\n",
            clientName(), busname, pasynUser->errorMessage);
        pasynInt32 = NULL;
        intrPvtInt32 = NULL;
    }

    // no asynInt32 available, thus try asynUInt32
    pasynInterface = pasynManager->findInterface(pasynUser,
        asynUInt32DigitalType, true);
    if(pasynInterface)
    {
        pasynUInt32 =
            static_cast<asynUInt32Digital*>(pasynInterface->pinterface);
        pvtUInt32 = pasynInterface->drvPvt;
        pasynUser->reason = ASYN_REASON_SIGNAL;
        if (pasynUInt32->registerInterruptUser(pvtUInt32,
            pasynUser, intrCallbackUInt32, this, 0xFFFFFFFF,
            &intrPvtUInt32) == asynSuccess)
        {
            return true;
        }
        error("%s: bus %s does not allow to register for "
            "UInt32 interrupts: %s\n",
            clientName(), busname, pasynUser->errorMessage);
        pasynUInt32 = NULL;
        intrPvtUInt32 = NULL;
    }

    // no event interface available, never mind
    return true;
}

// interface function: we want exclusive access to the device
// lockTimeout_ms=0 means "block here" (used in @init)
bool AsynDriverInterface::
lockRequest(unsigned long lockTimeout_ms)
{
    int connected;
    asynStatus status;
    
    debug("AsynDriverInterface::lockRequest(%s, %ld msec)\n",
        clientName(), lockTimeout_ms);
    lockTimeout = lockTimeout_ms ? lockTimeout_ms*0.001 : -1.0;
    ioAction = Lock;
    status = pasynManager->isConnected(pasynUser, &connected);
    if (status != asynSuccess)
    {
        error("%s: pasynManager->isConnected() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    status = pasynManager->queueRequest(pasynUser,
        connected ? priority() : asynQueuePriorityConnect,
        lockTimeout);
    if (status != asynSuccess)
    {
        error("%s lockRequest: pasynManager->queueRequest() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    // continues with:
    //    handleRequest() -> lockHandler() -> lockCallback()
    // or handleTimeout() -> lockCallback(StreamIoTimeout)
    return true;
}

bool AsynDriverInterface::
connectToAsynPort()
{
    asynStatus status;
    int connected;

    debug("AsynDriverInterface::connectToAsynPort(%s)\n",
        clientName());
    status = pasynManager->isConnected(pasynUser, &connected);
    if (status != asynSuccess)
    {
        error("%s: pasynManager->isConnected() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    debug("AsynDriverInterface::connectToAsynPort(%s) is %s connected\n",
        clientName(), connected ? "already" : "not yet");
    if (!connected)
    {
        status = pasynCommon->connect(pvtCommon, pasynUser);
        debug("AsynDriverInterface::connectToAsynPort(%s): "
                "status=%s\n",
            clientName(), asynStatusStr[status]);
        if (status != asynSuccess)
        {
            error("%s: pasynCommon->connect() failed: %s\n",
                clientName(), pasynUser->errorMessage);
            return false;
        }
    }
//  We probably should set REN=1 prior to sending but this
//  seems to hang up the device every other time.
//     if (pasynGpib)
//     {
//         pasynGpib->ren(pvtGpib, pasynUser, 1);
//     }
    return true;
}

// now, we can have exclusive access (called by asynManager)
void AsynDriverInterface::
lockHandler()
{
    int connected;
    debug("AsynDriverInterface::lockHandler(%s)\n",
        clientName());
    pasynManager->blockProcessCallback(pasynUser, false);
    connected = connectToAsynPort();
    lockCallback(connected ? StreamIoSuccess : StreamIoFault);
}

// interface function: we don't need exclusive access any more
bool AsynDriverInterface::
unlock()
{
    debug("AsynDriverInterface::unlock(%s)\n",
        clientName());
    pasynManager->unblockProcessCallback(pasynUser, false);
    return true;
}

// interface function: we want to write something
bool AsynDriverInterface::
writeRequest(const void* output, size_t size,
    unsigned long writeTimeout_ms)
{
#ifndef NO_TEMPORARY
    debug("AsynDriverInterface::writeRequest(%s, \"%s\", %ld msec)\n",
        clientName(), StreamBuffer(output, size).expand()(),
        writeTimeout_ms);
#endif

    asynStatus status;
    outputBuffer = (char*)output;
    outputSize = size;
    writeTimeout = writeTimeout_ms*0.001;
    ioAction = Write;
    status = pasynManager->queueRequest(pasynUser, priority(),
        writeTimeout);
    if (status != asynSuccess)
    {
        error("%s writeRequest: pasynManager->queueRequest() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    // continues with:
    //    handleRequest() -> writeHandler() -> lockCallback()
    // or handleTimeout() -> writeCallback(StreamIoTimeout)
    return true;
}

// now, we can write (called by asynManager)
void AsynDriverInterface::
writeHandler()
{
    debug("AsynDriverInterface::writeHandler(%s)\n",
        clientName());
    asynStatus status;
    size_t written = 0;
    pasynUser->timeout = writeTimeout;

    // discard any early input or early events
    status = pasynOctet->flush(pvtOctet, pasynUser);
    receivedEvent = 0;

    if (status != asynSuccess)
    {
        error("%s: pasynOctet->flush() failed: %s\n",
                clientName(), pasynUser->errorMessage);
        writeCallback(StreamIoFault);
        return;
    }
    
    // has stream already added a terminator or should
    // asyn do so?
    
    size_t streameoslen;
    const char* streameos = getOutTerminator(streameoslen);
    if (streameos) // stream has added eos
    {
        status = pasynOctet->writeRaw(pvtOctet, pasynUser,
            outputBuffer, outputSize, &written);
    }
    else // asyn should add eos
    {
        status = pasynOctet->write(pvtOctet, pasynUser,
            outputBuffer, outputSize, &written);
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
                if (status != asynSuccess)
                {
                    error("%s writeHandler: "
                        "pasynManager->queueRequest() failed: %s\n",
                        clientName(), pasynUser->errorMessage);
                    writeCallback(StreamIoFault);
                }
                // continues with:
                //    handleRequest() -> writeHandler() -> writeCallback()
                // or handleTimeout() -> writeCallback(StreamIoTimeout)
                return;
            }
            writeCallback(StreamIoSuccess);
            return;
        case asynTimeout:
            writeCallback(StreamIoTimeout);
            return;
        case asynOverflow:
            error("%s: asynOverflow in write: %s\n",
                clientName(), pasynUser->errorMessage);
            writeCallback(StreamIoFault);
            return;
        case asynError:
            error("%s: asynError in write: %s\n",
                clientName(), pasynUser->errorMessage);
            writeCallback(StreamIoFault);
            return;
    }
}

// interface function: we want to read something
bool AsynDriverInterface::
readRequest(unsigned long replyTimeout_ms, unsigned long readTimeout_ms,
    long _expectedLength, bool async)
{
    debug("AsynDriverInterface::readRequest(%s, %ld msec reply, "
        "%ld msec read, expect %ld bytes, asyn=%s)\n",
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
    if (status != asynSuccess && !async)
    {
        error("%s readRequest: pasynManager->queueRequest() failed: %s\n",
            clientName(), pasynUser->errorMessage);
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
    if (streameos) do // streameos == NULL means: don't change eos
    {
        asynStatus status;
        status = pasynOctet->getInputEos(pvtOctet,
            pasynUser, oldeos, sizeof(oldeos)-1, &oldeoslen);
        if (status != asynSuccess)
        {
            oldeoslen = -1;
            // No EOS support?
        }
        // device (e.g. GPIB) might not accept full eos length
        if (pasynOctet->setInputEos(pvtOctet, pasynUser,
            deveos, deveoslen) == asynSuccess)
        {
#ifndef NO_TEMPORARY
            if (ioAction != AsyncRead)
            {
                debug("AsynDriverInterface::readHandler(%s) "
                    "input EOS set to %s\n",
                    clientName(),
                    StreamBuffer(deveos, deveoslen).expand()());
            }
#endif
            break;
        }
        deveos++; deveoslen--;
        if (!deveoslen)
        {
            error("%s: warning: pasynOctet->setInputEos() failed: %s\n",
                clientName(), pasynUser->errorMessage);
        }
    } while (deveoslen);

    int bytesToRead = peeksize;
    long buffersize;

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
        pasynUser->timeout = readTimeout;
        bytesToRead = buffersize;
    }
    else
    {
        pasynUser->timeout = replyTimeout;
    }
    bool waitForReply = true;
    int received;
    int eomReason;
    asynStatus status;
    long readMore;

    while (1)
    {
        readMore = 0;
        received = 0;
        eomReason = 0;
        
        status = pasynOctet->read(pvtOctet, pasynUser,
            buffer, bytesToRead, (size_t*)&received, &eomReason);
        if (ioAction != AsyncRead || status != asynTimeout)
        {
            debug("AsynDriverInterface::readHandler(%s): "
                "read(..., bytesToRead=%d, ...) [timeout=%f seconds] = %s\n",
                clientName(), bytesToRead, pasynUser->timeout,
                asynStatusStr[status]);
        }
        // pasynOctet->read() has already cut off terminator.

        switch (status)
        {
            case asynSuccess:
                if (ioAction == AsyncRead)
                {
#ifndef NO_TEMPORARY
                    debug("AsynDriverInterface::readHandler(%s): "
                        "AsyncRead poll: received %d of %d bytes \"%s\" "
                        "eomReason=%s [data ignored]\n",
                        clientName(), received, bytesToRead,
                        StreamBuffer(buffer, received).expand()(),
                        eomReasonStr[eomReason&0x7]);
#endif
                    // ignore what we got from here.
                    // input was already handeled by asynReadHandler()
                    // read until no more input is available
                    readMore = -1;
                    break;
                }
#ifndef NO_TEMPORARY
                debug("AsynDriverInterface::readHandler(%s): "
                        "received %d of %d bytes \"%s\" "
                        "eomReason=%s\n",
                    clientName(), received, bytesToRead,
                    StreamBuffer(buffer, received).expand()(),
                    eomReasonStr[eomReason&0x7]);
#endif
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
                        if (received >= 0) buffer[received] = deveos[i];
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
                        // start next poll after timer expires
                        if (replyTimeout != 0.0) startTimer(replyTimeout);
                        // continues with:
                        //    timerExpired() -> queueRequest() -> handleRequest() -> readHandler()
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
#ifndef NO_TEMPORARY
                debug("AsynDriverInterface::readHandler(%s): "
                        "ioAction=%s, timeout after %d of %d bytes \"%s\"\n",
                    clientName(), ioActionStr[ioAction],
                    received, bytesToRead,
                    StreamBuffer(buffer, received).expand()());
#endif
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
                error("%s: asynError in read: %s\n",
                    clientName(), pasynUser->errorMessage);
                readCallback(StreamIoFault, buffer, received);
                break;
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
            "readMore=%ld bytesToRead=%d\n",
            clientName(), readMore, bytesToRead);
        pasynUser->timeout = readTimeout;
        waitForReply = false;
    }
    
    // restore original EOS
    if (oldeoslen >= 0)
    {
        pasynOctet->setInputEos(pvtOctet, pasynUser,
            oldeos, oldeoslen);
    }
}

void intrCallbackOctet(void* /*pvt*/, asynUser *pasynUser,
    char *data, size_t numchars, int eomReason)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pasynUser->userPvt);

// Problems here:
// 1. We get this message too when we are the poller.
//    Thus we have to ignore what we got from polling.
// 2. We get this message multiple times when original reader
//    reads in chunks.
// 3. eomReason=ASYN_EOM_CNT when message was too long for
//    internal buffer of asynDriver.

    if (!interruptAccept) return; // too early to process records
    if (interface->ioAction == AsyncRead ||
        interface->ioAction == AsyncReadMore)
    {
        interface->asynReadHandler(data, numchars, eomReason);
    }
    else
    {
#ifndef NO_TEMPORARY
    debug("AsynDriverInterface::intrCallbackOctet(%s, buffer=\"%s\", "
            "received=%d eomReason=%s) ioAction=%s\n",
        interface->clientName(), StreamBuffer(data, numchars).expand()(),
        numchars, eomReasonStr[eomReason&0x7], ioActionStr[interface->ioAction]);
#endif
    }
}

// get asynchronous input
void AsynDriverInterface::
asynReadHandler(const char *buffer, int received, int eomReason)
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
    
#ifndef NO_TEMPORARY
    debug("AsynDriverInterface::asynReadHandler(%s, buffer=\"%s\", "
            "received=%d eomReason=%s) ioAction=%s\n",
        clientName(), StreamBuffer(buffer, received).expand()(),
        received, eomReasonStr[eomReason&0x7], ioActionStr[ioAction]);
#endif

    ioAction = None;
    long readMore = 1;
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
                    readCallback(
                        StreamIoSuccess,
                        buffer, received);
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
            if (status == asynSuccess && received >= deveoslen)
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
    debug("AsynDriverInterface::asynReadHandler(%s) readMore=%li, ioAction=%s \n",
        clientName(), readMore, ioActionStr[ioAction]);
}

// interface function: we want to receive an event
bool AsynDriverInterface::
acceptEvent(unsigned long mask, unsigned long replytimeout_ms)
{
    if (receivedEvent & mask)
    {
        // handle early events
        receivedEvent = 0;
        eventCallback(StreamIoSuccess);
        return true;
    }
    eventMask = mask;
    ioAction = ReceiveEvent;
    startTimer(replytimeout_ms*0.001);
    return true;
}

void intrCallbackInt32(void* /*pvt*/, asynUser *pasynUser, epicsInt32 data)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pasynUser->userPvt);
    debug("AsynDriverInterface::intrCallbackInt32 (%s, %ld)\n",
        interface->clientName(), (long int) data);
    if (interface->eventMask)
    {
        if (data & interface->eventMask)
        {
            interface->eventMask = 0;
            interface->eventCallback(StreamIoSuccess);
        }
        return;
    }
    // store early events
    interface->receivedEvent = data;
}

void intrCallbackUInt32(void* /*pvt*/, asynUser *pasynUser,
    epicsUInt32 data)
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
            interface->eventCallback(StreamIoSuccess);
        }
        return;
    }
    // store early events
    interface->receivedEvent = data;
}

void AsynDriverInterface::
timerExpired()
{
    int autoconnect, connected;
    switch (ioAction)
    {
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
            pasynManager->isConnected(pasynUser, &connected);
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
                pasynManager->queueRequest(pasynUser,
                    asynQueuePriorityLow, -1.0);
                // continues with:
                //    handleRequest() -> readHandler() -> readCallback()
            }
            return;
        default:
            error("INTERNAL ERROR (%s): timerExpired() unexpected ioAction %s\n",
                clientName(), ioActionStr[ioAction]);
            return;
    }
}

#ifdef EPICS_3_14
epicsTimerNotify::expireStatus AsynDriverInterface::
expire(const epicsTime &)
{
    timerExpired();
    return noRestart;
}
#else
void AsynDriverInterface::
expire(CALLBACK *pcallback)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pcallback->user);
    interface->timerExpired();
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
    if (status != asynSuccess)
    {
        error("%s connectRequest: pasynManager->queueRequest() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    // continues with:
    //    handleRequest() -> connectHandler() -> connectCallback()
    // or handleTimeout() -> connectCallback(StreamIoTimeout)
    return true;
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
    if (status != asynSuccess)
    {
        error("%s disconnectRequest: pasynManager->queueRequest() failed: %s\n",
            clientName(), pasynUser->errorMessage);
        return false;
    }
    // continues with:
    //    handleRequest() -> disconnectHandler()
    return true;
}

void AsynDriverInterface::
disconnectHandler()
{
    int connected;
    asynStatus status;

    pasynManager->isConnected(pasynUser, &connected);
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
    }
    disconnectCallback(StreamIoSuccess);
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

void handleRequest(asynUser* pasynUser)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pasynUser->userPvt);
    debug("AsynDriverInterface::handleRequest(%s)\n",
        interface->clientName());
    switch (interface->ioAction)
    {
        case None:
            // ignore obsolete poll request
            // see asynReadHandler()
            break;
        case Lock:
            interface->lockHandler();
            break;
        case Write:
            interface->writeHandler();
            break;
        case AsyncRead: // polled async input
        case AsyncReadMore:
        case Read:      // sync input
            interface->readHandler();
            break;
        case Connect:
            interface->connectHandler();
            break;
        case Disconnect:
            interface->disconnectHandler();
            break;
        default:
            error("INTERNAL ERROR (%s): "
                "handleRequest() unexpected ioAction %s\n",
                interface->clientName(), ioActionStr[interface->ioAction]);
    }
}

void handleTimeout(asynUser* pasynUser)
{
    AsynDriverInterface* interface =
        static_cast<AsynDriverInterface*>(pasynUser->userPvt);
    debug("AsynDriverInterface::handleTimeout(%s)\n",
        interface->clientName());
    switch (interface->ioAction)
    {
        case Lock:
            interface->lockCallback(StreamIoTimeout);
            break;
        case Write:
            interface->writeCallback(StreamIoTimeout);
            break;
        case Read:
            interface->readCallback(StreamIoFault, NULL, 0);
            break;
        case AsyncReadMore:
            interface->readCallback(StreamIoTimeout, NULL, 0);
            break;
        case Connect:
            interface->connectCallback(StreamIoTimeout);
            break;
        case Disconnect:
            error("AsynDriverInterface %s: disconnect timeout\n",
                interface->clientName());
            // should not happen because of infinite timeout
            break;
        // No AsyncRead here because we don't use timeout when polling
        default:
            error("INTERNAL ERROR (%s): handleTimeout() "
                "unexpected ioAction %s\n",
                interface->clientName(), ioActionStr[interface->ioAction]);
    }
}

