/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the binary format converter of StreamDevice.         *
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

#include <ctype.h>
#include "StreamFormatConverter.h"
#include "StreamError.h"

// Binary ASCII Converter %b and %B

class BinaryConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    int scanLong(const StreamFormat&, const char*, long&);
};

int BinaryConverter::
parse(const StreamFormat& format, StreamBuffer& info,
    const char*& source, bool)
{
    if (format.conv == 'B')
    {
        // user defined characters for %B (next 2 in source)
        if (!*source )
        {
            if (*source == esc) source++;
            info.append(*source++);
            if (!*source)
            {
                if (*source == esc) source++;
                info.append(*source++);
                return long_format;
            }
        }
        error("Missing characters after %%B format conversion\n");
        return false;
    }
    // default characters for %b
    info.append("01");
    return long_format;
}

bool BinaryConverter::
printLong(const StreamFormat& format, StreamBuffer& output, long value)
{
    int prec = format.prec;
    if (prec == -1)
    {
        // find number of significant bits
        prec = sizeof (value) * 8;
        while (prec && (value & (1 << (prec - 1))) == 0) prec--;
    }
    if (prec == 0) prec++; // print at least one bit
    int width = prec;
    if (format.width > width) width = format.width;
    char zero = format.info[0];
    char one = format.info[1];
    if (!(format.flags & left_flag))
    {
        // pad left
        char fill = (format.flags & zero_flag) ? zero : ' ';
        while (width > prec)
        {
            output.append(fill);
            width--;
        }
    }
    while (prec--)
    {
        output.append((value & (1 << prec)) ? one : zero);
        width--;
    }
    while (width--)
    {
        // pad right
        output.append(' ');
    }
    return true;
}

int BinaryConverter::
scanLong(const StreamFormat& format, const char* input, long& value)
{
    long val = 0;
    int width = format.width;
    if (width == 0) width = -1;
    int length = 0;
    while (isspace(input[++length])); // skip whitespaces
    char zero = format.info[0];
    char one = format.info[1];
    if (input[length] != zero && input[length] != one) return -1;
    while (width-- && (input[length] == zero || input[length] == one))
    {
        val <<= 1;
        if (input[length++] == one) val |= 1;
    }
    value = val;
    return length;
}

RegisterConverter (BinaryConverter, "bB");
