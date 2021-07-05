/*************************************************************************
* This is the format converter base for StreamDevice.
* It includes the standard converters as known from printf/scanf.
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
#include <ctype.h>
#include <limits.h>

#include "StreamFormatConverter.h"
#include "StreamError.h"

StreamFormatConverter* StreamFormatConverter::
registered [256];

StreamFormatConverter::
~StreamFormatConverter()
{
}

int StreamFormatConverter::
parseFormat(const char*& source, FormatType formatType, StreamFormat& streamFormat, StreamBuffer& infoString)
{
/*
    source := [flags] [width] ['.' prec] conv [extra]
    flags := '-' | '+' | ' ' | '#' | '0' | '*' | '?' | '=' | '!'
    width := integer
    prec :=  integer
    conv := character
    extra := string
*/

    // look for flags
    streamFormat.flags = 0;
    bool loop = true;
    while (loop)
    {
        switch (*++source)
        {
            case '-':
                streamFormat.flags |= left_flag;
                break;
            case '+':
                streamFormat.flags |= sign_flag;
                break;
            case ' ':
                streamFormat.flags |= space_flag;
                break;
            case '#':
                streamFormat.flags |= alt_flag;
                break;
            case '0':
                streamFormat.flags |= zero_flag;
                break;
            case '*':
                if (formatType != ScanFormat)
                {
                    error("Use of skip modifier '*' "
                          "only allowed in input formats\n");
                    return false;
                }
                streamFormat.flags |= skip_flag;
                break;
            case '?':
                if (formatType != ScanFormat)
                {
                    error("Use of default modifier '?' "
                          "only allowed in input formats\n");
                    return false;
                }
                streamFormat.flags |= default_flag;
                break;
            case '!':
                if (formatType != ScanFormat)
                {
                    error("Use of fixed width modifier '!' "
                          "only allowed in input formats\n");
                    return false;
                }
                streamFormat.flags |= fix_width_flag;
                break;
            case '=':
                if (formatType != ScanFormat)
                {
                    error("Use of compare modifier '=' "
                          "only allowed in input formats\n");
                    return false;
                }
                streamFormat.flags |= compare_flag;
                formatType = PrintFormat;
                break;
            default:
                loop = false;
        }
    }
    // look for width
    unsigned long val;
    char* p;
    val = strtoul (source, &p, 10);
    source = p;
    if (val > LONG_MAX)
    {
        error("Field width %ld out of range\n", val);
        return false;
    }
    streamFormat.width = val;
    // look for prec
    streamFormat.prec = -1;
    if (*source == '.')
    {
        source++;
        val = strtoul(source, &p, 10);
        if (p == source)
        {
            debug("source = %s\n", source);
            error("Numeric precision field expected after '.'\n");
            return false;
        }
        source = p;
        if (val > SHRT_MAX)
        {
            error("Precision %ld out of range\n", val);
            return false;
        }
        streamFormat.prec = (short)val;
    }
    // look for converter
    streamFormat.conv = *source++;
    if (!streamFormat.conv || strchr("'\" (.0+-*?=", streamFormat.conv))
    {
        error("Missing converter character\n");
        return false;
    }
    StreamFormatConverter* converter;
    converter = StreamFormatConverter::find(streamFormat.conv);
    if (!converter)
    {
        error("No converter registered for format '%%%c'\n",
            streamFormat.conv);
        return false;
    }
    // parse format and get info string
    return converter->parse(streamFormat, infoString,
        source, formatType == ScanFormat);
}

void StreamFormatConverter::
provides(const char* name, const char* provided)
{
    _name = name;
    const unsigned char* p;
    for (p = reinterpret_cast<const unsigned char*>(provided);
        *p; p++)
    {
        registered[*p] = this;
    }
}

bool StreamFormatConverter::
printLong(const StreamFormat& fmt, StreamBuffer&, long)
{
    error("Unimplemented printLong method\n for %%%c format",
        fmt.conv);
    return false;
}

bool StreamFormatConverter::
printDouble(const StreamFormat& fmt, StreamBuffer&, double)
{
    error("Unimplemented printDouble method for %%%c format\n",
        fmt.conv);
    return false;
}

bool StreamFormatConverter::
printString(const StreamFormat& fmt, StreamBuffer&, const char*)
{
    error("Unimplemented printString method for %%%c format\n",
        fmt.conv);
    return false;
}

bool StreamFormatConverter::
printPseudo(const StreamFormat& fmt, StreamBuffer&)
{
    error("Unimplemented printPseudo method for %%%c format\n",
        fmt.conv);
    return false;
}

ssize_t StreamFormatConverter::
scanLong(const StreamFormat& fmt, const char*, long&)
{
    error("Unimplemented scanLong method for %%%c format\n",
        fmt.conv);
    return -1;
}

ssize_t StreamFormatConverter::
scanDouble(const StreamFormat& fmt, const char*, double&)
{
    error("Unimplemented scanDouble method for %%%c format\n",
        fmt.conv);
    return -1;
}

ssize_t StreamFormatConverter::
scanString(const StreamFormat& fmt, const char*, char*, size_t&)
{
    error("Unimplemented scanString method for %%%c format\n",
        fmt.conv);
    return -1;
}

ssize_t StreamFormatConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer&, size_t&)
{
    error("Unimplemented scanPseudo method for %%%c format\n",
        fmt.conv);
    return -1;
}

static void copyFormatString(StreamBuffer& info, const char* source)
{
    const char* p = source - 1;
    while (*p != '%' && *p != ')') p--;
    info.append('%');
    while (++p != source-1) if (*p != '?' && *p != '=') info.append(*p);
}

// A word on sscanf
// GNU's sscanf implementation sucks. It calls strlen on the buffer.
// That leads to a time consumption proportional to the buffer size.
// When reading huge arrays element-wise by repeatedly calling sscanf,
// the performance drops to O(n^2).
// The vxWorks implementation sucks, too. When all parsed values are skipped
// with %*, it returns -1 instead of 0 even though it was successful.

// Standard Long Converter for 'diouxX'

static ssize_t prepareval(const StreamFormat& fmt, const char*& input, bool& neg)
{
    size_t consumed = 0;
    neg = false;
    while (isspace(*input)) { input++; consumed++; }
    if (fmt.width)
    {
        // take local copy because strto* don't have width parameter
        size_t width = fmt.width;
        if (fmt.flags & space_flag)
        {
            // normally whitespace does not count to width
            // but do so if space flag is present
            width -= consumed;
        }
        strncpy((char*)fmt.info, input, width);
        ((char*)fmt.info)[width] = 0;
        input = fmt.info;
    }
    if (*input == '+')
    {
        goto skipsign;
    }
    if (*input == '-')
    {
        neg = true;
skipsign:
        input++;
        consumed++;
    }
    if (isspace(*input))
    {
        // allow space after sign only if # flag is set
        if (!(fmt.flags & alt_flag)) return -1;
    }
    return consumed;
}

class StdLongConverter : public StreamFormatConverter
{
    int parse(const StreamFormat& fmt, StreamBuffer& output, const char*& value, bool scanFormat);
    bool printLong(const StreamFormat& fmt, StreamBuffer& output, long value);
    ssize_t scanLong(const StreamFormat& fmt, const char* input, long& value);
};

int StdLongConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (scanFormat && fmt.prec >= 0)
    {
        error("Use of precision field '.%ld' not allowed with %%%c input conversion\n",
            fmt.prec, fmt.conv);
        return false;
    }
    if (scanFormat)
    {
        if (fmt.width) info.reserve(fmt.width+1);
    }
    else
    {
        copyFormatString(info, source);
        info.append('l');
        info.append(fmt.conv);
    }
    if (fmt.conv == 'd' || fmt.conv == 'i'
        || ((fmt.conv == 'x' || fmt.conv == 'o') && fmt.flags & (left_flag | sign_flag)))
        return signed_format;
    return unsigned_format;
}

bool StdLongConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    // limits %x/%X formats to number of half bytes in width.
    if (fmt.width && (fmt.conv == 'x' || fmt.conv == 'X') && fmt.width < 2*sizeof(long))
        value &= ~(-1L << (fmt.width*4));
    output.print(fmt.info, value);
    return true;
}

ssize_t StdLongConverter::
scanLong(const StreamFormat& fmt, const char* input, long& value)
{
    char* end;
    ssize_t consumed;
    bool neg;
    int base;
    long v;

    consumed = prepareval(fmt, input, neg);
    if (consumed < 0) return -1;
    switch (fmt.conv)
    {
        case 'd':
            base = 10;
            break;
        case 'o':
        case 'x':
        case 'X':
            // allow negative hex and oct numbers with - flag
            if (neg && !(fmt.flags & left_flag)) return -1;
            base = (fmt.conv == 'o') ? 8 : 16;
            break;
        case 'u':
            if (neg) return -1;
            base = 10;
            break;
        default:
            base = 0;
    }
    v = strtoul(input, &end, base);
    if (end == input) return -1;
    consumed += end-input;
    value = neg ? -v : v;
    return consumed;
}

RegisterConverter (StdLongConverter, "diouxX");

// Standard Double Converter for 'feEgG'

class StdDoubleConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual bool printDouble(const StreamFormat&, StreamBuffer&, double);
    virtual ssize_t scanDouble(const StreamFormat&, const char*, double&);
};

int StdDoubleConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (scanFormat && fmt.prec >= 0)
    {
        error("Use of precision field '.%ld' not allowed with %%%c input conversion\n",
            fmt.prec, fmt.conv);
        return false;
    }
    if (scanFormat)
    {
        if (fmt.width) info.reserve(fmt.width+1);
    }
    else
    {
        copyFormatString(info, source);
        info.append(fmt.conv);
    }
    return double_format;
}

bool StdDoubleConverter::
printDouble(const StreamFormat& fmt, StreamBuffer& output, double value)
{
    output.print(fmt.info, value);
    return true;
}

ssize_t StdDoubleConverter::
scanDouble(const StreamFormat& fmt, const char* input, double& value)
{
    char* end;
    ssize_t consumed;
    bool neg;

    consumed = prepareval(fmt, input, neg);
    if (consumed < 0) return -1;
    value = strtod(input, &end);
    if (neg) value = -value;
    if (end == input) return -1;
    consumed += end-input;
    return consumed;
}

RegisterConverter (StdDoubleConverter, "feEgG");

// Standard String Converter for 's'

class StdStringConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual bool printString(const StreamFormat&, StreamBuffer&, const char*);
    virtual ssize_t scanString(const StreamFormat&, const char*, char*, size_t&);
};

int StdStringConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (fmt.flags & sign_flag)
    {
        error("Use of modifier '+'"
            "not allowed with %%%c conversion\n",
            fmt.conv);
        return false;
    }
    if (scanFormat && fmt.prec >= 0)
    {
        error("Use of precision field '.%ld' not allowed with %%%c input conversion\n",
            fmt.prec, fmt.conv);
        return false;
    }
    copyFormatString(info, source);
    info.append(fmt.conv);
    if (scanFormat) info.append("%n");
    return string_format;
}

bool StdStringConverter::
printString(const StreamFormat& fmt, StreamBuffer& output, const char* value)
{
    if (fmt.flags & zero_flag && fmt.width)
    {
        size_t l;
        if (fmt.prec > -1)
        {
            char* p = (char*)memchr(value, 0, fmt.prec);
            if (p) l = p - value;
            else l = fmt.prec;
        }
        else l = strlen(value);
        if (!(fmt.flags & left_flag)) output.append('\0', fmt.width-l);
        output.append(value, l);
        if (fmt.flags & left_flag) output.append('\0', fmt.width-l);
    }
    else
        output.print(fmt.info, value);
    return true;
}

ssize_t StdStringConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t& size)
{
    ssize_t consumed = 0;
    size_t space_left = size;
    ssize_t width = fmt.width;

    if ((fmt.flags & skip_flag) || value == NULL) space_left = 0;

    // if user does not specify width assume "infinity" (-1)
    if (width == 0)
    {
        if (fmt.conv == 'c') width = 1;
        else width = -1;
    }

    while (isspace(*input) && width)
    {
        // normally leading whitespace does not count to width
        // but do so if space flag is present
        if (fmt.flags & space_flag)
        {
            if (space_left > 1) // keep space for terminal null byte
            {
                *value++ = *input;
                space_left--;
            }
            width--;
        }
        consumed++;
        input++;
    }
    while (*input && width)
    {
        // normally whitespace ends string
        // but don't end if # flag is present
        if (!(fmt.flags & alt_flag) && isspace(*input)) break;
        if (space_left > 1)
        {
            *value++ = *input;
            space_left--;
        }
        consumed++;
        width--;
        input++;
    }
    if (space_left)
    {
        *value = '\0';
        space_left--;
        size -= space_left; // update number of bytes copied to value
    }
    return consumed;
}

RegisterConverter (StdStringConverter, "s");

// Standard Characters Converter for 'c'

class StdCharsConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual bool printLong(const StreamFormat&, StreamBuffer&, long);
    virtual ssize_t scanString(const StreamFormat&, const char*, char*, size_t&);
};

int StdCharsConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (fmt.flags & (sign_flag|space_flag|zero_flag|alt_flag))
    {
        error("Use of modifiers '+', ' ', '0', '#' "
            "not allowed with %%c conversion\n");
        return false;
    }
    if (scanFormat && fmt.prec >= 0)
    {
        error("Use of precision field '.%ld' not allowed with %%%c input conversion\n",
            fmt.prec, fmt.conv);
        return false;
    }
    copyFormatString(info, source);
    info.append(fmt.conv);
    if (scanFormat)
    {
        info.append("%n");
        return string_format;
    }
    return unsigned_format;
}

bool StdCharsConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    output.print(fmt.info, value);
    return true;
}

ssize_t StdCharsConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t& size)
{
    size_t consumed = 0;
    size_t width = fmt.width;
    size_t space_left = size;

    if ((fmt.flags & skip_flag) || value == NULL) space_left = 0;

    // if user does not specify width assume 1
    if (width == 0) width = 1;

    while (*input && width)
    {
        if (space_left > 1) // keep space for terminal null byte
        {
            *value++ = *input;
            space_left--;
        }
        consumed++;
        width--;
        input++;
    }
    if (space_left)
    {
        *value = '\0';
        space_left--;
        size -= space_left; // update number of bytes written to value
    }
    return consumed; // return number of bytes consumed from input
}

RegisterConverter (StdCharsConverter, "c");

// Standard Charset Converter for '['

class StdCharsetConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual ssize_t scanString(const StreamFormat&, const char*, char*, size_t&);
    // no print method, %[ is readonly
};

inline void markbit(StreamBuffer& info, bool notflag, char c)
{
    char &infobyte = info[c>>3];
    char mask = 1<<(c&7);

    if (notflag)
        infobyte |= mask;
    else
        infobyte &= ~mask;
}

int StdCharsetConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (!scanFormat)
    {
        error("Format conversion %%[ is only allowed in input formats\n");
        return false;
    }
    if (fmt.flags & (left_flag|sign_flag|space_flag|zero_flag|alt_flag))
    {
        error("Use of modifiers '-', '+', ' ', '0', '#'"
            "not allowed with %%[ conversion\n");
        return false;
    }
    if (scanFormat && fmt.prec >= 0)
    {
        error("Use of precision field '.%ld' not allowed with %%%c input conversion\n",
            fmt.prec, fmt.conv);
        return false;
    }

    bool notflag = false;
    char c = 0;

    info.clear().reserve(32);
    if (*source == '^')
    {
        notflag = true;
        source++;
    }
    else
    {
        memset(info(1), 255, 32);
    }
    if (*source == ']')
    {
        markbit(info, notflag, *source++);
    }
    for (; *source && *source != ']'; source++)
    {
        if (*source == esc)
        {
            markbit(info, notflag, *++source);
            continue;
        }
        if (*source == '-' && c && source[1] && source[1] != ']')
        {
            source++;
            while (c < *source) markbit(info, notflag, c++);
            while (c > *source) markbit(info, notflag, c--);
        }
        c = *source;
        markbit(info, notflag, c);
    }
    if (!*source) {
        error("Missing ']' after %%[ format conversion\n");
        return false;
    }
    source++; // consume ']'

    return string_format;
}

ssize_t StdCharsetConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t& size)
{
    ssize_t consumed = 0;
    ssize_t width = fmt.width;
    size_t space_left = size;

    if ((fmt.flags & skip_flag) || value == NULL) space_left = 0;

    // if user does not specify width assume "infinity" (-1)
    if (width == 0) width = -1;

    while (*input && width)
    {
        if (fmt.info[*input>>3] & 1<<(*input&7)) break;
        if (space_left > 1) // keep space for terminal null byte
        {
            *value++ = *input;
            space_left--;
        }
        consumed++;
        width--;
        input++;
    }
    if (space_left)
    {
        *value = '\0';
        space_left--;
        size -= space_left; // update number of bytes written to value
    }
    return consumed;
}

RegisterConverter (StdCharsetConverter, "[");
