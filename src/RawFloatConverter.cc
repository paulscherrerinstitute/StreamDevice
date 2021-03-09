/*************************************************************************
* This is the raw floating point format converter of StreamDevice.
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

static int endian = 0;

// Raw Float Converter %R

class RawFloatConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printDouble(const StreamFormat&, StreamBuffer&, double);
    ssize_t scanDouble(const StreamFormat&, const char*, double&);
};

int RawFloatConverter::
parse(const StreamFormat& format, StreamBuffer&,
    const char*&, bool)
{
    // Find out byte order
    if (!endian) {
        union {long l; char c [sizeof(long)];} u;
        u.l=1;
        if (u.c[0]) { endian = 1234;} // little endian
        else if (u.c[sizeof(long)-1]) { endian = 4321;} // big endian
        else {
            error ("Cannot find out byte order for %%R format.\n");
            return false;
        }
    }
    // Assume IEEE formats with 4 or 8 bytes (default: 4)
    if (format.width==0 || format.width==4 || format.width==8)
        return double_format;
    error ("Only width 4 or 8 allowed for %%R format.\n");
    return false;
}

bool RawFloatConverter::
printDouble(const StreamFormat& format, StreamBuffer& output, double value)
{
    int nbOfBytes;
    int n;
    union {
        double dval;
        float  fval;
        char   bytes[8];
    } buffer;

    nbOfBytes = format.width;
    if (nbOfBytes == 0)
        nbOfBytes = 4;

    if (nbOfBytes == 4)
        buffer.fval = (float)value;
    else
        buffer.dval = value;

    if (!(format.flags & alt_flag) ^ (endian == 4321))
    {
        // swap if byte orders differ
        for (n = nbOfBytes-1; n >= 0; n--)
        {
            output.append(buffer.bytes[n]);
        }
    } else {
        for (n = 0; n < nbOfBytes; n++)
        {
            output.append(buffer.bytes[n]);
        }
    }
    return true;
}

ssize_t RawFloatConverter::
scanDouble(const StreamFormat& format, const char* input, double& value)
{
    int nbOfBytes;
    int i, n;

    union {
        double dval;
        float  fval;
        char   bytes[8];
    } buffer;

    nbOfBytes = format.width;
    if (nbOfBytes == 0)
        nbOfBytes = 4;

    if (format.flags & skip_flag)
    {
        return(nbOfBytes); // just skip input
    }

    if (!(format.flags & alt_flag) ^ (endian == 4321))
    {
        // swap if byte orders differ
        for (n = nbOfBytes-1, i = 0; n >= 0; n--, i++)
        {
            buffer.bytes[n] = input[i];
        }
    } else {
        for (n = 0; n < nbOfBytes; n++)
        {
            buffer.bytes[n] = input[n];
        }
    }

    if (nbOfBytes == 4)
        value = buffer.fval;
    else
        value = buffer.dval;

    return nbOfBytes;
}

RegisterConverter (RawFloatConverter, "R");
