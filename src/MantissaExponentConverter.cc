/*************************************************************************
* This is a custom exponential format converter for StreamDevice.
* A number is represented as two signed integers, mantissa and exponent,
* like in "+00011-01" (without an "E")
* Please see ../docs/ for detailed documentation.
*
* (C) 2008 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#include <math.h>

#include "StreamFormatConverter.h"
#include "StreamError.h"

// Exponential Converter %m
// Eric Berryman requested a double format that reads
// +00011-01 as 11e-01
// I.e integer mantissa and exponent without 'e' or '.'
// But why not +11000-04 ?
// For writing, I chose the following convention:
// Format precision defines number of digits in mantissa
// No leading '0' in mantissa (except for 0.0 of course)
// Number of digits in exponent is at least 2
// Format flags +, -, and space are supported in the usual way
// Flags #, 0 are not supported

class MantissaExponentConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual ssize_t scanDouble(const StreamFormat&, const char*, double&);
    virtual bool printDouble(const StreamFormat&, StreamBuffer&, double);
};

int MantissaExponentConverter::
parse(const StreamFormat&, StreamBuffer&,
    const char*&, bool)
{
    return double_format;
}

ssize_t MantissaExponentConverter::
scanDouble(const StreamFormat& fmt, const char* input, double& value)
{
    int mantissa;
    int exponent;
    int length = -1;

    sscanf(input, "%d%d%n", &mantissa, &exponent, &length);
    if (fmt.flags & skip_flag) return length;
    if (length == -1) return -1;
    value = (double)(mantissa) * pow(10.0, exponent);
    return length;
}

bool MantissaExponentConverter::
printDouble(const StreamFormat& fmt, StreamBuffer& output, double value)
{
    // Have to divide value into mantissa and exponent
    // precision field is number of characters in mantissa
    // number of characters in exponent is at least 2
    ssize_t spaces;
    StreamBuffer buf;
    int prec = fmt.prec;

    if (prec < 1) prec = 6;
    buf.print("%.*e", prec-1, fabs(value)/pow(10.0, prec-1));
    buf.remove(1,1);
    buf.remove(buf.find('e'),1);

    spaces = fmt.width-buf.length();
    if (fmt.flags & (space_flag|sign_flag) || value < 0.0) spaces--;
    if (spaces < 0) spaces = 0;
    if (!(fmt.flags & left_flag))
        output.append(' ', spaces);
    if ((fmt.flags & (space_flag|sign_flag)) == space_flag && value >= 0.0)
        output.append(' ');
    if (fmt.flags & sign_flag && value >= 0.0)
        output.append('+');
    if (value < 0.0)
        output.append('-');
    output.append(buf);
    if (fmt.flags & left_flag)
        output.append(' ', spaces);
    return true;
}

RegisterConverter (MantissaExponentConverter, "m");

