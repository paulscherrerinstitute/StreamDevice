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

#include "StreamCore.h"
#include "StreamError.h"
#include <ctype.h>
#include <stdlib.h>

#define Z PRINTF_SIZE_T_PREFIX

/// debug functions /////////////////////////////////////////////

char* StreamCore::
printCommands(StreamBuffer& buffer, const char* c)
{
    unsigned long timeout;
    unsigned long eventnumber;
    while (1)
    {
        switch (*c++)
        {
            case end:
                return buffer();
            case in:
                buffer.append("    in \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case out:
                buffer.append("    out \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case wait:
                timeout = extract<unsigned long>(c);
                buffer.print("    wait %ld; # ms\n", timeout);
                break;
            case event:
                eventnumber = extract<unsigned long>(c);
                timeout = extract<unsigned long>(c);
                buffer.print("    event(%ld) %ld; # ms\n", eventnumber, timeout);
                break;
            case exec:
                buffer.append("    exec \"");
                c = StreamProtocolParser::printString(buffer, c);
                buffer.append("\";\n");
                break;
            case connect:
                timeout = extract<unsigned long>(c);
                buffer.print("    connect %ld; # ms\n", timeout);
                break;
            case disconnect:
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
printProtocol(FILE* file)
{
    StreamBuffer buffer;
    fprintf(file, "%s {\n", protocolname());
    fprintf(file, "  extraInput    = %s;\n",
      (flags & IgnoreExtraInput) ? "ignore" : "error");
    fprintf(file, "  lockTimeout   = %ld; # ms\n", lockTimeout);
    fprintf(file, "  readTimeout   = %ld; # ms\n", readTimeout);
    fprintf(file, "  replyTimeout  = %ld; # ms\n", replyTimeout);
    fprintf(file, "  writeTimeout  = %ld; # ms\n", writeTimeout);
    fprintf(file, "  pollPeriod    = %ld; # ms\n", pollPeriod);
    fprintf(file, "  maxInput      = %ld; # bytes\n", maxInput);
    StreamProtocolParser::printString(buffer.clear(), inTerminator());
    fprintf(file, "  inTerminator  = \"%s\";\n", buffer());
        StreamProtocolParser::printString(buffer.clear(), outTerminator());
    fprintf(file, "  outTerminator = \"%s\";\n", buffer());
        StreamProtocolParser::printString(buffer.clear(), separator());
    fprintf(file, "  separator     = \"%s\";\n", buffer());
    if (onInit)
        fprintf(file, "  @Init {\n%s  }\n",
        printCommands(buffer.clear(), onInit()));
    if (onReplyTimeout)
        fprintf(file, "  @ReplyTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onReplyTimeout()));
    if (onReadTimeout)
        fprintf(file, "  @ReadTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onReadTimeout()));
    if (onWriteTimeout)
        fprintf(file, "  @WriteTimeout {\n%s  }\n",
        printCommands(buffer.clear(), onWriteTimeout()));
    if (onMismatch)
        fprintf(file, "  @Mismatch {\n%s  }\n",
        printCommands(buffer.clear(), onMismatch()));
    fprintf(file, "\n%s}\n",
        printCommands(buffer.clear(), commands()));
}

///////////////////////////////////////////////////////////////////////////

StreamCore* StreamCore::first = NULL;

StreamCore::
StreamCore() : activeCommand(end)
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
        error("Cannot find a bus named '%s' for '%s'\n",
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
    // extract substitutions from protocolname "name ( sub1, sub2 ) "
    ssize_t i = protocolname.find('(');
    if (i < 0) i = 0;
    while (protocolname[i-1] == ' ')
        protocolname.remove(--i, 1);
    if (protocolname[i] == '(')
    {
        while (i < (ssize_t)protocolname.length())
        {
            if (protocolname[i-1] == ' ')
                protocolname.remove(--i, 1); // remove trailing space
            protocolname[i] = '\0'; // replace initial '(' and separating ',' with '\0'
            if (protocolname[i+1] == ' ')
                protocolname.remove(i+1, 1); // remove leading space
            int brackets = 0;
            do {
                i++;
                i += strcspn(protocolname(i), ",()\\");
                char c = protocolname[i];
                if (c == '(') brackets++;
                else if (c == ')') brackets--;
                else if (c == ',' && brackets <= 0) break;
                else if (c == '\\') {
                    if (protocolname[i+1] == '\\') i++; // keep '\\'
                    else protocolname.remove(i, 1); // else skip over next char
                }
            } while (i < (ssize_t)protocolname.length());
        }
        // should have closing parentheses
        if (protocolname[-1] != ')')
        {
            error("Missing ')' after substitutions '%s'\n", _protocolname);
            return false;
        }
        protocolname.truncate(-1); // remove ')'
        if (protocolname[-1] == ' ')
            protocolname.truncate(-1); // remove trailing space
        debug("StreamCore::parse \"%s\" -> \"%s\"\n", _protocolname, protocolname.expand()());
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
        return false;

    if (ignoreExtraInput) flags |= IgnoreExtraInput;

    if (!(protocol->getNumberVariable("locktimeout", lockTimeout) &&
        protocol->getNumberVariable("readtimeout", readTimeout) &&
        protocol->getNumberVariable("replytimeout", replyTimeout) &&
        protocol->getNumberVariable("writetimeout", writeTimeout) &&
        protocol->getNumberVariable("maxinput", maxInput) &&
        // use replyTimeout as default for pollPeriod
        protocol->getNumberVariable("replytimeout", pollPeriod) &&
        protocol->getNumberVariable("pollperiod", pollPeriod)))
        return false;

    if (!(protocol->getStringVariable("interminator", inTerminator, &inTerminatorDefined) &&
        protocol->getStringVariable("outterminator", outTerminator, &outTerminatorDefined) &&
        (inTerminatorDefined ||
            protocol->getStringVariable("terminator", inTerminator, &inTerminatorDefined)) &&
        (outTerminatorDefined ||
            protocol->getStringVariable("terminator", outTerminator, &outTerminatorDefined)) &&
        protocol->getStringVariable("separator", separator)))
        return false;

    if (!(protocol->getCommands(NULL, commands, this) &&
        protocol->getCommands("@init", onInit, this) &&
        protocol->getCommands("@writetimeout", onWriteTimeout, this) &&
        protocol->getCommands("@replytimeout", onReplyTimeout, this) &&
        protocol->getCommands("@readtimeout", onReadTimeout, this) &&
        protocol->getCommands("@mismatch", onMismatch, this)))
        return false;

    return protocol->checkUnused();
}

bool StreamCore::
compileCommand(StreamProtocolParser::Protocol* protocol,
    StreamBuffer& buffer, const char* command, const char*& args)
{
    unsigned long timeout = 0;

    if (strcmp(command, "in") == 0)
    {
        buffer.append(in);
        if (!protocol->compileString(buffer, args,
            ScanFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, "out") == 0)
    {
        buffer.append(out);
        if (!protocol->compileString(buffer, args,
            PrintFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, "wait") == 0)
    {
        buffer.append(wait);
        if (!protocol->compileNumber(timeout, args))
        {
            return false;
        }
        buffer.append(&timeout, sizeof(timeout));
        return true;
    }
    if (strcmp(command, "event") == 0)
    {
        if (!busSupportsEvent())
        {
            error(getLineNumber(command), protocol->filename(),
                    "Events not supported by businterface.\n");
            return false;
        }
        unsigned long eventmask = 0xffffffff;
        buffer.append(event);
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
            while (isspace(*args)) args++;
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
    if (strcmp(command, "exec") == 0)
    {
        buffer.append(exec);
        if (!protocol->compileString(buffer, args,
            PrintFormat, this))
        {
            return false;
        }
        buffer.append(StreamProtocolParser::eos);
        return true;
    }
    if (strcmp(command, "connect") == 0)
    {
        buffer.append(connect);
        if (!protocol->compileNumber(timeout, args))
        {
            return false;
        }
        buffer.append(&timeout, sizeof(timeout));
        return true;
    }
    if (strcmp(command, "disconnect") == 0)
    {
        buffer.append(disconnect);
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
    debug("StreamCore::startProtocol(%s, startMode=%s)\n",
        name(), toStr(startMode));
    if (!businterface)
    {
        error("%s: No businterface attached\n", name());
        return false;
    }
    flags &= ~ClearOnStart;
    switch (startMode)
    {
        case StartInit:
            if (!onInit) return false;
            flags |= InitRun;
            commandIndex = onInit();
            break;
        case StartAsync:
            if (!busSupportsAsyncRead())
            {
                error("%s: Businterface does not support async mode\n", name());
                return false;
            }
            flags |= AsyncMode;
        case StartNormal:
            if (!commands) return false;
            commandIndex = commands();
            break;
    }
    StreamBuffer buffer;
    runningHandler = Success;
    protocolStartHook();
    return evalCommand();
}

void StreamCore::
finishProtocol(ProtocolResult status)
{
    debug("StreamCore::finishProtocol(%s, %s) %sbus owner\n",
        name(), toStr(status), flags & BusOwner ? "" : "not ");

    if (status == Success && flags & BusPending)
    {
        error("StreamCore::finishProtocol(%s, %s): Still waiting for %s%s%s\n",
            name(), toStr(status),
            flags & LockPending ? "lockSuccess() " : "",
            flags & WritePending ? "writeSuccess() " : "",
            flags & WaitPending ? "timerCallback()" : "");
        status = Fault;
    }
    activeCommand = end;

////    flags &= ~(AcceptInput|AcceptEvent);
    if (runningHandler || flags & InitRun)
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
                if (*handler == in)
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
            case Abort:
                flags |= Aborted;
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
    activeCommand = commandIndex[0];
    debug("StreamCore::evalCommand(%s): activeCommand = %s\n",
        name(), CommandsToStr(activeCommand));
    switch (*commandIndex++)
    {
        case out:
            //// flags &= ~(AcceptInput|AcceptEvent);
            return evalOut();
        case in:
            //// flags &= ~AcceptEvent;
            return evalIn();
        case wait:
            //// flags &= ~(AcceptInput|AcceptEvent);
            return evalWait();
        case event:
            //// flags &= ~AcceptInput;
            return evalEvent();
        case exec:
            return evalExec();
        case end:
            finishProtocol(Success);
            return true;
        case connect:
            return evalConnect();
        case disconnect:
            return evalDisconnect();
        default:
            error("INTERNAL ERROR (%s): illegal command code 0x%02x\n",
                name(), activeCommand);
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
    if (*commandIndex == in)  // prepare for early input
    {
        flags |= AcceptInput;
    }
    if (*commandIndex == event)  // prepare for early event
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
            flags &= ~LockPending;
            debug ("StreamCore::evalOut(%s): lockRequest failed. Device is offline.\n",
                name());
            finishProtocol(Offline);
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
    size_t formatstringlen;

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
                debug("StreamCore::formatOutput(%s): format = %%%s\n",
                    name(), StreamBuffer(formatstring, formatstringlen).expand()());

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
    size_t i = 0;
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
    if (fmt.type != unsigned_format && fmt.type != signed_format && fmt.type != enum_format)
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
    debug("StreamCore::printValue(%s, %%%c, %ld = 0x%lx): \"%s\"\n",
        name(), fmt.conv, value, value, outputLine.expand()());
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
    debug("StreamCore::printValue(%s, %%%c, %#g): \"%s\"\n",
        name(), fmt.conv, value, outputLine.expand()());
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
    debug("StreamCore::printValue(%s, %%%c, \"%s\"): \"%s\"\n",
        name(), fmt.conv, value, outputLine.expand()());
    return true;
}

void StreamCore::
lockCallback(StreamIoStatus status)
{
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug("StreamCore::lockCallback(%s, %s)\n",
        name(), ::toStr(status));
    if (!(flags & LockPending))
    {
        error("%s: StreamCore::lockCallback(%s) called unexpectedly\n",
            name(), ::toStr(status));
        return;
    }
    flags &= ~LockPending;
    flags |= BusOwner;
    switch (status)
    {
        case StreamIoSuccess:
            break;
        case StreamIoTimeout:
            debug("%s: Cannot lock device within %ld ms, device seems to be busy\n",
                name(), lockTimeout);
            flags &= ~BusOwner;
            finishProtocol(LockTimeout);
            return;
        case StreamIoFault:
            error("%s: Locking failed because of a device fault\n",
                name());
            flags &= ~BusOwner;
            finishProtocol(LockTimeout);
            return;
        default:
            error("StreamCore::lockCallback(%s) unexpected status %s\n",
                name(), ::toStr(status));
            flags &= ~BusOwner;
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug("StreamCore::writeCallback(%s, %s)\n",
        name(), ::toStr(status));
    if (!(flags & WritePending))
    {
        error("%s: StreamCore::writeCallback(%s) called unexpectedly\n",
            name(), ::toStr(status));
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
    ssize_t expectedInput;

    expectedInput = maxInput;
    if (unparsedInput)
    {
        // handle early input
        debug("StreamCore::evalIn(%s): early input: %s\n",
            name(), inputBuffer.expand()());
        expectedInput = readCallback(lastInputStatus, NULL, 0);
        if (expectedInput == 0)
            return true;
        if (expectedInput == -1) // don't know how much
            expectedInput = 0;
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
        return busReadRequest(pollPeriod, readTimeout,
            expectedInput, true);
    }
    return busReadRequest(replyTimeout, readTimeout,
        expectedInput, false);
    // continue with readCallback() in another thread
}

ssize_t StreamCore::
readCallback(StreamIoStatus status,
    const void* input, size_t size)
// returns number of bytes to read additionally

{
    if (flags & Aborted) return 0;
    if (status < 0 || status > StreamIoFault)
    {
        error("StreamCore::readCallback(%s) called with illegal StreamIoStatus %d\n",
            name(), status);
        return 0;
    }
    MutexLock lock(this);
    lastInputStatus = status;

    debug("StreamCore::readCallback(%s, %s input=\"%s\", size=%" Z "u)\n",
        name(), ::toStr(status),
        StreamBuffer(input, size).expand()(), size);

    if (!(flags & AcceptInput))
    {
        error("%s: StreamCore::readCallback(%s, \"%s\") called unexpectedly\n",
            name(), ::toStr(status),
            StreamBuffer(input, size).expand()());
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
            error("%s: No reply within %ld ms to \"%s\"\n",
                name(), replyTimeout, outputLine.expand()());
            inputBuffer.clear();
            finishProtocol(ReplyTimeout);
            return 0;
        case StreamIoFault:
            error("%s: I/O error after reading %" Z "d byte%s: \"%s%s\"\n",
                name(),
                inputBuffer.length(), inputBuffer.length()==1 ? "" : "s",
                inputBuffer.length() > 20 ? "..." : "",
                inputBuffer.expand(-20,20)());
            finishProtocol(Fault);
            return 0;
    }
    inputBuffer.append(input, size);
    debug("StreamCore::readCallback(%s) inputBuffer=\"%s\", size %" Z "u\n",
        name(), inputBuffer.expand()(), inputBuffer.length());
    if (activeCommand != in)
    {
        // early input, stop here and wait for in command
        // -- Should we limit size of inputBuffer? --
        if (inputBuffer) unparsedInput = true;
        return 0;
    }

    // prepare to parse the input
    const char *commandStart = commandIndex;
    ssize_t end = -1;
    size_t termlen = 0;

    if (inTerminator)
    {
        // look for terminator
        // performance issue for long inputs that come in chunks:
        // do not parse old chunks again or performance decreases to O(n^2)
        // but make sure to get all terminators in multi-line input

        ssize_t start;
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
            debug("StreamCore::readCallback(%s) inTerminator %s at position %" Z "u\n",
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
    if (maxInput && end < 0 && maxInput <= inputBuffer.length())
    {
        // no terminator but maxInput bytes read
        debug("StreamCore::readCallback(%s) maxInput size %lu reached\n",
            name(), maxInput);
        end = maxInput;
    }
    if (maxInput && end > (ssize_t)maxInput)
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
                return -1; // We don't know for how much to wait
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
            error("%s: Timeout after reading %" Z "d byte%s \"%s%s\"\n",
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
                ssize_t consumed;
                // code layout:
                // formatstring <eos> StreamFormat [info]
                formatstring.clear();
                commandIndex = StreamProtocolParser::printString(formatstring, commandIndex);

                StreamFormat fmt = extract<StreamFormat>(commandIndex);
                fmt.info = commandIndex; // point to info string
                commandIndex += fmt.infolen;
                debug("StreamCore::matchInput(%s): format = \"%%%s\"\n",
                    name(), formatstring());

                if (fmt.flags & skip_flag || fmt.type == pseudo_format)
                {
                    long ldummy;
                    double ddummy;
                    size_t size=0;
                    switch (fmt.type)
                    {
                        case unsigned_format:
                        case signed_format:
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
                                scanString(fmt, inputLine(consumedInput), NULL, size);
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
                            if (!(flags & AsyncMode) && onMismatch[0] != in)
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
                    debug("StreamCore::matchInput(%s): compare \"%s\" with \"%s\"\n",
                        name(), inputLine.expand(consumedInput,
                            outputLine.length())(), outputLine.expand()());
                    if (inputLine.length() - consumedInput < outputLine.length())
                    {
                        if (!(flags & AsyncMode) && onMismatch[0] != in)
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
                        if (!(flags & AsyncMode) && onMismatch[0] != in)
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
                    if (!(flags & AsyncMode) && onMismatch[0] != in)
                    {
                        if (flags & ScanTried)
                            error("%s: Input \"%s%s\" does not match format \"%%%s\"\n",
                                name(), inputLine.expand(consumedInput, 20)(),
                                inputLine.length()-consumedInput > 20 ? "..." : "",
                                formatstring());
                        else
                            error("%s: Format \"%%%s\" has data type %s which is not supported by \"%s\".\n",
                                name(), formatstring(), StreamFormatTypeStr[fmt.type], fieldAddress ? fieldName : name());
                    }
                    return false;
                }
                // matchValue() has already removed consumed bytes from inputBuffer
                break;
            }
            case StreamProtocolParser::skip:
                // ignore next input byte (if exists)
                if (consumedInput < inputLine.length()) consumedInput++;
                break;
            case StreamProtocolParser::whitespace:
                // any number of whitespace (including 0)
                while (consumedInput < inputLine.length() && isspace(inputLine[consumedInput])) consumedInput++;
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
                    if (!(flags & AsyncMode) && onMismatch[0] != in)
                    {
                        error("%s: Input \"%s%s\" too short.\n",
                            name(),
                            inputLine.length() > 20 ? "..." : "",
                            inputLine.expand(-20)());
                        error("No match for \"%s\"\n",
                            StreamBuffer(commandIndex-1,i+1).expand()());
                    }
                    return false;
                }
                if (command != inputLine[consumedInput])
                {
                    if (!(flags & AsyncMode) && onMismatch[0] != in)
                    {
                        int i = 0;
                        while (commandIndex[i] >= ' ') i++;
                        error("%s: Input \"%s%s%s\"\n",
                            name(),
                            consumedInput > 20 ? "..." : "",
                            inputLine.expand(consumedInput > 20 ? consumedInput-20 : 0, 40)(),
                            inputLine.length() - consumedInput > 20 ? "..." : "");

                        error("%s: mismatch after %" Z "d byte%s \"%s%s\"\n",
                            name(),
                            consumedInput,
                            consumedInput==1 ? "" : "s",
                            consumedInput > 10 ? "..." : "",
                            inputLine.expand(consumedInput > 10 ? consumedInput-10 : 0,
                                consumedInput > 10 ? 10 : consumedInput)());

                        error("%s: got \"%s%s\" where \"%s\" was expected\n",
                            name(),
                            inputLine.expand(consumedInput, 10)(),
                            inputLine.length() - consumedInput > 10 ? "..." : "",
                            StreamBuffer(commandIndex-1, i+1).expand()());
                    }
                    return false;
                }
                consumedInput++;
        }
    }
    size_t surplus = inputLine.length()-consumedInput;
    if (surplus > 0 && !(flags & IgnoreExtraInput))
    {
        if (!(flags & AsyncMode) && onMismatch[0] != in)
        {
            error("%s: %" Z "d byte%s surplus input \"%s%s\"\n",
                name(), surplus, surplus==1 ? "" : "s",
                inputLine.expand(consumedInput, 20)(),
                surplus > 20 ? "..." : "");

            if (consumedInput>20)
                error("%s: after %" Z "d byte%s \"...%s\"\n",
                    name(), consumedInput,
                    consumedInput==1 ? "" : "s",
                    inputLine.expand(consumedInput-20, 20)());
            else
                error("%s: after %" Z "d byte%s: \"%s\"\n",
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
    size_t i;
    size_t j = consumedInput;
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
                    debug("StreamCore::matchSeparator(%s) separator \"%s\" not found\n",
                        name(), separator.expand()());
                    return false;
                }
                j++;
        }
    }
    // separator successfully read
    debug("StreamCore::matchSeparator(%s) separator \"%s\" found\n",
        name(), separator.expand()());
    consumedInput = j;
    return true;
}

ssize_t StreamCore::
scanValue(const StreamFormat& fmt, long& value)
{
    if (fmt.type != unsigned_format && fmt.type != signed_format && fmt.type != enum_format)
    {
        error("%s: scanValue(long&) called with %%%c format\n",
            name(), fmt.conv);
        return -1;
    }
    flags |= ScanTried;
    if (!matchSeparator()) return -1;
    ssize_t consumed = StreamFormatConverter::find(fmt.conv)->
        scanLong(fmt, inputLine(consumedInput), value);
    if (consumed < 0)
    {
        debug("StreamCore::scanValue(%s, format=%%%c, long) input=\"%s\" failed\\n",
            name(), fmt.conv, inputLine.expand(consumedInput)());
        if (fmt.flags & default_flag)
        {
            value = 0;
            consumed = 0;
        }
        else return -1;
    }
    debug("StreamCore::scanValue(%s, format=%%%c, long) input=\"%s\" value=%li\n",
        name(), fmt.conv, inputLine.expand(consumedInput, consumed)(), value);
    if (fmt.flags & fix_width_flag && (unsigned long)consumed != fmt.width) return -1;
    if ((size_t)consumed > inputLine.length()-consumedInput) return -1;
    flags |= GotValue;
    return consumed;
}

ssize_t StreamCore::
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
    ssize_t consumed = StreamFormatConverter::find(fmt.conv)->
        scanDouble(fmt, inputLine(consumedInput), value);
    if (consumed < 0)
    {
        debug("StreamCore::scanValue(%s, format=%%%c, double) input=\"%s\" failed\n",
            name(), fmt.conv, inputLine.expand(consumedInput)());
        if (fmt.flags & default_flag)
        {
            value = 0.0;
            consumed = 0;
        }
        else return -1;
    }
    debug("StreamCore::scanValue(%s, format=%%%c, double) input=\"%s\" value=%#g\n",
        name(), fmt.conv, inputLine.expand(consumedInput, consumed)(), value);
    if (fmt.flags & fix_width_flag && (consumed != (ssize_t)(fmt.width + fmt.prec + 1))) return -1;
    if ((size_t)consumed > inputLine.length()-consumedInput) return -1;
    flags |= GotValue;
    return consumed;
}

ssize_t StreamCore::
scanValue(const StreamFormat& fmt, char* value, size_t& size)
{
    if (fmt.type != string_format)
    {
        error("%s: scanValue(char*) called with %%%c format\n",
            name(), fmt.conv);
        return -1;
    }
    flags |= ScanTried;
    if (!matchSeparator()) return -1;
    ssize_t consumed = StreamFormatConverter::find(fmt.conv)->
        scanString(fmt, inputLine(consumedInput), value, size);
    if (consumed < 0)
    {
        debug("StreamCore::scanValue(%s, format=%%%c, char*, size=%" Z "d) input=\"%s\" failed\n",
            name(), fmt.conv, size, inputLine.expand(consumedInput)());
        if (fmt.flags & default_flag)
        {
            value[0] = 0;
            consumed = 0;
        }
        else return -1;
    }
    debug("StreamCore::scanValue(%s, format=%%%c, char*, size=%" Z "d) input=\"%s\" value=\"%s\"\n",
        name(), fmt.conv, size, inputLine.expand(consumedInput)(),
        StreamBuffer(value, size).expand()());
    if (fmt.flags & fix_width_flag && consumed != (ssize_t)fmt.width) return -1;
    if ((size_t)consumed > inputLine.length()-consumedInput) return -1;
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug ("StreamCore::eventCallback(%s, %s) activeCommand: %s\n",
        name(), ::toStr(status), CommandsToStr(activeCommand));

    if (!(flags & AcceptEvent))
    {
        error("%s: StreamCore::eventCallback(%s) called unexpectedly\n",
            name(), ::toStr(status));
        return;
    }
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
                name(), ::toStr(status));
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug ("StreamCore::timerCallback(%s)\n", name());
    if (!(flags & WaitPending))
    {
        error("%s: StreamCore::timerCallback() called unexpectedly\n",
            name());
        return;
    }
    flags &= ~WaitPending;
    evalCommand();
}


bool StreamCore::
evalExec()
{
    if (!formatOutput())
    {
        finishProtocol(FormatError);
        return false;
    }
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug ("StreamCore::execCallback(%s, %s) activeCommand: %s\n",
        name(), ::toStr(status), CommandsToStr(activeCommand));

    if (activeCommand != exec)
    {
        error("%s: execCallback (%s) called unexpectedly during command %s\n",
            name(), ::toStr(status), CommandsToStr(activeCommand));
    }
    else if (status != StreamIoSuccess)
    {
        error("%s: Shell command \"%s\" failed\n",
            name(), outputLine());
        finishProtocol(Fault);
    }
    else
        evalCommand();
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug ("StreamCore::connectCallback(%s, %s) activeCommand: %s\n",
        name(), ::toStr(status), CommandsToStr(activeCommand));

    if (activeCommand == end)
    {
        // asynchronous connect
        startProtocol(StartInit);
    }
    else if (activeCommand != connect)
    {
        error("%s: connectCallback(%s) called unexpectedly during command %s\n",
            name(), ::toStr(status), CommandsToStr(activeCommand));
    }
    else if (status != StreamIoSuccess)
    {
        error("%s: Connect failed\n", name());
        finishProtocol(Fault);
    }
    else
        evalCommand();
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
    if (flags & Aborted) return;
    MutexLock lock(this);
    debug ("StreamCore::disconnectCallback(%s, %s) activeCommand: %s\n",
        name(), ::toStr(status), CommandsToStr(activeCommand));

    if (activeCommand != disconnect)
    {
        // asynchronous disconnect
        flags &= ~BusPending;
        finishProtocol(Offline);
    }
    else if (status != StreamIoSuccess)
    {
        error("%s: Disconnect failed\n",
            name());
        finishProtocol(Fault);
    }
    else
        evalCommand();
}

void StreamCore::
printStatus(StreamBuffer& buffer)
{
    buffer.print("active command=%s ",
        activeCommand ? CommandsToStr(activeCommand) : "none");
    buffer.print("flags=0x%04lx", flags);
    if (flags & IgnoreExtraInput) buffer.append(" IgnoreExtraInput");
    if (flags & InitRun)          buffer.append(" InitRun");
    if (flags & AsyncMode)        buffer.append(" AsyncMode");
    if (flags & GotValue)         buffer.append(" GotValue");
    if (flags & BusOwner)         buffer.append(" BusOwner");
    if (flags & Separator)        buffer.append(" Separator");
    if (flags & ScanTried)        buffer.append(" ScanTried");
    if (flags & AcceptInput)      buffer.append(" AcceptInput");
    if (flags & AcceptEvent)      buffer.append(" AcceptEvent");
    if (flags & LockPending)      buffer.append(" LockPending");
    if (flags & WritePending)     buffer.append(" WritePending");
    if (flags & WaitPending)      buffer.append(" WaitPending");
    if (flags & Aborted)          buffer.append(" Aborted");
    busPrintStatus(buffer);
}

const char* StreamCore::
license(void)
{
    return
        "StreamDevice is free software: You can redistribute it and/or modify\n"
        "it under the terms of the GNU Lesser General Public License as published\n"
        "by the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "StreamDevice is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
        "GNU Lesser General Public License for more details\n"
        "\n"
        "You should have received a copy of the GNU Lesser General Public License\n"
        "along with StreamDevice. If not, see https://www.gnu.org/licenses/.\n";
}

#include "streamReferences"
