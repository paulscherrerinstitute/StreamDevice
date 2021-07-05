/*************************************************************************
* This is the enum format converter of StreamDevice.
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

#include <stdlib.h>

#include "StreamFormatConverter.h"
#include "StreamError.h"
#include "StreamProtocol.h"

// Enum %{string0|string1|...}

class EnumConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    ssize_t scanLong(const StreamFormat&, const char*, long&);
};

// info format: <numEnums><index><string>0<index><string>0...

int EnumConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (fmt.flags & (left_flag|sign_flag|space_flag|zero_flag))
    {
        error("Use of modifiers '-', '+', ' ', '0' "
            "not allowed with %%{ conversion\n");
        return false;
    }
    long numEnums = 0;
    size_t n = info.length(); // put numEnums here later
    info.append(&numEnums, sizeof(numEnums));
    long index = 0;
    size_t i = 0;
    i = info.length(); // put index here later
    info.append(&index, sizeof(index));
    while (*source)
    {
        if (*source == '=' && (fmt.flags & alt_flag))
        {
            char* p;

            if (*++source == '?')
            {
                // default choice
                if (scanFormat)
                {
                    error("Default value only allowed in output formats\n");
                    return false;
                }
                if (*++source != '}')
                {
                    error("Default value must be last\n");
                    return false;
                }
                source++;
                numEnums = -(numEnums+1);
                info.append('\0');
                memcpy(info(n), &numEnums, sizeof(numEnums));
                debug2("EnumConverter::parse %ld choices with default: %s\n",
                    -numEnums, info.expand()());
                return enum_format;
            }

            index = strtol(source, &p, 0);
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
                debug2("EnumConverter::parse %ld choices: %s\n",
                    numEnums, info.expand()());
                return enum_format;
            }
            index++;
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
    bool noDefault = numEnums >= 0;

    if (numEnums < 0) numEnums=-numEnums-1;
    while (numEnums-- && (value != index))
    {
        while (*s)
        {
            if (*s == esc) s++;
            s++;
        }
        s++;
        index = extract<long>(s);
    }
    if (numEnums == -1 && noDefault)
    {
        error("Value %li not found in enum set\n", value);
        return false;
    }
    while (*s)
    {
        if (*s == esc) s++;
        output.append(*s++);
    }
    return true;
}

ssize_t EnumConverter::
scanLong(const StreamFormat& fmt, const char* input, long& value)
{
    debug("EnumConverter::scanLong(%%%c, \"%s\")\n",
        fmt.conv, input);
    const char* s = fmt.info;
    long numEnums = extract<long>(s);
    long index;
    ssize_t consumed;
    bool match;

    while (numEnums--)
    {
        index = extract<long>(s);
        debug("EnumConverter::scanLong: check #%ld \"%s\"\n", index, s);
        consumed = 0;
        match = true;
        while (*s)
        {
            if (*s == StreamProtocolParser::skip)
            {
                s++;
                consumed++;
                continue;
            }
            if (*s == esc) s++;
            if (*s++ != input[consumed++]) match = false;
        }
        if (match)
        {
            debug("EnumConverter::scanLong: value %ld matches\n", index);
            value = index;
            return consumed;
        }
        s++;
    }
    debug("EnumConverter::scanLong: no value matches\n");
    return -1;
}

RegisterConverter (EnumConverter, "{");
