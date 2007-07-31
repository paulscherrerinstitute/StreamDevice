/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to a debug and example bus drivers for *
* StreamDevice. Please refer to the HTML files in ../doc/ for  *
* a detailed documentation.                                    *
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

#include "StreamBusInterface.h"
#include "StreamError.h"
#include "StreamBuffer.h"

// This is a non-blocking bus interface for debugging purpose.
// Normally, a bus interface will use blocking I/O and thus require
// a separate thread.

class DebugInterface : StreamBusInterface
{
    DebugInterface(Client* client);

    // StreamBusInterface methods
    bool lockRequest(unsigned long lockTimeout_ms);
    bool unlock();
    bool writeRequest(const void* output, size_t size,
        unsigned long writeTimeout_ms);
    bool readRequest(unsigned long replyTimeout_ms,
        unsigned long readTimeout_ms, long expectedLength, bool async);

protected:
    ~DebugInterface();

public:
    // static creator method
    static StreamBusInterface* getBusInterface(Client* client,
        const char* busname, int addr, const char* param);
};

RegisterStreamBusInterface(DebugInterface);

DebugInterface::
DebugInterface(Client* client) : StreamBusInterface(client)
{
    // Create or attach to interface thread
}

DebugInterface::
~DebugInterface()
{
    // Abort any running I/O
    // Disconnect low-level bus driver
    // Free all resources
}

// Interface method getBusInterface():
// Do we have this bus/addr ?
StreamBusInterface* DebugInterface::
getBusInterface(Client* client,
    const char* busname, int addr, const char*)
{
    if (strcmp(busname, "debug") == 0)
    {
        DebugInterface* interface = new DebugInterface(client);
        debug ("DebugInterface::getBusInterface(%s, %d): "
            "new Interface allocated\n",
            busname, addr);
        // Connect to low-level bus driver here or in constructor
        // Delete interface and return NULL if connect fails.
        return interface;
    }
    return NULL;
}

// Interface method lockRequest():
// We want exclusive access to the device
// lockTimeout_ms=0 means "block here" (used in @init)
// Return false if the lock request cannot be accepted.
bool DebugInterface::
lockRequest(unsigned long lockTimeout_ms)
{
    debug("DebugInterface::lockRequest(%s, %ld msec)\n",
        clientName(), lockTimeout_ms);

    // Debug interface is non-blocking,
    // thus we can call lockCallback() immediately.
    lockCallback(StreamIoSuccess);

    // A blocking interface would send a message that requests
    // exclusive access to the interface thread.
    // Once the interface is available, the interface thread
    // would call lockCallback(StreamIoSuccess).
    // If lockTimeout_ms expires, the interface would
    // call lockCallback(ioTimeout).

    // Only if lockTimeout_ms==0, a blocking interface is allowed,
    // even required, to block here until the interface is available.

    return true;
}

// Interface method unlock():
// We don't need exclusive access any more
// Return false if the unlock fails. (How can that be?)
bool DebugInterface::
unlock()
{
    debug("DebugInterface::unlock(%s)\n",
        clientName());

    // debug interface is non-blocking, thus unlock does nothing.

    // A blocking interface would call lockCallback(StreamIoSuccess) now
    // for the next interface that had called lockRequest.

    return true;
}

// Interface method writeRequest():
// We want to write something
// This method will only be called by StreamDevice when locked.
// I.e. lockRequest() has been called and the interface
// has called lockCallback(StreamIoSuccess).
// It may be called several times before unlock() is called.
// Return false if the write request cannot be accepted.
bool DebugInterface::
writeRequest(const void* output, size_t size, unsigned long writeTimeout_ms)
{
    debug("DebugInterface::writeRequest(%s, \"%.*s\", %ld msec)\n",
        clientName(), (int)size, (char*)output, writeTimeout_ms);

    // Debug interface is non-blocking,
    // thus we can call writeCallback() immediately.
    writeCallback(StreamIoSuccess);

    // A blocking interface would send a message that requests
    // write access to the interface thread.
    // That thread would handle the actual write.
    // Writing all output might take some time and might be
    // done in several chunks.
    // If any chunk cannot be written within writeTimeout_ms milliseconds,
    // the interface thread would call writeCallback(ioTimeout).
    // If anything is wrong with the bus (e.g. unpugged cable),
    // the interface would return false now or call writeCallback(ioFault)
    // later when it finds out.
    // Once the interface has completed writing, the interface thread
    // would call writeCallback(StreamIoSuccess).

    return true;
}

// Interface method readRequest():
// We want to read something
// This method may be called in async mode, if a previous call to
// supportsAsyncRead() had returned true.
// It may be called unlocked.
// If expectedLength!=0, it is a hint when to stop reading input.
// Return false if the read request cannot be accepted.
bool DebugInterface::
readRequest(unsigned long replyTimeout_ms, unsigned long readTimeout_ms,
    long expectedLength, bool async)
{
    debug("DebugInterface::readRequest(%s, %ld msec reply, %ld msec read, expect %ld bytes, asyn=%s)\n",
        clientName(), replyTimeout_ms, readTimeout_ms, expectedLength, async?"yes":"no");

    // Debug interface does not support async mode.
    // Since we have not implemented supportsAsyncRead(),
    // this method is never called with async=true.
    if (async) return false;

    // Debug interface is non-blocking,
    // thus we can call writeCallback() immediately.
    const char input [] = "Receviced input 3.1415\r\n";
    readCallback(StreamIoEnd, input, sizeof(input));

    // A blocking interface would send a message that requests
    // read access to the interface thread.
    // That thread would handle the actual read.
    // Reading all input might take some time and might be
    // done in several chunks.
    // If no data arrives within replyTimeout_ms milliseconds,
    // the interface thread would call
    // readCallback(ioNoReply).
    // If any following chunk cannot be read within readTimeout_ms
    // milliseconds, the interface thread would call
    // readCallback(ioTimeout, input, inputlength)
    // with all input received so far.
    // For each reveived chunk, the interface would call
    // readCallback(StreamIoSuccess, input, inputlength)
    // and check its return value.
    // The return value is 0 if no more input is expected, i.e.
    // the interface would tell the low-level driver that
    // reading has finished (if necessary) and return.
    // If the return value is >0, this number of additional bytes is
    // expected at maximum (this replaces the original expectedLengh).
    // If the return value is -1, an unknown number of additional
    // bytes is expected.
    // In the both cases where more input is expected, the interface
    // would do another read and call readCallback() again.
    // The return value needs only to be checked if readCallback()
    // is called with StreamIoSuccess as the first argument.
    // If the interface received a signal from the low-level driver
    // telling that input has terminated (e.g. EOS in GPIB, EOF when
    // reading file or socket, ...) it would call
    // readCallback(StreamIoEnd, input, inputlength).
    // If anything is wrong with the bus (e.g. unpugged cable),
    // the interface would immediately return false now or
    // call readCallback(ioFault) later when it finds out.

    // On some busses, input may arrive even before readRequest()
    // has been called, e.g. when a device replies very fast.
    // In this case, the interface would buffer the input or call
    // readCallback() even without beeing asked to do so.

    // In async mode, the interface would arrange that the client
    // gets a copy of the next input on the same bus/address,
    // even when the input is meant for an other client.

    return true;
}
