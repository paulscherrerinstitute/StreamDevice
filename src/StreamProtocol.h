/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the protocol parser of StreamDevice.                 *
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

#ifndef StreamProtocol_h
#define StreamProtocol_h

#include "StreamBuffer.h"
#include <stdio.h>

enum FormatType {NoFormat, ScanFormat, PrintFormat};

class StreamProtocolParser
{
public:

    enum Codes
    {
        eos = 0, skip, whitespace, format, format_field, last_function_code
    };

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
            FormatType formatType = NoFormat, Client* = NULL, int quoted = false);
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
