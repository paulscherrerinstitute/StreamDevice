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

#include "StreamCore.h"
#include "StreamError.h"
#include <ctype.h>
#include <stdlib.h>

enum Commands { end_cmd, in_cmd, out_cmd, wait_cmd, event_cmd, exec_cmd,
    connect_cmd, disconnect_cmd };
const char* commandStr[] = { "end", "in", "out", "wait", "event", "exec",
    "connect", "disconnect" };

inline const char* commandName(unsigned char i)
{
    return i >= sizeof(commandStr)/sizeof(char*) ? "invalid" : commandStr[i];
}

/// debug functions /////////////////////////////////////////////

static char* printCommands(StreamBuffer& buffer, const char* c)
{
    unsigned long timeout;
    unsigned long eventnumber;
    while (1)
    {
        switch(*c++)
        {
            case end_cmd:
                return buffer();
            case in_cmd:
                buffer.append("    in \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case out_cmd:
                buffer.append("    out \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case wait_cmd:
                timeout = extract<unsigned long>(c);
                buffer.print("    wait %ld;\n # ms", timeout);
                break;
            case event_cmd:
                eventnumber = extract<unsigned long>(c);
                timeout = extract<unsigned long>(c);
                buffer.print("    event(%ld) %ld; # ms\n", eventnumber, timeout);
                break;
            case exec_cmd:
                buffer.append("    exec \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case connect_cmd:
                timeout = extract<unsigned long>(c);
                buffer.print("    connect %ld; # ms\n", timeout);
                break;
            case disconnect_cmd:
                buffer.append("    disconnect;\n");
                break;
            default:
                buffer.append("\033[31;1mGARBAGE: ");
                c = StreamProtocolParser::printString(buffer, c-1);
                buffer.append("\033[0m\n");
        }
    }
}

void StreamCore::
printProtocol()
{
    StreamBuffer buffer;
    printf("%s {\n", protocolname());
    printf("  extraInput    = %s;\n",
      (flags & IgnoreExtraInput) ? "ignore" : "error");
    printf("  lockTimeout   = %ld; # ms\n", lockTimeout);
    printf("  readTimeout   = %ld; # ms\n", readTimeout);
    printf("  replyTimeout  = %ld; # ms\n", replyTimeout);
    printf("  writeTimeout  = %ld; # ms\n", writeTimeout);
    printf("  pollPeriod    = %ld; # ms\n", pollPeriod);
    printf("  maxInput      = %ld; # bytes\n", maxInput);
    StreamProtocolParser::printString(buffer.clear(), inTerminator());
    printf("  inTerminator  = \"%s\";\n", buffer());
        StreamProtocolParser::printString(buffer.clear(), outTerminator());
    printf("  outTerminator = \"%s\";\n", buffer());
        StreamProtocolParser::printString(buffer.clear(), separator());
    printf("  separator     = \"%s\";\n", buffer());
    if (onInit)
        printf("  @Init {\n%s  }\n",
        printCommands(buffer.clear(), onInit()));
    if (onReplyTimeout)
        printf("  @ReplyTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onReplyTimeout()));
    if (onReadTimeout)
        printf("  @ReadTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onReadTimeout()));
    if (onWriteTimeout)
        printf("  @WriteTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onWriteTimeout()));
    if (onMismatch)
        printf("  @Mismatch {\n%s  }\n",
        printCommands(buffer.clear(), onMismatch()));
    printf("\n%s}\n",
        printCommands(buffer.clear(), commands()));
}

///////////////////////////////////////////////////////////////////////////

StreamCore* StreamCore::first = NULL;

StreamCore::
StreamCore()
{
    businterface = NULL;
    flags = None;
    next = NULL;
    unparsedInput = false;
    // add myself to list of streams
    StreamCore** pstream;
    for (pstream = &first; *pstream; pstream = &(*pstream)->next);
    *pstream = this;
}

StreamCore::
~StreamCore()
{
    debug("~StreamCore(%s) %p\n", name(), (void*)this);
    releaseBus();
    // remove myself from list of all streams
    StreamCore** pstream;
    for (pstream = &first; *pstream; pstream = &(*pstream)->next)
    {
        if (*pstream == this)
        {
            *pstream = next;
            break;
        }
    }
}

bool StreamCore::
attachBus(const char* busname, int addr, const char* param)
{
    releaseBus();
    businterface = StreamBusInterface::find(this, busname, addr, param);
    if (!businterface)
    {
        error("Businterface '%s' not found for '%s'\n",
            busname, name());
        return false;
    }
    debug("StreamCore::attachBus(busname=\"%s\", addr=%i, param=\"%s\") businterface=%p\n",
        busname, addr, param, (void*)businterface);
    return true;
}

void StreamCore::
releaseBus()
{
    if (businterface)
    {
        if (flags & BusOwner)
        {
            busUnlock();
        }
        busRelease();
        businterface = NULL;
    }
}

// Parse the protocol

bool StreamCore::
parse(const char* filename, const char* _protocolname)
{
    protocolname = _protocolname;
    // extract substitutions from protocolname "name(sub1,sub2)"
    int i = protocolname.find('(');
    if (i >= 0)
    {
        while (i >= 0)
        {
            protocolname[i] = '\0'; // replace '(' and ',' with '\0'
            i = protocolname.find(',', i+1);
        }
        // should have closing parentheses
        if (protocolname[-1] != ')')
        {
            error("Missing ')' after substitutions '%s'\n", _protocolname);
            return false;
        }
        protocolname.truncate(-1); // remove ')'
    }
    StreamProtocolParser::Protocol* protocol;
    protocol = StreamProtocolParser::getProtocol(filename, protocolname);
    if (!protocol)
    {
        error("while reading protocol '%s' for '%s'\n", protocolname(), name());
        return false;
    }
    if (!compile(protocol))
    {
        delete protocol;
        error("while compiling protocol '%s' for '%s'\n", _protocolname, name());
        return false;
    }
    delete protocol;
    return true;
}

bool StreamCore::
compile(StreamProtocolParser::Protocol* protocol)
{
    const char* extraInputNames [] = {"error", "ignore", NULL};

    // default values for protocol variables
    flags &= ~IgnoreExtraInput;
    lockTimeout = 5000;
    readTimeout = 100;
    replyTimeout = 1000;
    writeTimeout = 100;
    maxInput = 0;
    pollPeriod = 1000;
    inTerminatorDefined = false;
    outTerminatorDefined = false;
    
    unsigned short ignoreExtraInput = false;
    if (!protocol->getEnumVariable("extrainput", ignoreExtraInput,
        extraInputNames))
    {
        return false;
    }
    if (ignoreExtraInput) flags |= IgnoreExtraInput;
    if (!(protocol->getNumberVariable("locktimeout", lockTimeout) &&
        protocol->getNumberVariable("readtimeout", readTimeout) &&
        protocol->getNumberVariable("replytimeout", replyTimeout) &&
        protocol->getNumberVariable("writetimeout", writeTimeout) &&
        protocol->getNumberVariable("maxinput", maxInput) &&
        // use replyTimeout as default for pollPeriod
        protocol->getNumberVariable("replytimeout", pollPeriod) &&
        protocol->getNumberVariable("pollperiod", pollPeriod)))
    {
        return false;
    }
    if (!(protocol->getStringVariable("terminator", inTerminator, &inTerminatorDefined) &&
        protocol->getStringVariable("terminator", outTerminator, &outTerminatorDefined) &&
        protocol->getStringVariable("interminator", inTerminator, &inTerminatorDefined) &&
        protocol->getStringVariable("outterminator", outTerminator, &outTerminatorDefined) &&
        protocol->getStringVariable("separator", separator)))
    {
        return false;
    }
    if (!(protocol->getCommands(NULL, commands, this) &&
        protocol->getCommands("@init", onInit, this) &&
        protocol->getCommands("@writetimeout", onWriteTimeout, this) &&
        protocol->getCommands("@replytimeout", onReplyTimeout, this) &&
        protocol->getCommands("@readtimeout", onReadTimeout, this) &&
        protocol->getCommands("@mismatch", onMismatch, this)))
    {
        return false;
    }
    return protocol->checkUnused();
}

bool StreamCore::
compileCommand(StreamProtocolParser::Protocol* protocol,
    StreamBuffer& buffer, const char* command, const char*& args)
{
    unsigned long timeout = 0;

    if (strcmp(command, commandStr[in_cmd]) == 0)
    {
        buffer.append(in_cmd);
        if (!protocol->compileString(buffer, args,
            ScanFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, commandStr[out_cmd]) == 0)
    {
        buffer.append(out_cmd);
        if (!protocol->compileString(buffer, args,
            PrintFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, commandStr[wait_cmd]) == 0)
    {
        buffer.append(wait_cmd);
        if (!protocol->compileNumber(timeout, args))
        {
            return false;
        }
        buffer.append(&timeout, sizeof(timeout));
        return true;
    }
    if (strcmp(command, commandStr[event_cmd]) == 0)
    {
        if (!busSupportsEvent())
        {
            error(getLineNumber(command), protocol->filename(),
                    "Events not supported by businterface.\n");
            return false;
        }
        unsigned long eventmask = 0xffffffff;
        buffer.append(event_cmd);
        if (*args == '(')
        {
            if (!protocol->compileNumber(eventmask, ++args))
            {
                return false;
            }
            if (*args != ')')
            {
                error(getLineNumber(command), protocol->filename(),
                    "Expect ')' instead of: '%s'\n", args);
                return false;
            }
            args++;
        }
        buffer.append(&eventmask, sizeof(eventmask));
        if (*args)
        {
            if (!protocol->compileNumber(timeout, args))
            {
                return false;
            }
        }
        buffer.append(&timeout, sizeof(timeout));
        return true;
    }
    if (strcmp(command, commandStr[exec_cmd]) == 0)
    {
        buffer.append(exec_cmd);
        if (!protocol->compileString(buffer, args,
            PrintFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, commandStr[connect_cmd]) == 0)
    {
        buffer.append(connect_cmd);
        if (!protocol->compileNumber(timeout, args))
        {
            return false;
        }
        buffer.append(&timeout, sizeof(timeout));
        return true;
    }
    if (strcmp(command, commandStr[disconnect_cmd]) == 0)
    {
        buffer.append(disconnect_cmd);
        return true;
    }
    
    error(getLineNumber(command), protocol->filename(),
        "Unknown command name '%s'\n", command);
    return false;
}

// Run the protocol

// Input and events may come asynchonously from the bus driver
// Especially in the sequence 'out "request"; in "reply";' the
// reply can come before the 'in' command has actually started.
// For asyncronous protocols, input can come at any time.
// Thus, we must always accept input and event while the protocol
// is running or when asyncronous mode is active.
// Early input and event must be buffered until 'in' or 'event'
// start. An 'out' command must discard any early input to avoid
// problems with late input from aborted protocols.
// Async mode ends on Abort or Error or if another command comes
// after the asynchronous 'in' command.
// Input can be discarded when it is not accepted any more, i.e.
// at the end of syncronous protocols and when an asynchronous
// mode ends (but not when 'in'-only protocol finishes normally).


bool StreamCore::
startProtocol(StartMode startMode)
{
    MutexLock lock(this);
    debug("StreamCore::startProtocol(%s, startMode=%s)\n", name(),
        startMode == StartNormal ? "StartNormal" :
        startMode == StartInit ? "StartInit" :
        startMode == StartAsync ? "StartAsync" : "Invalid");
    if (!businterface)
    {
        error("%s: No businterface attached\n", name());
        return false;
    }
    flags &= ~ClearOnStart;
    switch (startMode)
    {
        case StartInit:
            flags |= InitRun;
            break;
        case StartAsync:
            if (!busSupportsAsyncRead())
            {
                error("%s: Businterface does not support async mode\n", name());
                return false;
            }
            flags |= AsyncMode;
            break;
        case StartNormal:
            break;
    }
    if (!commands)
    {
        error ("%s: No protocol loaded\n", name());
        return false;
    }
    commandIndex = (startMode == StartInit) ? onInit() : commands();
    runningHandler = Success;
    protocolStartHook();
    return evalCommand();
}

void StreamCore::
finishProtocol(ProtocolResult status)
{
    if (flags & BusPending)
    {
        error("StreamCore::finishProtocol(%s): Still waiting for %s%s%s\n",
            name(),
            flags & LockPending ? "lockSuccess() " : "",
            flags & WritePending ? "writeSuccess() " : "",
            flags & WaitPending ? "timerCallback()" : "");
        status = Fault;
    }
////    flags &= ~(AcceptInput|AcceptEvent);
    if (runningHandler)
    {
        // get original error status
        if (status == Success) status = runningHandler;
    }
    else
    {
        // save original error status
        runningHandler = status;
        // look for error handler
        char* handler;
        switch (status)
        {
            case Success:
                handler = NULL;
                break;
            case WriteTimeout:
                handler = onWriteTimeout();
                break;
            case ReplyTimeout:
                handler = onReplyTimeout();
                break;
            case ReadTimeout:
                handler = onReadTimeout();
                break;
            case ScanError:
                handler = onMismatch();
                /* reparse old input if first command in handler is 'in' */
                if (*handler == in_cmd)
                {
                    debug("reparsing input \"%s\"\n",
                        inputLine.expand()());
                    commandIndex = handler + 1;
                    if (matchInput())
                    {
                        evalCommand();
                        return;
                    }
                    handler = NULL;
                    break;
                }
                break;
            default:
                // get rid of all the rubbish whe might have collected
                unparsedInput = false;
                inputBuffer.clear();
                handler = NULL;
        }
        if (handler)
        {
            debug("starting exception handler\n");
            // execute handler
            commandIndex = handler;
            evalCommand();
            return;
        }
    }
    debug("StreamCore::finishProtocol(%s, status=%s) %sbus owner\n",
        name(),
        status==0 ? "Success" :
        status==1 ? "LockTimeout" :
        status==2 ? "WriteTimeout" :
        status==3 ? "ReplyTimeout" :
        status==4 ? "ReadTimeout" :
        status==5 ? "ScanError" :
        status==6 ? "FormatError" :
        status==7 ? "Abort" :
        status==8 ? "Fault" : "Invalid",
        flags & BusOwner ? "" : "not ");
    if (flags & BusOwner)
    {
        busUnlock();
        flags &= ~BusOwner;
    }
    busFinish();
    flags &= ~(AcceptInput|AcceptEvent);
    protocolFinishHook(status);
}

bool StreamCore::
evalCommand()
{
    if (flags & BusPending)
    {
        error("StreamCore::evalCommand(%s): Still waiting for %s%s%s",
            name(),
            flags & LockPending ? "lockSuccess() " : "",
            flags & WritePending ? "writeSuccess() " : "",
            flags & WaitPending ? "timerCallback()" : "");
        return false;
    }
    activeCommand = commandIndex;
    debug("StreamCore::evalCommand(%s): activeCommand = %s\n",
        name(), commandName(*activeCommand));
    switch (*commandIndex++)
    {
        case out_cmd:
            //// flags &= ~(AcceptInput|AcceptEvent);
            return evalOut();
        case in_cmd:
            //// flags &= ~AcceptEvent;
            return evalIn();
        case wait_cmd:
            //// flags &= ~(AcceptInput|AcceptEvent);
            return evalWait();
        case event_cmd:
            //// flags &= ~AcceptInput;
            return evalEvent();
        case exec_cmd:
            return evalExec();
        case end_cmd:
            finishProtocol(Success);
            return true;
        case connect_cmd:
            return evalConnect();
        case disconnect_cmd:
            return evalDisconnect();
        default:
            error("INTERNAL ERROR (%s): illegal command code 0x%02x\n",
                name(), *activeCommand);
            flags &= ~BusPending;
            finishProtocol(Fault);
            return false;
    }
}

// Handle 'out' command

bool StreamCore::
evalOut()
{
    // flush all unread input
    unparsedInput = false;
    inputBuffer.clear();
    if (!formatOutput())
    {
        finishProtocol(FormatError);
        return false;
    }
    outputLine.append(outTerminator);
    debug ("StreamCore::evalOut: outputLine = \"%s\"\n", outputLine.expand()());
    if (*commandIndex == in_cmd)  // prepare for early input
    {
        flags |= AcceptInput;
    }
    if (*commandIndex == event_cmd)  // prepare for early event
    {
        flags |= AcceptEvent;
    }
    if (!(flags & BusOwner))
    {
        debug ("StreamCore::evalOut(%s): lockRequest(%li)\n",
            name(), flags & InitRun ? 0 : lockTimeout);
        flags |= LockPending;
        if (!busLockRequest(flags & InitRun ? 0 : lockTimeout))
        {
            return false;
        }
        return true;
    }
    flags |= WritePending;
    if (!busWriteRequest(outputLine(), outputLine.length(), writeTimeout))
    {
        return false;
    }
    return true;
}

bool StreamCore::
formatOutput()
{
    char command;
    const char* fieldName = NULL;
    const char* formatstring;
    int formatstringlen;
    
    outputLine.clear();
    while ((command = *commandIndex++) != StreamProtocolParser::eos)
    {
        switch (command)
        {
            case StreamProtocolParser::format_field:
            {
                debug("StreamCore::formatOutput(%s): StreamProtocolParser::redirect_format\n",
                    name());
                // code layout:
                // field <eos> addrlen AddressStructure formatstring <eos> StreamFormat [info]
                fieldName = commandIndex;
                commandIndex += strlen(commandIndex)+1;
                unsigned short addrlen = extract<unsigned short>(commandIndex);
                fieldAddress.set(commandIndex, addrlen);
                commandIndex += addrlen;
                goto normal_format;
            }
            case StreamProtocolParser::format:
            {
                fieldAddress.clear();
normal_format:
                // code layout:
                // formatstring <eos> StreamFormat [info]
                formatstring = commandIndex;
                // jump after <eos>
                while (*commandIndex)
                {
                    if (*commandIndex == esc) commandIndex++;
                    commandIndex++;
                }
                formatstringlen = commandIndex-formatstring;
                commandIndex++;
                
                StreamFormat fmt = extract<StreamFormat>(commandIndex);
                fmt.info = commandIndex; // point to info string
                commandIndex += fmt.infolen;
#ifndef NO_TEMPORARY
                debug("StreamCore::formatOutput(%s): format = %%%s\n",
                    name(), StreamBuffer(formatstring, formatstringlen).expand()());
#endif

                if (fmt.type == pseudo_format)
                {
                    if (!StreamFormatConverter::find(fmt.conv)->
                        printPseudo(fmt, outputLine))
                    {
                        error("%s: Can't print pseudo value '%%%s'\n",
                            name(), formatstring);
                        return false;
                    }
                    continue;
                }
                flags &= ~Separator;
                if (!formatValue(fmt, fieldAddress ? fieldAddress() : NULL))
                {
                    StreamBuffer formatstr(formatstring, formatstringlen);
                    if (fieldAddress)
                        error("%s: Cannot format field '%s' with '%%%s'\n",
                            name(), fieldName, formatstr.expand()());
                    else
                        error("%s: Cannot format value with '%%%s'\n",
                            name(), formatstr.expand()());
                    return false;
                }
                continue;
            }
            case StreamProtocolParser::whitespace:
                outputLine.append(' ');
            case StreamProtocolParser::skip:
                continue;
            case esc:
                // escaped literal byte
                command = *commandIndex++;
            default:
                // literal byte
                outputLine.append(command);
        }
    }
    return true;
}

void StreamCore::
printSeparator()
{
    if (!(flags & Separator))
    {
        flags |= Separator;
        return;
    }
    if (!separator) return;
    long i = 0;
    for (; i < separator.length(); i++)
    {
        switch (separator[i])
        {
            case StreamProtocolParser::whitespace:
                outputLine.append(' '); // print single space
            case StreamProtocolParser::skip:
                continue;
            case esc:
                // escaped literal byte
                i++;
            default:
                // literal byte
                outputLine.append(separator[i]);
        }
    }
}

bool StreamCore::
printValue(const StreamFormat& fmt, long value)
{
    if (fmt.type != long_format && fmt.type != enum_format)
    {
        error("%s: printValue(long) called with %%%c format\n",
            name(), fmt.conv);
        return false;
    }
    printSeparator();
    if (!StreamFormatConverter::find(fmt.conv)->
        printLong(fmt, outputLine, value))
    {
        error("%s: Formatting value %li failed\n",
            name(), value);
        return false;
    }
    debug("StreamCore::printValue(%s, long): \"%s\"\n",
        name(), outputLine.expand()());
    return true;
}

bool StreamCore::
printValue(const StreamFormat& fmt, double value)
{
    if (fmt.type != double_format)
    {
        error("%s: printValue(double) called with %%%c format\n",
            name(), fmt.conv);
        return false;
    }
    printSeparator();
    if (!StreamFormatConverter::find(fmt.conv)->
        printDouble(fmt, outputLine, value))
    {
        error("%s: Formatting value %#g failed\n",
            name(), value);
        return false;
    }
    debug("StreamCore::printValue(%s, double): \"%s\"\n",
        name(), outputLine.expand()());
    return true;
}

bool StreamCore::
printValue(const StreamFormat& fmt, char* value)
{
    if (fmt.type != string_format)
    {
        error("%s: printValue(char*) called with %%%c format\n",
            name(), fmt.conv);
        return false;
    }
    printSeparator();
    if (!StreamFormatConverter::find(fmt.conv)->
        printString(fmt, outputLine, value))
    {
        StreamBuffer buffer(value);
        error("%s: Formatting value \"%s\" failed\n",
            name(), buffer.expand()());
        return false;
    }
    debug("StreamCore::printValue(%s, char*): \"%s\"\n",
        name(), outputLine.expand()());
    return true;
}

void StreamCore::
lockCallback(StreamIoStatus status)
{
    MutexLock lock(this);
    debug("StreamCore::lockCallback(%s, status=%s)\n",
        name(), status ? "Timeout" : "Success");
    if (!(flags & LockPending))
    {
        error("StreamCore::lockCallback(%s) called unexpectedly\n",
            name());
        return;
    }
    flags &= ~LockPending;
    flags |= BusOwner;
    switch (status)
    {
        case StreamIoSuccess:
            break;
        case StreamIoTimeout:
            finishProtocol(LockTimeout);
            return;
        default:
            finishProtocol(Fault);
            return;
    }
    flags |= WritePending;
    if (!busWriteRequest(outputLine(), outputLine.length(), writeTimeout))
    {
        finishProtocol(Fault);
    }
}

void StreamCore::
writeCallback(StreamIoStatus status)
{
    MutexLock lock(this);
    debug("StreamCore::writeCallback(%s, status=%s)\n",
        name(), status ? "Timeout" : "Success");
    if (!(flags & WritePending))
    {
        error("StreamCore::writeCallback(%s) called unexpectedly\n",
            name());
        return;
    }
    flags &= ~WritePending;
    if (status != StreamIoSuccess)
    {
        finishProtocol(WriteTimeout);
        return;
    }
    evalCommand();
}

const char* StreamCore::
getOutTerminator(size_t& length)
{
    if (outTerminatorDefined)
    {
        length = outTerminator.length();
        return outTerminator();
    }
    else
    {
        length = 0;
        return NULL;
    }
}

// Handle 'in' command

bool StreamCore::
evalIn()
{
    flags |= AcceptInput;
    long expectedInput;

    expectedInput = maxInput;
    if (unparsedInput)
    {
        // handle early input
        debug("StreamCore::evalIn(%s): early input: %s\n",
            name(), inputBuffer.expand()());
        expectedInput = readCallback(lastInputStatus, NULL, 0);
        if (!expectedInput)
        {
            // no more input needed
            return true;
        }
    }
    if (flags & AsyncMode)
    {
        // release bus
        if (flags & BusOwner)
        {
            debug("StreamCore::evalIn(%s): unlocking bus\n",
                name());
            busUnlock();
            flags &= ~BusOwner;
        }
        busReadRequest(pollPeriod, readTimeout,
            expectedInput, true);
        return true;
    }
    busReadRequest(replyTimeout, readTimeout,
        expectedInput, false);
    // continue with readCallback() in another thread
    return true;
}

long StreamCore::
readCallback(StreamIoStatus status,
    const void* input, long size)
// returns number of bytes to read additionally

{
    if (status < 0 || status > StreamIoFault)
    {
        error("StreamCore::readCallback(%s) called with illegal StreamIoStatus %d\n",
            name(), status);
        return 0;
    }
    MutexLock lock(this);
    lastInputStatus = status;

#ifndef NO_TEMPORARY
    debug("StreamCore::readCallback(%s, status=%s input=\"%s\", size=%ld)\n",
        name(), StreamIoStatusStr[status],
        StreamBuffer(input, size).expand()(), size);
#endif

    if (!(flags & AcceptInput))
    {
#ifdef NO_TEMPORARY
        error("StreamCore::readCallback(%s, %s) called unexpectedly\n",
            name(), StreamIoStatusStr[status]);
#else
        error("StreamCore::readCallback(%s, %s, \"%s\") called unexpectedly\n",
            name(), StreamIoStatusStr[status],
            StreamBuffer(input, size).expand()());
#endif
        return 0;
    }
////    flags &= ~AcceptInput;
    switch (status)
    {
        case StreamIoTimeout:
            // timeout is valid end if we have no terminator
            // and number of input bytes is not limited
            if (!inTerminator && !maxInput)
            {
                status = StreamIoEnd;
            }
            // else timeout might be ok if we find a terminator
            break;
        case StreamIoSuccess:
        case StreamIoEnd:
            break;
        case StreamIoNoReply:
            if (flags & AsyncMode)
            {
                // just restart in asyn mode
                debug("StreamCore::readCallback(%s) no async input: just restart\n",
                    name());
                evalIn();
                return 0;
            }
            error("%s: No reply from device within %ld ms\n",
                name(), replyTimeout);
            inputBuffer.clear();
            finishProtocol(ReplyTimeout);
            return 0;
        case StreamIoFault:
            error("%s: I/O error after reading %ld byte%s: \"%s%s\"\n",
                name(),
                inputBuffer.length(), inputBuffer.length()==1 ? "" : "s",
                inputBuffer.length() > 20 ? "..." : "",
                inputBuffer.expand(-20,20)());
            finishProtocol(Fault);
            return 0;
    }
    inputBuffer.append(input, size);
    debug("StreamCore::readCallback(%s) inputBuffer=\"%s\", size %ld\n",
        name(), inputBuffer.expand()(), inputBuffer.length());
    if (*activeCommand != in_cmd)
    {
        // early input, stop here and wait for in command
        // -- Should we limit size of inputBuffer? --
        if (inputBuffer) unparsedInput = true;
        return 0;
    }
    
    // prepare to parse the input
    const char *commandStart = commandIndex;
    long end = -1;
    long termlen = 0;
    
    if (inTerminator)
    {
        // look for terminator
        // performance issue for long inputs that come in chunks:
        // do not parse old chunks again or performance decreases to O(n^2)
        // but make sure to get all terminators in multi-line input

        long start;
        if (unparsedInput)
        {
            // multi-line input sets 'unparsedInput' and removes the line
            // remaining unparsed lines are left at start of inputBuffer
            start = 0;
        }
        else
        {
            // long line does not set 'unparsedInput' but keeps
            // already parsed chunks in inputBuffer
            // start parsing at beginning of new data
            // but beware of split terminators
            start = inputBuffer.length() - size - inTerminator.length();
            if (start < 0) start = 0;
        }
        end = inputBuffer.find(inTerminator, start);
        if (end >= 0)
        {
            termlen = inTerminator.length();
            debug("StreamCore::readCallback(%s) inTerminator %s at position %ld\n",
                name(), inTerminator.expand()(), end);
        } else {
            debug("StreamCore::readCallback(%s) inTerminator %s not found\n",
                name(), inTerminator.expand()());
        }
    }
    if (status == StreamIoEnd && end < 0)
    {
        // no terminator but end flag
        debug("StreamCore::readCallback(%s) end flag received\n",
            name());
        end = inputBuffer.length();
    }
    if (maxInput && end < 0 && (long)maxInput <= inputBuffer.length())
    {
        // no terminator but maxInput bytes read
        debug("StreamCore::readCallback(%s) maxInput size reached\n",
            name());
        end = maxInput;
    }
    if (maxInput && end > (long)maxInput)
    {
        // limit input length to maxInput (ignore terminator)
        end = maxInput;
        termlen = 0;
    }
    if (end >= 0)
    {
        // be forgiving with timeout because end is found
        if (status == StreamIoTimeout)
            status = StreamIoEnd;
    }
    if (end < 0)
    {
        // no end found
        if (status != StreamIoTimeout)
        {
            // input is incomplete - wait for more
            debug("StreamCore::readCallback(%s) wait for more input\n",
                name());
            flags |= AcceptInput;
            if (maxInput)
                return maxInput - inputBuffer.length();
            else
                return -1;
        }
        // try to parse what we got
        end = inputBuffer.length();
        if (flags & AsyncMode)
        {
            debug("StreamCore::readCallback(%s) async timeout: just restart\n",
                name());
            unparsedInput = false;
            inputBuffer.clear();
            commandIndex = commandStart;
            evalIn();
            return 0;
        }
        else
        {
            error("%s: Timeout after reading %ld byte%s \"%s%s\"\n",
                name(), end, end==1 ? "" : "s", end > 20 ? "..." : "",
                inputBuffer.expand(-20)());
        }
    }

    inputLine.set(inputBuffer(), end);
    debug("StreamCore::readCallback(%s) input line: \"%s\"\n",
        name(), inputLine.expand()());
    bool matches = matchInput();
    inputBuffer.remove(end + termlen);
    if (inputBuffer)
    {
        debug("StreamCore::readCallback(%s) unpared input left: \"%s\"\n",
            name(), inputBuffer.expand()());
        unparsedInput = true;
    }
    else
    {
        unparsedInput = false;
    }
    if (!matches)
    {
        if (status == StreamIoTimeout)
        {
            // we have not forgotten the timeout
            finishProtocol(ReadTimeout);
            return 0;
        }
        if (flags & AsyncMode)
        {
            debug("StreamCore::readCallback(%s) async match failure: just restart\n",
                name());
            commandIndex = commandStart;
            evalIn();
            return 0;
        }
        debug("StreamCore::readCallback(%s) match failure\n",
            name());
        finishProtocol(ScanError);
        return 0;
    }
    if (status == StreamIoTimeout)
    {
        // we have not forgotten the timeout
        finishProtocol(ReadTimeout);
        return 0;
    }
    // end input mode and do next command
    //// flags &= ~(AsyncMode|AcceptInput);
    // -- should we tell someone that input has finished? --
    evalCommand();
    return 0;
}

bool StreamCore::
matchInput()
{
    /* Don't write messages about matching errors if either in asynchronous
       mode (then we just wait for new matching input) or if @mismatch handler
       is installed and starts with 'in' (then we reparse the input).
    */
    char command;
    const char* fieldName = NULL;
    StreamBuffer formatstring;
    
    consumedInput = 0;
    
    while ((command = *commandIndex++) != StreamProtocolParser::eos)
    {
        switch (command)
        {
            case StreamProtocolParser::format_field:
            {
                // code layout:
                // field <StreamProtocolParser::eos> addrlen AddressStructure formatstring <StreamProtocolParser::eos> StreamFormat [info]
                fieldName = commandIndex;
                commandIndex += strlen(commandIndex)+1;
                unsigned short addrlen = extract<unsigned short>(commandIndex);
                fieldAddress.set(commandIndex, addrlen);
                commandIndex += addrlen;
                goto normal_format;
            }
            case StreamProtocolParser::format:
            {
                fieldAddress.clear();
normal_format:
                int consumed;
                // code layout:
                // formatstring <eos> StreamFormat [info]
                commandIndex = StreamProtocolParser::printString(formatstring, commandIndex);
                
                StreamFormat fmt = extract<StreamFormat>(commandIndex);
                fmt.info = commandIndex; // point to info string
                commandIndex += fmt.infolen;
#ifndef NO_TEMPORARY
                debug("StreamCore::matchInput(%s): format = \"%%%s\"\n",
                    name(), formatstring());
#endif

                if (fmt.flags & skip_flag || fmt.type == pseudo_format)
                {
                    long ldummy;
                    double ddummy;
                    switch (fmt.type)
                    {
                        case long_format:
                        case enum_format:
                            consumed = StreamFormatConverter::find(fmt.conv)->
                                scanLong(fmt, inputLine(consumedInput), ldummy);
                            break;
                        case double_format:
                            consumed = StreamFormatConverter::find(fmt.conv)->
                                scanDouble(fmt, inputLine(consumedInput), ddummy);
                            break;
                        case string_format:
                            consumed = StreamFormatConverter::find(fmt.conv)->
                                scanString(fmt, inputLine(consumedInput), NULL, 0);
                            break;
                        case pseudo_format:
                            // pass complete input
                            consumed = StreamFormatConverter::find(fmt.conv)->
                                scanPseudo(fmt, inputLine, consumedInput);
                            break;
                        default:
                            error("INTERNAL ERROR (%s): illegal format.type 0x%02x\n",
                                name(), fmt.type);
                            return false;
                    }
                    if (consumed < 0)
                    {
                        if (fmt.flags & default_flag)
                        {
                            consumed = 0;
                        }
                        else
                        {
                            if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                            {
                                error("%s: Input \"%s%s\" does not match format \"%%%s\"\n",
                                    name(), inputLine.expand(consumedInput, 20)(),
                                    inputLine.length()-consumedInput > 20 ? "..." : "",
                                    formatstring());
                            }
                            return false;
                        }
                    }
                    consumedInput += consumed;
                    break;
                }
                if (fmt.flags & compare_flag)
                {
                    outputLine.clear();
                    flags &= ~Separator;
                    if (!formatValue(fmt, fieldAddress ? fieldAddress() : NULL))
                    {
                        if (fieldAddress)
                            error("%s: Cannot format variable \"%s\" with \"%%%s\"\n",
                                name(), fieldName, formatstring());
                        else
                            error("%s: Cannot format value with \"%%%s\"\n",
                                name(), formatstring());
                        return false;
                    }
#ifndef NO_TEMPORARY
                    debug("StreamCore::matchInput(%s): compare \"%s\" with \"%s\"\n",
                        name(), inputLine.expand(consumedInput,
                            outputLine.length())(), outputLine.expand()()); 
#endif
                    if (inputLine.length() - consumedInput < outputLine.length())
                    {
                        if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                        {
                            error("%s: Input \"%s%s\" too short."
                                  " No match for format \"%%%s\" (\"%s\")\n",
                                name(), 
                                inputLine.length() > 20 ? "..." : "",
                                inputLine.expand(-20)(),
                                formatstring(),
                                outputLine.expand()());
                        }
                        return false;
                    }
                    if (!outputLine.startswith(inputLine(consumedInput),outputLine.length()))
                    {
                        if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                        {
                            error("%s: Input \"%s%s\" does not match format \"%%%s\" (\"%s\")\n",
                                name(), inputLine.expand(consumedInput, 20)(),
                                inputLine.length()-consumedInput > 20 ? "..." : "",
                                formatstring(),
                                outputLine.expand()());
                        }
                        return false;
                    }
                    consumedInput += outputLine.length();
                    break;
                }
                flags &= ~Separator;
                if (!matchValue(fmt, fieldAddress ? fieldAddress() : NULL))
                {
                    if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                    {
                        if (flags & ScanTried)
                            error("%s: Input \"%s%s\" does not match format \"%%%s\"\n",
                                name(), inputLine.expand(consumedInput, 20)(),
                                inputLine.length()-consumedInput > 20 ? "..." : "",
                                formatstring());
                        else
                            error("%s: Format \"%%%s\" has data type %s which does not match variable \"%s\".\n",
                                name(), formatstring(), StreamFormatTypeStr[fmt.type], fieldName);
                    }
                    return false;
                }
                // matchValue() has already removed consumed bytes from inputBuffer
                break;
            }
            case StreamProtocolParser::skip:
                // ignore next input byte
                consumedInput++;
                break;
            case StreamProtocolParser::whitespace:
                // any number of whitespace (including 0)
                while (isspace(inputLine[consumedInput])) consumedInput++;
                break;
            case esc:
                // escaped literal byte
                command = *commandIndex++;
            default:
                // literal byte
                if (consumedInput >= inputLine.length())
                {
                    int i = 0;
                    while (commandIndex[i] >= ' ') i++;
                    if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                    {
                        error("%s: Input \"%s%s\" too short.\n",
                            name(), 
                            inputLine.length() > 20 ? "..." : "",
                            inputLine.expand(-20)());
#ifndef NO_TEMPORARY
                        error("No match for \"%s\"\n",
                            StreamBuffer(commandIndex-1,i+1).expand()());
#endif
                    }
                    return false;
                }
                if (command != inputLine[consumedInput])
                {
                    if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
                    {
                        int i = 0;
                        while (commandIndex[i] >= ' ') i++;
                        error("%s: Input \"%s%s\" mismatch after %ld byte%s\n",
                            name(),
                            consumedInput > 10 ? "..." : "",
                            inputLine.expand(consumedInput > 10 ?
                                consumedInput-10 : 0,20)(),
                            consumedInput,
                            consumedInput==1 ? "" : "s");

#ifndef NO_TEMPORARY
                        error("%s: got \"%s\" where \"%s\" was expected\n",
                            name(),
                            inputLine.expand(consumedInput, 20)(),
                            StreamBuffer(commandIndex-1,i+1).expand()());
#endif
                    }
                    return false;
                }
                consumedInput++;
        }
    }
    long surplus = inputLine.length()-consumedInput;
    if (surplus > 0 && !(flags & IgnoreExtraInput))
    {
        if (!(flags & AsyncMode) && onMismatch[0] != in_cmd)
        {
            error("%s: %ld byte%s surplus input \"%s%s\"\n",
                name(), surplus, surplus==1 ? "" : "s",
                inputLine.expand(consumedInput, 20)(),
                surplus > 20 ? "..." : "");
                
            if (consumedInput>20)
                error("%s: after %ld byte%s \"...%s\"\n",
                    name(), consumedInput,
                    consumedInput==1 ? "" : "s",
                    inputLine.expand(consumedInput-20, 20)());
            else
                error("%s: after %ld byte%s: \"%s\"\n",
                    name(), consumedInput,
                    consumedInput==1 ? "" : "s",
                    inputLine.expand(0, consumedInput)());
        }
        return false;
    }
    return true;
}

bool StreamCore::
matchSeparator()
{
    // called before value is read, first value has Separator flag cleared
    // for second and next value set Separator flag

    if (!separator) {
        // empty separator matches
        return true;
    }
    if (!(flags & Separator))
    {
        // before first element, don't expect separator yet
        flags |= Separator;
        return true;
    }
    long i;
    long j = consumedInput;
    for (i = 0; i < separator.length(); i++)
    {
        switch (separator[i])
        {
            case StreamProtocolParser::skip:
                j++;
                continue;
            case StreamProtocolParser::whitespace:
                while (isspace(inputLine[j])) j++;
                continue;
            case esc:
                i++;
            default:
                if (separator[i] != inputLine[j])
                {
                    // no match
                    // don't complain here, just return false
                    return false;
                }
            j++;
        }
    }
    // separator successfully read
    consumedInput = j;
    return true;
}

long StreamCore::
scanValue(const StreamFormat& fmt, long& value)
{
    if (fmt.type != long_format && fmt.type != enum_format)
    {
        error("%s: scanValue(long&) called with %%%c format\n",
            name(), fmt.conv);
        return -1;
    }
    flags |= ScanTried;
    if (!matchSeparator()) return -1;
    long consumed = StreamFormatConverter::find(fmt.conv)->
        scanLong(fmt, inputLine(consumedInput), value);
    debug("StreamCore::scanValue(%s, format=%%%c, long) input=\"%s\"\n",
        name(), fmt.conv, inputLine.expand(consumedInput)());
    if (consumed < 0)
    {
        if (fmt.flags & default_flag)
        {
            value = 0;
            consumed = 0;
        }
        else return -1;
    }
    if (consumed > inputLine.length()-consumedInput) return -1;
    debug("StreamCore::scanValue(%s) scanned %li\n",
        name(), value);
    flags |= GotValue;
    return consumed;
}

long StreamCore::
scanValue(const StreamFormat& fmt, double& value)
{
    if (fmt.type != double_format)
    {
        error("%s: scanValue(double&) called with %%%c format\n",
            name(), fmt.conv);
        return -1;
    }
    flags |= ScanTried;
    if (!matchSeparator()) return -1;
    long consumed = StreamFormatConverter::find(fmt.conv)->
        scanDouble(fmt, inputLine(consumedInput), value);
    debug("StreamCore::scanValue(%s, format=%%%c, double) input=\"%s\"\n",
        name(), fmt.conv, inputLine.expand(consumedInput, 20)());
    if (consumed < 0)
    {
        if (fmt.flags & default_flag)
        {
            value = 0.0;
            consumed = 0;
        }
        else return -1;
    }
    if (consumed > inputLine.length()-consumedInput) return -1;
    debug("StreamCore::scanValue(%s) scanned %#g\n",
        name(), value);
    flags |= GotValue;
    return consumed;
}

long StreamCore::
scanValue(const StreamFormat& fmt, char* value, long maxlen)
{
    if (fmt.type != string_format)
    {
        error("%s: scanValue(char*) called with %%%c format\n",
            name(), fmt.conv);
        return -1;
    }
    if (maxlen < 0) maxlen = 0;
    flags |= ScanTried;
    if (!matchSeparator()) return -1;
    long consumed = StreamFormatConverter::find(fmt.conv)->
        scanString(fmt, inputLine(consumedInput), value, maxlen);
    debug("StreamCore::scanValue(%s, format=%%%c, char*, maxlen=%ld) input=\"%s\"\n",
        name(), fmt.conv, maxlen, inputLine.expand(consumedInput)());
    if (consumed < 0)
    {
        if (fmt.flags & default_flag)
        {
            value[0] = 0;
            consumed = 0;
        }
        else return -1;
    }
    if (consumed > inputLine.length()-consumedInput) return -1;
#ifndef NO_TEMPORARY
    debug("StreamCore::scanValue(%s) scanned \"%s\"\n",
        name(), StreamBuffer(value, maxlen).expand()());
#endif
    flags |= GotValue;
    return consumed;
}

const char* StreamCore::
getInTerminator(size_t& length)
{
    if (inTerminatorDefined)
    {
        length = inTerminator.length();
        return inTerminator();
    }
    else
    {
        length = 0;
        return NULL;
    }
}

// Handle 'event' command

bool StreamCore::
evalEvent()
{
    // code layout:
    // eventmask timeout
    unsigned long eventMask = extract<unsigned long>(commandIndex);
    unsigned long eventTimeout = extract<unsigned long>(commandIndex);
    if (flags & AsyncMode && eventTimeout == 0)
    {
        if (flags & BusOwner)
        {
            busUnlock();
            flags &= ~BusOwner;
        }
    }
    flags |= AcceptEvent;
    busAcceptEvent(eventMask, eventTimeout);
    return true;
}

void StreamCore::
eventCallback(StreamIoStatus status)
{
    if (status < 0 || status > StreamIoFault)
    {
        error("StreamCore::eventCallback(%s) called with illegal StreamIoStatus %d\n",
            name(), status);
        return;
    }
    if (!(flags & AcceptEvent))
    {
        error("StreamCore::eventCallback(%s) called unexpectedly\n",
            name());
        return;
    }
    debug("StreamCore::eventCallback(%s, status=%s)\n",
        name(), StreamIoStatusStr[status]);
    MutexLock lock(this);
    flags &= ~AcceptEvent;
    switch (status)
    {
        case StreamIoTimeout:
            error("%s: No event from device\n", name());
            finishProtocol(ReplyTimeout);
            return;
        case StreamIoSuccess:
            evalCommand();
            return;
        default:
            error("%s: Event error from device: %s\n",
                name(), StreamIoStatusStr[status]);
            finishProtocol(Fault);
            return;
    }
}

// Handle 'wait' command

bool StreamCore::
evalWait()
{
    unsigned long waitTimeout = extract<unsigned long>(commandIndex);
    flags |= WaitPending;
    startTimer(waitTimeout);
    return true;
}

void StreamCore::
timerCallback()
{
    MutexLock lock(this);
    debug ("StreamCore::timerCallback(%s)\n", name());
    if (!(flags & WaitPending))
    {
        error("StreamCore::timerCallback(%s) called unexpectedly\n",
            name());
        return;
    }
    flags &= ~WaitPending;
    evalCommand();
}


bool StreamCore::
evalExec()
{
    formatOutput();
    debug ("StreamCore::evalExec: command = \"%s\"\n", outputLine.expand()());
    // release bus
    if (flags & BusOwner)
    {
        debug("StreamCore::evalExec(%s): unlocking bus\n",
            name());
        busUnlock();
        flags &= ~BusOwner;
    }
    if (!execute())
    {
        error("%s: executing command \"%s\"\n", name(), outputLine());
        return false;
    }
    return true;
}

void StreamCore::
execCallback(StreamIoStatus status)
{
    switch (status)
    {
        case StreamIoSuccess:
            evalCommand();
            return;
        default:
            error("%s: Shell command \"%s\" failed\n",
                name(), outputLine());
            finishProtocol(Fault);
            return;
    }
}

bool StreamCore::execute()
{
    error("%s: Command 'exec' not implemented on this system\n",
        name());
    return false;
}

bool StreamCore::evalConnect()
{
    unsigned long connectTimeout = extract<unsigned long>(commandIndex);
    if (!busConnectRequest(connectTimeout))
    {
        error("%s: Connect not supported for this bus\n",
            name());
        return false;
    }
    return true;
}

void StreamCore::
connectCallback(StreamIoStatus status)
{
    switch (status)
    {
        case StreamIoSuccess:
            evalCommand();
            return;
        default:
            error("%s: Connect failed\n",
                name());
            finishProtocol(Fault);
            return;
    }
}

bool StreamCore::evalDisconnect()
{
    if (!busDisconnect())
    {
        error("%s: Cannot disconnect from this bus\n",
            name());
        finishProtocol(Fault);
        return false;
    }
    return true;
}

void StreamCore::
disconnectCallback(StreamIoStatus status)
{
    switch (status)
    {
        case StreamIoSuccess:
            evalCommand();
            return;
        default:
            error("%s: Disconnect failed\n",
                name());
            finishProtocol(Fault);
            return;
    }
}

#include "streamReferences"
