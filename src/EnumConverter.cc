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

// Enum %{string0|string1|...}

class EnumConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    int scanLong(const StreamFormat&, const char*, long&);
};

int EnumConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool)
{
    if (fmt.flags & (left_flag|sign_flag|space_flag|zero_flag|alt_flag))
    {
        error("Use of modifiers '-', '+', ' ', '0', '#'"
            "not allowed with %%{ conversion\n");
        return false;
    }
    int i = info.length(); // put maxValue here later
    info.append('\0');
    int maxValue = 0;
    while (*source)
    {
        switch (*source)
        {
            case '|':
                info.append('\0');
                if (++maxValue > 255)
                {
                    error("Too many enums (max 256)\n");
                    return false;
                }
                break;
            case '}':
                source++;
                info.append('\0');
                info[i] = maxValue;
                debug("EnumConverter::parse %d choices: %s\n",
                    maxValue+1, info.expand(i+1)());
                return enum_format;
            case esc:
                info.append(*source++);
            default:
                info.append(*source);
        }
        source++;
    }
    error("Missing '}' after %%{ format conversion\n");
    return false;
}

bool EnumConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    long maxValue = fmt.info[0]; // number of enums
    const char* s = fmt.info+1;  // first enum string
    if (value < 0 || value > maxValue)
    {
        error("Value %li out of range [0...%li]\n", value, maxValue);        
        return false;
    }
    while (value--)
    {
        while(*s)
        {
            if (*s == esc) s++;
            s++;
        }
        s++;
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
    long maxValue = fmt.info[0]; // number of enums
    const char* s = fmt.info+1; // first enum string
    int length;
    long val;
    bool match;
    for (val = 0; val <= maxValue; val++)
    {
        debug("EnumConverter::scanLong: check #%ld \"%s\"\n", val, s);
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
            debug("EnumConverter::scanLong: value %ld matches\n", val);
            value = val;
            return length;
        }
        s++;
    }
    debug("EnumConverter::scanLong: no value matches\n");
    return -1;
}

RegisterConverter (EnumConverter, "{");
