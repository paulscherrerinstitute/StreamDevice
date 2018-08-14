/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to bus drivers for StreamDevice.       *
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

#ifndef StreamBusInterface_h
#define StreamBusInterface_h

#include <stddef.h>
#include "StreamBuffer.h"
#include "MacroMagic.h"

ENUM (StreamIoStatus,
    StreamIoSuccess, StreamIoTimeout, StreamIoNoReply,
    StreamIoEnd, StreamIoFault);

class StreamBusInterface
{
public:

    class Client
    {
    private:
        friend class StreamBusInterface;

        virtual void lockCallback(StreamIoStatus status) = 0;
        virtual void writeCallback(StreamIoStatus status);
        virtual ssize_t readCallback(StreamIoStatus status,
            const void* input, size_t size);
        virtual void eventCallback(StreamIoStatus status);
        virtual void connectCallback(StreamIoStatus status);
        virtual void disconnectCallback(StreamIoStatus status);
        virtual long priority();
        virtual const char* getInTerminator(size_t& length) = 0;
        virtual const char* getOutTerminator(size_t& length) = 0;
    public:
        virtual const char* name() = 0;
        virtual ~Client();
    protected:
        StreamBusInterface* businterface;
        bool busSupportsEvent() {
            return businterface && businterface->supportsEvent();
        }
        bool busSupportsAsyncRead() {
            return businterface && businterface->supportsAsyncRead();
        }
        bool busAcceptEvent(unsigned long mask,
            unsigned long replytimeout_ms) {
            return businterface && businterface->acceptEvent(mask, replytimeout_ms);
        }
        void busRelease() {
            if (businterface) businterface->release();
        }
        bool busLockRequest(unsigned long timeout_ms) {
            return businterface && businterface->lockRequest(timeout_ms);
        }
        bool busUnlock() {
            return businterface && businterface->unlock();
        }
        bool busWriteRequest(const void* output, size_t size,
            unsigned long timeout_ms) {
            return businterface && businterface->writeRequest(output, size, timeout_ms);
        }
        bool busReadRequest(unsigned long replytimeout_ms,
            unsigned long readtimeout_ms, size_t expectedLength,
            bool async) {
            return businterface && businterface->readRequest(replytimeout_ms,
                    readtimeout_ms, expectedLength, async);
        }
        void busFinish() {
            if (businterface) businterface->finish();
        }
        bool busConnectRequest(unsigned long timeout_ms) {
            return businterface && businterface->connectRequest(timeout_ms);
        }
        bool busDisconnect() {
            return businterface && businterface->disconnectRequest();
        }
        void busPrintStatus(StreamBuffer& buffer) {
            if (businterface) businterface->printStatus(buffer);
        }
    };

private:
    friend class StreamBusInterfaceClass; // the iterator
    friend class Client;
    char* _name;

public:
    Client* client;
    virtual ~StreamBusInterface() {};
    const char* name() { return _name; }

protected:
    StreamBusInterface(Client* client);

// map client functions into StreamBusInterface namespace
    void lockCallback(StreamIoStatus status = StreamIoSuccess)
        { client->lockCallback(status); }
    void writeCallback(StreamIoStatus status = StreamIoSuccess)
        { client->writeCallback(status); }
    ssize_t readCallback(StreamIoStatus status,
        const void* input = NULL, size_t size = 0)
        { return client->readCallback(status, input, size); }
    void eventCallback(StreamIoStatus status = StreamIoSuccess)
        { client->eventCallback(status); }
    void connectCallback(StreamIoStatus status = StreamIoSuccess)
        { client->connectCallback(status); }
    void disconnectCallback(StreamIoStatus status = StreamIoSuccess)
        { client->disconnectCallback(status); }
    const char* getInTerminator(size_t& length)
        { return client->getInTerminator(length); }
    const char* getOutTerminator(size_t& length)
        { return client->getOutTerminator(length); }
    long priority() { return client->priority(); }
    const char* clientName() { return client->name(); }

// default implementations
    virtual bool writeRequest(const void* output, size_t size,
        unsigned long timeout_ms);
    virtual bool readRequest(unsigned long replytimeout_ms,
        unsigned long readtimeout_ms, size_t expectedLength,
        bool async);
    virtual bool supportsEvent(); // defaults to false
    virtual bool supportsAsyncRead(); // defaults to false
    virtual bool acceptEvent(unsigned long mask, // implement if
        unsigned long replytimeout_ms);     // supportsEvents() returns true
    virtual void release();
    virtual bool connectRequest(unsigned long connecttimeout_ms);
    virtual bool disconnectRequest();
    virtual void finish();
    virtual void printStatus(StreamBuffer& buffer) {};

// pure virtual
    virtual bool lockRequest(unsigned long timeout_ms) = 0;
    virtual bool unlock() = 0;

public:
// static methods
    static StreamBusInterface* find(Client*, const char* busname,
        int addr, const char* param);
};

class StreamBusInterfaceRegistrarBase
{
    friend class StreamBusInterfaceClass; // the iterator
    friend class StreamBusInterface;      // the implementation
    static StreamBusInterfaceRegistrarBase* first;
    StreamBusInterfaceRegistrarBase* next;
    virtual StreamBusInterface* find(StreamBusInterface::Client* client,
        const char* busname, int addr, const char* param) = 0;
protected:
    const char* name;
    StreamBusInterfaceRegistrarBase(const char* name);
    virtual ~StreamBusInterfaceRegistrarBase();
};

template <class C>
class StreamBusInterfaceRegistrar : protected StreamBusInterfaceRegistrarBase
{
    StreamBusInterface* find(StreamBusInterface::Client* client,
        const char* busname, int addr, const char* param)
        { return C::getBusInterface(client, busname, addr, param); }
public:
    StreamBusInterfaceRegistrar(const char* name) :
        StreamBusInterfaceRegistrarBase(name) {};
};


#define RegisterStreamBusInterface(interface) \
template class StreamBusInterfaceRegistrar<interface>; \
StreamBusInterfaceRegistrar<interface> \
registrar_##interface(#interface); \
void* ref_##interface = &registrar_##interface\

// Interface class iterator

class StreamBusInterfaceClass
{
    StreamBusInterfaceRegistrarBase* ptr;
public:
    StreamBusInterfaceClass () {
        ptr = StreamBusInterfaceRegistrarBase::first;
    }
    StreamBusInterfaceClass& operator ++ () {
        ptr = ptr->next; return *this;
    }
    const char* name() {
        return ptr->name;
    }
    operator bool () {
        return ptr != NULL;
    }
};

#endif
