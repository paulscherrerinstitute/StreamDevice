/*************************************************************************
* This is the core of StreamDevice.
* Please see ../docs/ for detailed documentation.
*
* (C) 1999,2005 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#ifndef StreamCore_h
#define StreamCore_h

#include "StreamProtocol.h"
#include "StreamFormatConverter.h"
#include "StreamBusInterface.h"

/**************************************
 virtual methods:

bool getFieldAddress(const char* fieldname, StreamBuffer& address)
  If a format sting contains a field name (like "%(EGU)s") a value should
  be taken from / put to that field instead of the default location.
  This function must convert the fieldname into some address structure and
  put the structure into the address buffer (e.g. address.set(addressStruct)).
  The address structure must be suitable for bytewise copying. I.e. memcpy()
  to so some other memory location must not invalidate its contents. No
  constructor, destructor or copy operator will be called.
  formatValue() and matchValue() get a pointer to the structure.
  getFieldAddress() must return true on success and false on failure.

bool formatValue(const StreamFormat& format, const void* fieldaddress)
  This function should first get a value from fieldaddress (if not NULL) or
  the default location (which may depend on format.type).
  The printValue(format,XXX) function suitable for format.type should be
  called to print value. If value is an array, printValue() should be called
  for each element. The separator string will be added automatically.
  formatValue() must return true on success and false on failure.

bool matchValue(const StreamFormat& format, const void* fieldaddress)
  This function should first call the scanValue(format,XXX) function
  suitable for format.type. If scanValue() returns true, the scanned value
  should be put into fieldaddress (if not NULL) or the default location
  (which may depend on format.type).
  If value is an array, scanValue() should be called for each element. It
  returns false if there is no more element available. The separator string
  is matched automatically.
  matchValue() must return true on success and false on failure.


void protocolStartHook()
void protocolFinishHook(ProtocolResult)
void startTimer(unsigned short timeout)
void lockRequest(unsigned short timeout)
void unlock()
void writeRequest(unsigned short timeout)
void readRequest(unsigned short replytimeout, unsigned short timeout)
void readAsyn(unsigned short timeout)
void acceptEvent(unsigned short mask, unsigned short timeout)

***************************************/

#include "MacroMagic.h"
#include "time.h"

// Flags: 0x00FFFFFF reserved for StreamCore
const unsigned long None             = 0x0000;
const unsigned long IgnoreExtraInput = 0x0001;
const unsigned long InitRun          = 0x0002;
const unsigned long AsyncMode        = 0x0004;
const unsigned long GotValue         = 0x0008;
const unsigned long BusOwner         = 0x0010;
const unsigned long Separator        = 0x0020;
const unsigned long ScanTried        = 0x0040;
const unsigned long AcceptInput      = 0x0100;
const unsigned long AcceptEvent      = 0x0200;
const unsigned long LockPending      = 0x0400;
const unsigned long WritePending     = 0x0800;
const unsigned long WaitPending      = 0x1000;
const unsigned long Aborted          = 0x2000;
const unsigned long BusPending       = LockPending|WritePending|WaitPending;
const unsigned long ClearOnStart     = InitRun|AsyncMode|GotValue|Aborted|
                                       BusOwner|Separator|ScanTried|
                                       AcceptInput|AcceptEvent|BusPending;

// The amount of time to wait before printing duplicated messages
extern int streamErrorDeadTime;

struct StreamFormat;

class StreamCore :
    StreamProtocolParser::Client,
    StreamBusInterface::Client
{
protected:

    ENUM(ProtocolResult,
        Success, LockTimeout, WriteTimeout, ReplyTimeout, ReadTimeout, ScanError, FormatError, Abort, Fault, Offline);

    ENUM(StartMode,
        StartNormal, StartInit, StartAsync);

    ENUM (Commands,
        end, in, out, wait, event, exec, connect, disconnect);

    class MutexLock
    {
        StreamCore* stream;

    public:
        MutexLock(StreamCore* _stream) : stream(_stream)
            { _stream->lockMutex(); }
        ~MutexLock()
            { stream->releaseMutex(); }
    };

    friend class MutexLock;

    StreamCore* next;
    static StreamCore* first;

    char* streamname;
    unsigned long flags;

    bool attachBus(const char* busname, int addr, const char* param);
    void releaseBus();

    bool startProtocol(StartMode);
    void finishProtocol(ProtocolResult);
    void timerCallback();

    bool printValue(const StreamFormat& format, long value);
    bool printValue(const StreamFormat& format, double value);
    bool printValue(const StreamFormat& format, char* value);
    ssize_t scanValue(const StreamFormat& format, long& value);
    ssize_t scanValue(const StreamFormat& format, double& value);
    ssize_t scanValue(const StreamFormat& format, char* value, size_t& size);
    ssize_t scanValue(const StreamFormat& format);

    StreamBuffer protocolname;
    unsigned long lockTimeout;
    unsigned long writeTimeout;
    unsigned long replyTimeout;
    unsigned long readTimeout;
    unsigned long pollPeriod;
    unsigned long maxInput;
    bool inTerminatorDefined;
    bool outTerminatorDefined;
    StreamBuffer inTerminator;
    StreamBuffer outTerminator;
    StreamBuffer separator;
    StreamBuffer commands;        // the normal protocol
    StreamBuffer onInit;          // init protocol (optional)
    StreamBuffer onWriteTimeout;  // error handler (optional)
    StreamBuffer onReplyTimeout;  // error handler (optional)
    StreamBuffer onReadTimeout;   // error handler (optional)
    StreamBuffer onMismatch;      // error handler (optional)
    const char* commandIndex;     // current position
    char activeCommand;           // current command
    StreamBuffer outputLine;
    StreamBuffer inputBuffer;
    StreamBuffer inputLine;
    size_t consumedInput;
    ProtocolResult runningHandler;
    StreamBuffer fieldAddress;

    // Keep track of errors to reduce logging frequencies
    ProtocolResult previousResult;
    time_t lastErrorTime;
    int numberOfErrors;

    StreamIoStatus lastInputStatus;
    bool unparsedInput;

    StreamCore(const StreamCore&); // undefined
    bool compile(StreamProtocolParser::Protocol*);
    bool evalCommand();
    bool evalOut();
    bool evalIn();
    bool evalEvent();
    bool evalWait();
    bool evalExec();
    bool evalConnect();
    bool evalDisconnect();
    bool formatOutput();
    bool matchInput();
    bool matchSeparator();
    void printSeparator();

// StreamProtocolParser::Client methods
    bool compileCommand(StreamProtocolParser::Protocol*,
        StreamBuffer&, const char* command, const char*& args);
    bool getFieldAddress(const char* fieldname,
        StreamBuffer& address) = 0;

// StreamBusInterface::Client methods
    void lockCallback(StreamIoStatus status);
    void writeCallback(StreamIoStatus status);
    ssize_t readCallback(StreamIoStatus status,
        const void* input, size_t size);
    void eventCallback(StreamIoStatus status);
    void execCallback(StreamIoStatus status);
    void connectCallback(StreamIoStatus status);
    void disconnectCallback(StreamIoStatus status);
    const char* getInTerminator(size_t& length);
    const char* getOutTerminator(size_t& length);

// virtual methods
    virtual void protocolStartHook() {}
    virtual void protocolFinishHook(ProtocolResult) {}
    virtual void startTimer(unsigned long timeout) = 0;
    virtual bool formatValue(const StreamFormat&, const void* fieldaddress) = 0;
    virtual bool matchValue (const StreamFormat&, const void* fieldaddress) = 0;
    virtual void lockMutex() = 0;
    virtual void releaseMutex() = 0;
    virtual bool execute();

public:
    StreamCore();
    virtual ~StreamCore();
    bool parse(const char* filename, const char* protocolname);
    void printProtocol(FILE* = stdout);
    const char* name() { return streamname; }
    void printStatus(StreamBuffer& buffer);
    static const char* license(void);

private:
    char* printCommands(StreamBuffer& buffer, const char* c);
    bool  checkShouldPrint(ProtocolResult newErrorType);
};

#endif
