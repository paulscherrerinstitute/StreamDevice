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

#include <stdio.h>
#include "StreamBusInterface.h"
#include "StreamError.h"

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
    debug("StreamBusInterface::find(%s, %s, %d, \"%s\")\n",
            client->name(), busname, addr, param);
    StreamBusInterfaceRegistrarBase* r;
    StreamBusInterface* bus;
    for (r = StreamBusInterfaceRegistrarBase::first; r; r = r->next)
    {
    debug("StreamBusInterface::find %s check %s\n",
            client->name(), r->name);
        bus = r->find(client, busname, addr, param);
        debug("StreamBusInterface::find %s %s\n",
            r->name, bus ? "matches" : "does not match");
        if (bus)
        {
            if (addr >= 0)
            {
                bus->_name = new char[strlen(busname) + sizeof(int)*2 + sizeof(int)/2 + 2];
                sprintf(bus->_name, "%s %d", busname, addr);
            }
            else
            {
                bus->_name = new char[strlen(busname) + 1];
                strcpy(bus->_name, busname);
            }
            return bus;
        }
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
readRequest(unsigned long, unsigned long, size_t, bool)
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

ssize_t StreamBusInterface::Client::
readCallback(StreamIoStatus, const void*, size_t)
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
