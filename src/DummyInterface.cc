/*************************************************************************
* This is a debug and example bus interface for StreamDevice.
* It does not provide any I/O functionality.
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

#include "StreamBusInterface.h"
#include "StreamError.h"
#include "StreamBuffer.h"

class DummyInterface : StreamBusInterface
{
    DummyInterface(Client* client);

    // StreamBusInterface methods
    bool lockRequest(unsigned long lockTimeout_ms);
    bool unlock();

protected:
    ~DummyInterface();

public:
    // static creator method
    static StreamBusInterface* getBusInterface(Client* client,
        const char* busname, int addr, const char* param);
};

RegisterStreamBusInterface(DummyInterface);

DummyInterface::
DummyInterface(Client* client) : StreamBusInterface(client)
{
    // Nothing to do
}

DummyInterface::
~DummyInterface()
{
    // Nothing to do
}

StreamBusInterface* DummyInterface::
getBusInterface(Client* client,
    const char* busname, int addr, const char*)
{
    if (strcmp(busname, "dummy") == 0)
    {
        DummyInterface* interface = new DummyInterface(client);
        return interface;
    }
    return NULL;
}

bool DummyInterface::
lockRequest(unsigned long lockTimeout_ms)
{
    return false;
}

bool DummyInterface::
unlock()
{
    return false;
}
