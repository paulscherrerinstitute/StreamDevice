/*************************************************************************
* This is a buffer class used in StreamDevice for I/O.
* Please see ../docs/ for detailed documentation.
*
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)
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

// Make sure that vsnprintf is available
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "StreamBuffer.h"
#include "StreamError.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef vxWorks
#include <version.h>
#ifndef _WRS_VXWORKS_MAJOR
// VxWorks 5 has no vsnprintf
// Implementation taken from EPICS 3.14

#include <vxWorks.h>
#include <fioLib.h>

struct outStr_s {
    char *str;
    int free;
};

static STATUS outRoutine(char *buffer, int nchars, int outarg) {
    struct outStr_s *poutStr = (struct outStr_s *) outarg;
    int free = poutStr->free;
    int len;

    if (free < 1) { /*let fioFormatV continue to count length*/
        return OK;
    } else if (free > 1) {
        len = min(free-1, nchars);
        strncpy(poutStr->str, buffer, len);
        poutStr->str += len;
        poutStr->free -= len;
    }
    /*make sure final string is null terminated*/
    *poutStr->str = 0;
    return OK;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    struct outStr_s outStr;

    outStr.str = str;
    outStr.free = size;
    return fioFormatV(format, ap, (FUNCPTR)outRoutine, (int)&outStr);
}
#endif // ! _WRS_VXWORKS_MAJOR
#endif // vxWorks

#define P PRINTF_SIZE_T_PREFIX

void StreamBuffer::
init(const void* s, ssize_t minsize)
{
    len = 0;
    offs = 0;
    buffer = local;
    cap = sizeof(local);
    if (minsize < 0) minsize = 0;
    if ((size_t)minsize >= cap)
    {
        // use allocated buffer
        grow(minsize);
    }
    else
    {
        // clear local buffer
        memset(buffer, 0, cap);
    }
    if (s) {
        len = minsize;
        memcpy(buffer, s, minsize);
    }
}

// How the buffer looks like:
// |----free-----|####used####|--------00--------|
///|<--- offs -->|<-- len --->|<- cap-offs-len ->|
// 0            offs      offs+len              cap
//               |<-------------- minsize --------------->


void StreamBuffer::
grow(size_t minsize)
{
    // make space for minsize + 1 (for termination) bytes
    char* newbuffer;
    size_t newcap;
#ifdef EXPLODE
    if (minsize > 1000000)
    {
        // crude trap against infinite grow
        error ("StreamBuffer exploded growing from %ld to %ld chars. Exiting\n",
            cap, minsize);
        int i;
        char c;
        fprintf(stderr, "String contents (len=%ld):\n", len);
        for (i = offs; i < len; i++)
        {
            c = buffer[i];
            if ((c & 0x7f) < 0x20 || (c & 0x7f) == 0x7f)
            {
                fprintf(stderr, "<%02x>", c & 0xff);
            }
            else
            {
                fprintf(stderr, "%c", c);
            }
        }
        fprintf(stderr, "\n");
        abort();
    }
#endif
    if (minsize < cap)
    {
        // just move contents to start of buffer and clear end
        // to avoid reallocation
        memmove(buffer, buffer+offs, len);
        memset(buffer+len, 0, offs);
        offs = 0;
        return;
    }
    // allocate new buffer
    for (newcap = sizeof(local)*2; newcap <= minsize; newcap *= 2);
    newbuffer = new char[newcap];
    // copy old buffer to new buffer and clear end
    memcpy(newbuffer, buffer+offs, len);
    memset(newbuffer+len, 0, newcap-len);
    if (buffer != local)
    {
        delete [] buffer;
    }
    buffer = newbuffer;
    cap = newcap;
    offs = 0;
}

StreamBuffer& StreamBuffer::
append(const void* s, ssize_t size)
{
    if (size <= 0)
    {
        // append negative number of bytes? let's delete some
        if (size < -(ssize_t)len) size = -(ssize_t)len;
        memset (buffer+offs+len+size, 0, -size);
    }
    else
    {
        check(size);
        memcpy(buffer+offs+len, s, size);
    }
    len += size;
    return *this;
}

ssize_t StreamBuffer::
find(const void* m, size_t size, ssize_t start) const
{
    if (start < 0)
    {
        start += len;
        if (start < 0) start = 0;
    }
    if (start+size > len) return -1; // find nothing after end
    if (!m || size <= 0) return start; // find empty string at start
    const char* s = static_cast<const char*>(m);
    char* b = buffer+offs;
    char* p = b+start;
    size_t i;
    while ((p = static_cast<char*>(memchr(p, s[0], b-p+len-size+1))))
    {
        for (i = 1; i < size; i++)
        {
            if (p[i] != s[i]) goto next;
        }
        return p-b;
next:   p++;
    }
    return -1;
}

StreamBuffer& StreamBuffer::
replace(ssize_t remstart, ssize_t remlen, const void* ins, ssize_t inslen)
{
    if (remstart < 0)
    {
        // remove from end
        remstart += len;
    }
    if (remlen < 0)
    {
        // remove left of remstart
        remstart += remlen;
        remlen = -remlen;
    }
    if (inslen < 0)
    {
        // handle negative inserts as additional remove
        remstart += inslen;
        remlen -= inslen;
        inslen = 0;
    }
    if (remstart < 0)
    {
        // truncate remove before bufferstart
        remlen += remstart;
        remstart = 0;
    }
    if ((size_t)remstart > len)
    {
        // remove begins after bufferend
        remstart = len;
    }
    if ((size_t)remlen >= len-remstart)
    {
        // truncate remove after bufferend
        remlen = len-remstart;
    }
    if (inslen == 0 && remstart == 0)
    {
        // optimize remove of bufferstart
        offs += remlen;
        len -= remlen;
        return *this;
    }
    if (inslen < 0) inslen = 0;
    size_t remend = remstart+remlen;
    size_t newlen = len+inslen-remlen;
    if (cap <= newlen)
    {
        // buffer too short
        size_t newcap;
        for (newcap = sizeof(local)*2; newcap <= newlen; newcap *= 2);
        char* newbuffer = new char[newcap];
        memcpy(newbuffer, buffer+offs, remstart);
        memcpy(newbuffer+remstart, ins, inslen);
        memcpy(newbuffer+remstart+inslen, buffer+offs+remend, len-remend);
        memset(newbuffer+newlen, 0, newcap-newlen);
        if (buffer != local)
        {
            delete [] buffer;
        }
        buffer = newbuffer;
        cap = newcap;
        offs = 0;
    }
    else
    {
        if (newlen+offs<=cap)
        {
            // move to start of buffer
            memmove(buffer+offs+remstart+inslen, buffer+offs+remend, len-remend);
            memcpy(buffer+offs+remstart, ins, inslen);
            if (newlen<len) memset(buffer+offs+newlen, 0, len-newlen);
        }
        else
        {
            memmove(buffer,buffer+offs,remstart);
            memmove(buffer+remstart+inslen, buffer+offs+remend, len-remend);
            memcpy(buffer+remstart, ins, inslen);
            if (newlen<len) memset(buffer+newlen, 0, len-newlen);
            offs = 0;
        }
    }
    len = newlen;
    return *this;
}

StreamBuffer& StreamBuffer::
print(const char* fmt, ...)
{
    va_list va;
    ssize_t printed;
    while (1)
    {
        va_start(va, fmt);
        printed = vsnprintf(buffer+offs+len, cap-offs-len, fmt, va);
        va_end(va);
        if (printed > -1 && printed < (ssize_t)(cap-offs-len))
        {
            len += printed;
            return *this;
        }
        if (printed > -1) grow(len+printed);
        else grow(cap*2-1);
    }
}

StreamBuffer StreamBuffer::expand(ssize_t start, ssize_t length) const
{
    size_t end;
    if (start < 0)
    {
        start += len;
    }
    if (length < 0)
    {
        start += length;
        length = -length;
    }
    if (start < 0)
    {
        length += start;
        start = 0;
    }
    end = start+length;
    if (end > len) end = len;
    StreamBuffer result;
    start += offs;
    end += offs;
    size_t i;
    char c;
    for (i = start; i < end; i++)
    {
        c = buffer[i];
        if (c < 0x20 || c >= 0x7f)
            result.print("\033[7m<%02x>\033[27m", c & 0xff);
        else
            result.append(c);
    }
    return result;
}

StreamBuffer StreamBuffer::
dump() const
{
    StreamBuffer result;
    size_t i;
    result.print("%" P "d,%" P "d,%" P "d:", offs, len, cap);
    if (offs) result.print("\033[47m");
    char c;
    for (i = 0; i < cap; i++)
    {
        c = buffer[i];
        if (offs && i == offs) result.append("\033[0m");
        if (c < 0x20 || c >= 0x7f)
            result.print("\033[7m<%02x>\033[27m", c & 0xff);
        else
            result.append(c);
        if (i == offs+len-1) result.append("\033[47m");
    }
    result.append("\033[0m");
    return result;
}
