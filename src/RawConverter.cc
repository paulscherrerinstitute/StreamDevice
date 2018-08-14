/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the raw format converter of StreamDevice.            *
* Please refer to the HTML files in ../docs/ for a detailed    *
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

// Raw Bytes Converter %r

class RawConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    ssize_t scanLong(const StreamFormat&, const char*, long&);
};

int RawConverter::
parse(const StreamFormat& fmt, StreamBuffer&,
    const char*&, bool)
{
    return (fmt.flags & zero_flag) ? unsigned_format : signed_format;
}

bool RawConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    unsigned int prec = fmt.prec < 0 ? 1 : fmt.prec; // number of bytes from value, default 1
    unsigned long width = prec;  // number of bytes in output
    if (prec > sizeof(long)) prec=sizeof(long);
    if (fmt.width > width) width = fmt.width;

    char byte = 0;
    if (fmt.flags & alt_flag) // little endian (lsb first)
    {
        while (prec--)
        {
            byte = static_cast<char>(value);
            output.append(byte);
            value >>= 8;
            width--;
        }
        if (fmt.flags & zero_flag)
        {
            // fill with zero
            byte = 0;
        }
        else
        {
            // fill with sign
            byte = (byte & 0x80) ? 0xFF : 0x00;
        }
        while (width--)
        {
            output.append(byte);
        }
    }
    else // big endian (msb first)
    {
        if (fmt.flags & zero_flag)
        {
            // fill with zero
            byte = 0;
        }
        else
        {
            // fill with sign
            byte = ((value >> (8 * (prec-1))) & 0x80) ? 0xFF : 0x00;
        }
        while (width > prec)
        {
            output.append(byte);
            width--;
        }
        while (prec--)
        {
            output.append(static_cast<char>(value >> (8 * prec)));
        }
    }
    return true;
}

ssize_t RawConverter::
scanLong(const StreamFormat& fmt, const char* input, long& value)
{
    ssize_t consumed = 0;
    long val = 0;
    unsigned long width = fmt.width;
    if (width == 0) width = 1; // default: 1 byte
    if (fmt.flags & skip_flag)
    {
        return width; // just skip input
    }
    if (fmt.flags & alt_flag)
    {
        // little endian (lsb first)
        unsigned int shift = 0;
        while (--width && shift < sizeof(long)*8)
        {
            val |= (unsigned long)((unsigned char)input[consumed++]) << shift;
            shift += 8;
        }
        if (width == 0)
        {
            if (fmt.flags & zero_flag)
            {
                // fill with zero
                val |= (unsigned long)((unsigned char)input[consumed++]) << shift;
            }
            else
            {
                // fill with sign
                val |= ((long)(signed char)input[consumed++]) << shift;
            }
        }
        consumed += width; // ignore upper bytes not fitting in long
    }
    else
    {
        // big endian  (msb first)
        if (fmt.flags & zero_flag)
        {
            // fill with zero
            val = (unsigned char)input[consumed++];
        }
        else
        {
            // fill with sign
            val = (signed char)input[consumed++];
        }
        while (--width)
        {
            val <<= 8;
            val |= (unsigned char)input[consumed++];
        }
    }
    value = val;
    return consumed;
}

RegisterConverter (RawConverter, "r");
