/*************************************************************************
* This is the interface to bus (I/O) drivers for StreamDevice.
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
readRequest(unsigned long, unsigned long, ssize_t, bool)
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
