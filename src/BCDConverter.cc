/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the BCD format converter of StreamDevice.            *
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

// packed BCD (0x00 - 0x99)

class BCDConverter : public StreamFormatConverter
{
    int parse (const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printLong(const StreamFormat&, StreamBuffer&, long);
    ssize_t scanLong(const StreamFormat&, const char*, long&);
};

int BCDConverter::
parse(const StreamFormat& fmt, StreamBuffer&, const char*&, bool)
{
    return (fmt.flags & sign_flag) ? signed_format : unsigned_format;
}

bool BCDConverter::
printLong(const StreamFormat& fmt, StreamBuffer& output, long value)
{
    unsigned char bcd;
    bool neg = false;
    ssize_t i;
    unsigned long prec = fmt.prec < 0 ? 2 * sizeof(value) : fmt.prec; // number of nibbles
    unsigned long width = (prec + (fmt.flags & sign_flag ? 2 : 1)) / 2;
    if (fmt.width > width) width = fmt.width;
    if (fmt.flags & sign_flag && value < 0 && prec > 0)
    {
        neg = true;
        value = -value;
    }
    if (fmt.flags & alt_flag)
    {
        // least significant byte first (little endian)
        while (width && prec)
        {
            width--;
            bcd = value%10;
            if (--prec)
            {
                --prec;
                value /= 10;
                bcd |= (value%10)<<4;
                value /= 10;
            }
            output.append(bcd);
        }
        if (width)
            output.append('\0', width);
        if (neg) output[-1] |= 0xf0;
    }
    else
    {
        // most significant byte first (big endian)
        output.append('\0', width);
        if (neg) output[-(long)width] |= 0xf0;
        i = 0;
        while (width && prec)
        {
            width--;
            bcd = value%10;
            if (--prec)
            {
                --prec;
                value /= 10;
                bcd |= (value%10)<<4;
                value /= 10;
            }
            output[--i]=bcd;
        }
    }
    return true;
}

ssize_t BCDConverter::
scanLong(const StreamFormat& fmt, const char* input, long& value)
{
    ssize_t consumed = 0;
    long val = 0;
    unsigned char bcd1, bcd10;
    long width = fmt.width;
    if (width == 0) width = 1;
    if (fmt.flags & alt_flag)
    {
        // little endian
        int shift = 1;
        while (width--)
        {
            bcd1 = bcd10 = input[consumed++];
            bcd1 &= 0x0F;
            bcd10 >>= 4;
            if (bcd1 > 9 || shift * bcd1 < bcd1) break;
            if (width == 0 && fmt.flags & sign_flag)
            {
                val += bcd1 * shift;
                if (bcd10 != 0) val = -val;
                break;
            }
            if (bcd10 > 9 || shift * bcd10 < bcd10) break;
            val += (bcd1 + 10 * bcd10) * shift;
            if (shift <= 100000000) shift *= 100;
            else shift = 0;
        }
    }
    else
    {
        // big endian
        int sign = 1;
        while (width--)
        {
            long temp;
            bcd1 = bcd10 = input[consumed];
            bcd1 &= 0x0F;
            bcd10 >>= 4;
            if (consumed == 0 && fmt.flags & sign_flag && bcd10)
            {
                sign = -1;
                bcd10 = 0;
            }
            if (bcd1 > 9 || bcd10 > 9) break;
            temp = val * 100 + (bcd1 + 10 * bcd10);
            if (temp < val)
            {
                consumed = 0;
                break;
            }
            val = temp;
            consumed++;
        }
        val *= sign;
    }
    if (consumed == 0) return -1;
    value = val;
    return consumed;
}

RegisterConverter (BCDConverter, "D");
