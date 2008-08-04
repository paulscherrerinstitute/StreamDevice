/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to bus drivers for StreamDevice.       *
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

#include "StreamBusInterface.h"

const char* StreamIoStatusStr[] = {
    "StreamIoSuccess",
    "StreamIoTimeout",
    "StreamIoNoReply",
    "StreamIoEnd",
    "StreamIoFault"
};

StreamBusInterfaceRegistrarBase* StreamBusInterfaceRegistrarBase::first;

StreamBusInterfaceRegistrarBase::
StreamBusInterfaceRegistrarBase(const char* name) : name(name)
{
    next = NULL;
    StreamBusInterfaceRegistrarBase** pr;
    for (pr = &first; *pr; pr = &(*pr)->next);
    *pr = this;
}

StreamBusInterfaceRegistrarBase::
~StreamBusInterfaceRegistrarBase()
{
}

StreamBusInterface::
StreamBusInterface(Client* client) :
    client(client)
{
}

bool StreamBusInterface::
supportsEvent()
{
    return false;
}

bool StreamBusInterface::
supportsAsyncRead()
{
    return false;
}

StreamBusInterface* StreamBusInterface::
find(Client* client, const char* busname, int addr, const char* param)
{
    StreamBusInterfaceRegistrarBase* r;
    StreamBusInterface* bus;
    for (r = r->first; r; r = r->next)
    {
        bus = r->find(client, busname, addr, param);
        if (bus) return bus;
    }
    return NULL;
}

bool StreamBusInterface::
acceptEvent(unsigned long, unsigned long)
{
    return false;
}

void StreamBusInterface::
release ()
{
    delete this;
}

bool StreamBusInterface::
connectRequest (unsigned long)
{
    return false;
}

bool StreamBusInterface::
disconnectRequest ()
{
    return false;
}

bool StreamBusInterface::
writeRequest(const void*, size_t, unsigned long)
{
    return false;
}

bool StreamBusInterface::
readRequest(unsigned long, unsigned long, long, bool)
{
    return false;
}

void StreamBusInterface::
finish()
{
}

StreamBusInterface::Client::
~Client()
{
}

void StreamBusInterface::Client::
writeCallback(StreamIoStatus)
{
}

long StreamBusInterface::Client::
readCallback(StreamIoStatus, const void*, long)
{
    return 0;
}

void StreamBusInterface::Client::
eventCallback(StreamIoStatus)
{
}

void StreamBusInterface::Client::
connectCallback(StreamIoStatus)
{
}

void StreamBusInterface::Client::
disconnectCallback(StreamIoStatus)
{
}

long StreamBusInterface::Client::
priority()
{
    return 0;
}
