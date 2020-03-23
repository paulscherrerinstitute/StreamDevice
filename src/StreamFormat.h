/*************************************************************************
* This header defines the format stucture used to interface
* format converters and EPICS records to StreamDevice
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

#ifndef StreamFormat_h
#define StreamFormat_h

typedef enum {
    left_flag    = 0x01,
    sign_flag    = 0x02,
    space_flag   = 0x04,
    alt_flag     = 0x08,
    zero_flag    = 0x10,
    skip_flag    = 0x20,
    default_flag = 0x40,
    compare_flag = 0x80,
    fix_width_flag = 0x100
} StreamFormatFlag;

typedef enum {
    unsigned_format = 1,
    signed_format,
    enum_format,
    double_format,
    string_format,
    pseudo_format
} StreamFormatType;

extern const char* StreamFormatTypeStr[];

typedef struct StreamFormat
{
    char conv;
    StreamFormatType type;
    unsigned short flags;
    long prec;
    unsigned long width;
    unsigned long infolen;
    const char* info;
} StreamFormat;

#endif
