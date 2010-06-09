/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the enum format converter of StreamDevice.           *
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

#include "StreamFormatConverter.h"
#include "StreamError.h"
#include "StreamProtocol.h"
#include <stdlib.h>

// Enum %{string0|string1|...}

class EnumConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    int scanLong(const StreamFormat&, const char*, long&);
};

// info format: <numEnums><index><string>0<index><string>0...

int EnumConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool)
{
    if (fmt.flags & (left_flag|sign_flag|space_flag|zero_flag))
    {
        error("Use of modifiers '-', '+', ' ', '0' "
            "not allowed with %%{ conversion\n");
        return false;
    }
    long numEnums = 0;
    int n = info.length(); // put numEnums here later
    info.append(&numEnums, sizeof(numEnums));
    long index = 0;
    int i = 0;
    i = info.length(); // put index here later
    info.append(&index, sizeof(index));
    while (*source)
    {
        if (*source == '=' && (fmt.flags & alt_flag))
        {
            char* p;
            index = strtol(++source, &p, 0);
            if (p == source || (*p != '|' && *p != '}'))
            {
                error("Integer expected after '=' "
                    "in %%{ format conversion\n");
                return false;
            }
            memcpy(info(i), &index, sizeof(index));
            source = p;
        }
        if (*source == '|' || *source == '}')
        {
            numEnums++;
            info.append('\0');
            
            if (*source++ == '}')
            {
                memcpy(info(n), &numEnums, sizeof(numEnums));
                debug("EnumConverter::parse %ld choices: %s\n",
                    numEnums, info.expand()());
                return enum_format;
            }
            index ++;
            i = info.length();
            info.append(&index, sizeof(index));
        }
        else
        {
            if (*source == esc)
                info.append(*source++);
            info.append(*source++);
        }
    }
    error("Missing '}' after %%{ format conversion\n");
    return false;
}

bool EnumConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    const char* s = fmt.info;
    long numEnums = extract<long>(s);
    long index = extract<long>(s);
    while (numEnums-- && (value != index))
    {
        while(*s)
        {
            if (*s == esc) s++;
            s++;
        }
        s++;
        index = extract<long>(s);
    }
    if (numEnums == -1)
    {
        error("Value %li not found in enum set\n", value);
        return false;
    }
    while(*s)
    {
        if (*s == esc) s++;
        output.append(*s++);
    }
    return true;
}

int EnumConverter::
scanLong(const StreamFormat& fmt, const char* input, long& value)
{
    debug("EnumConverter::scanLong(%%%c, \"%s\")\n",
        fmt.conv, input);
    const char* s = fmt.info;
    long numEnums = extract<long>(s);
    long index;
    int length;

    bool match;
    while (numEnums--)
    {
        index = extract<long>(s);
        debug("EnumConverter::scanLong: check #%ld \"%s\"\n", index, s);
        length = 0;
        match = true;
        while(*s)
        {
            if (*s == StreamProtocolParser::skip)
            {
                s++;
                length++;
                continue;
            }
            if (*s == esc) s++;
            if (*s++ != input[length++]) match = false;
        }
        if (match)
        {
            debug("EnumConverter::scanLong: value %ld matches\n", index);
            value = index;
            return length;
        }
        s++;
    }
    debug("EnumConverter::scanLong: no value matches\n");
    return -1;
}

RegisterConverter (EnumConverter, "{");
