/*************************************************************************
* This is the protocol parser of StreamDevice.
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

#ifndef StreamProtocol_h
#define StreamProtocol_h

#include <stdio.h>
#include "StreamBuffer.h"
#include "MacroMagic.h"

ENUM (FormatType,
    NoFormat, ScanFormat, PrintFormat);

class StreamProtocolParser
{
public:

    ENUM (Codes,
        eos, skip, whitespace, format, format_field, last_function_code);

    class Client;

    class Protocol
    {
        class Variable;

        friend class StreamProtocolParser;

    private:
        Protocol* next;
        Variable* variables;
        const StreamBuffer protocolname;
        StreamBuffer* commands;
        int line;
        const char* parameter[10];

        Protocol(const char* filename);
        Protocol(const Protocol& p, StreamBuffer& name, int line);
        StreamBuffer* createVariable(const char* name, int line);
        bool compileFormat(StreamBuffer&, const char*& source,
            FormatType, Client*);
        bool compileCommands(StreamBuffer&, const char*& source, Client*);
        bool replaceVariable(StreamBuffer&, const char* varname);
        const Variable* getVariable(const char* name);
        bool compileString(StreamBuffer& buffer, const char*& source,
            FormatType formatType, Client*, int quoted, int recursionDepth);

    public:

        const StreamBuffer filename;

        bool getNumberVariable(const char* varname, unsigned long& value,
            unsigned long max = 0xFFFFFFFF);
        bool getEnumVariable(const char* varname, unsigned short& value,
            const char ** enumstrings);
        bool getStringVariable(const char* varname,StreamBuffer& value, bool* defined = NULL);
        bool getCommands(const char* handlername, StreamBuffer& code, Client*);
        bool compileNumber(unsigned long& number, const char*& source,
            unsigned long max = 0xFFFFFFFF);
        bool compileString(StreamBuffer& buffer, const char*& source,
            FormatType formatType = NoFormat, Client* client = NULL, int quoted = false) {
            return compileString(buffer, source, formatType, client, quoted, 0);
        }
        bool checkUnused();
        ~Protocol();
        void report();
    };

    class Client
    {
        friend class StreamProtocolParser::Protocol;
        virtual bool compileCommand(Protocol*, StreamBuffer& buffer,
            const char* command, const char*& args) = 0;
        virtual bool getFieldAddress(const char* fieldname,
            StreamBuffer& address) = 0;
        virtual const char* name() = 0;
    public:
        virtual ~Client();
    };

private:
    StreamBuffer filename;
    FILE* file;
    int line;
    int quote;
    Protocol globalSettings;
    Protocol* protocols;
    StreamProtocolParser* next;
    static StreamProtocolParser* parsers;
    bool valid;

    StreamProtocolParser(FILE* file, const char* filename);
    Protocol* getProtocol(const StreamBuffer& protocolAndParams);
    bool isGlobalContext(const StreamBuffer* commands);
    bool isHandlerContext(Protocol&, const StreamBuffer* commands);
    static StreamProtocolParser* readFile(const char* file);
    bool parseProtocol(Protocol&, StreamBuffer* commands);
    int readChar();
    bool readToken(StreamBuffer& buffer,
        const char* specialchars = NULL, bool eofAllowed = false);
    bool parseAssignment(const char* variable, Protocol&);
    bool parseValue(StreamBuffer& buffer, bool lazy = false);

protected:
    ~StreamProtocolParser(); // get rid of cygnus-2.7.2 compiler warning

public:
    static Protocol* getProtocol(const char* file,
        const StreamBuffer& protocolAndParams);
    static void free();
    static const char* path;
    static const char* printString(StreamBuffer&, const char* string);
    void report();
};

inline int getLineNumber(const char* s)
{
    int l;
    memcpy (&l, s+strlen(s)+1, sizeof(int));
    return l;
}

template<class T>
inline const T extract(const char*& string)
{
    T p;
    memcpy (&p, string, sizeof(T));
    string += sizeof(T);
    return p;
}

/*

API functions:

NAME: getProtocol()
PURPOSE: read protocol from file, create parser if necessary
RETURNS: a copy of a protocol that must be deleted by the caller
SIDEEFFECTS: file IO, memory allocation for parser

NAME: free()
PURPOSE: free all parser resources allocated by getProtocol()
Call this function once after the last getProtocol() to clean up.

*/

#endif
