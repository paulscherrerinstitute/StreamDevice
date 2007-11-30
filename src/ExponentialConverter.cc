/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2007 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is a custom exponential format converter for            * 
* StreamDevice.                                                *
* The number is represented as two signed integers, mantissa   *
* and exponent, like in +00011-01                              *
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
#include <math.h>

// Exponential Converter %m: format +00351-02 means +351e-2

class ExponentialConverter : public StreamFormatConverter
{
    virtual int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    virtual int scanDouble(const StreamFormat&, const char*, double&);
};

int ExponentialConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (!scanFormat)
    {
        error("At the moment for %%m format only input is implemented\n");
        return false;
    }
    return double_format;
}

int ExponentialConverter::
scanDouble(const StreamFormat& fmt, const char* input, double& value)
{
    int mantissa;
    int exponent;
    int length = -1;
    
    sscanf(input, "%d%d%n", &mantissa, &exponent, &length);
    if (fmt.flags & skip_flag) return length;
    if (length == -1) return -1;
    value = (double)(mantissa) * pow(10, exponent);
    return length;
}

RegisterConverter (ExponentialConverter, "m");

