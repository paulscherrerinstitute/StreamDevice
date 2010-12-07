/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the kernel of StreamDevice.                          *
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

enum Flags {
    // 0x00FFFFFF reserved for StreamCore
    None = 0x0000,
    IgnoreExtraInput = 0x0001,
    InitRun = 0x0002,
    AsyncMode = 0x0004,
    GotValue = 0x0008,
    BusOwner = 0x0010,
    Separator = 0x0020,
    ScanTried = 0x0040,
    AcceptInput = 0x0100,
    AcceptEvent = 0x0200,
    LockPending = 0x0400,
    WritePending = 0x0800,
    WaitPending = 0x1000,
    BusPending = LockPending|WritePending|WaitPending,
    ClearOnStart = InitRun|AsyncMode|GotValue|BusOwner|Separator|ScanTried|
                    AcceptInput|AcceptEvent|BusPending
};

struct StreamFormat;

class StreamCore :
    StreamProtocolParser::Client,
    StreamBusInterface::Client
{
protected:
    enum ProtocolResult {
        Success, LockTimeout, WriteTimeout, ReplyTimeout, ReadTimeout,
        ScanError, FormatError, Abort, Fault
    };

    enum StartMode {
        StartNormal, StartInit, StartAsync
    };

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
    long scanValue(const StreamFormat& format, long& value);
    long scanValue(const StreamFormat& format, double& value);
    long scanValue(const StreamFormat& format, char* value, long maxlen);
    long scanValue(const StreamFormat& format);

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
    const char* activeCommand;    // start of current command
    StreamBuffer outputLine;
    StreamBuffer inputBuffer;
    StreamBuffer inputLine;
    long consumedInput;
    ProtocolResult runningHandler;
    StreamBuffer fieldAddress;

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
    long readCallback(StreamIoStatus status,
        const void* input, long size);
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
    void printProtocol();
    const char* name() { return streamname; }
};

#endif
