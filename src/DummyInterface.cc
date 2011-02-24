/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2011 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the interface to a "dummy" bus driver for            *
* StreamDevice. It does not provide any I/O functionality.     *
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
