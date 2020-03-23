/*************************************************************************
* This is the raw integer format converter of StreamDevice.
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
