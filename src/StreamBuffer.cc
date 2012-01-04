/***************************************************************
* StreamBuffer                                                 *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is a buffer class used in StreamDevice for I/O.         *
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

#include "StreamBuffer.h"
#include "StreamError.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#if defined(__vxworks) || defined(vxWorks) || defined(_WIN32) || defined(__rtems__)
// These systems have no vsnprintf
#include <epicsStdio.h>
#define vsnprintf epicsVsnprintf
#endif

void StreamBuffer::
init(const void* s, long minsize)
{
    len = 0;
    offs = 0;
    buffer = local;
    cap = sizeof(local);
    if (minsize < 0) minsize = 0;
    if (minsize >= cap)
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
// |----free-----|####used####|-------free-------|
///|<--- offs -->|<-- len --->|<- cap-offs-len ->|
// 0            offs      offs+len              cap
//               |<-------------- minsize --------------->


void StreamBuffer::
grow(long minsize)
{
    // make space for minsize + 1 (for termination) bytes
    char* newbuffer;
    long newcap;
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
append(const void* s, long size)
{
    if (size <= 0)
    {
        // append negative number of bytes? let's delete some
        if (size < -len) size = -len;
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

long int StreamBuffer::
find(const void* m, long size, long start) const
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
    long i;
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
replace(long remstart, long remlen, const void* ins, long inslen)
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
    if (remstart > len)
    {
        // remove begins after bufferend
        remstart = len;
    }
    if (remlen >= len-remstart)
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
    long remend = remstart+remlen;
    long newlen = len+inslen-remlen;
    if (cap <= newlen)
    {
        // buffer too short
        long newcap;
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
    int printed;
    while (1)
    {
        va_start(va, fmt);
        printed = vsnprintf(buffer+offs+len, cap-offs-len, fmt, va);
        va_end(va);
        if (printed > -1 && printed < (int)(cap-offs-len))
        {
            len += printed;
            return *this;
        }
        if (printed > -1) grow(len+printed);
        else grow(len);
    }
}

StreamBuffer StreamBuffer::expand(long start, long length) const
{
    long end;
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
    StreamBuffer result((end-start)*2);
    start += offs;
    end += offs;
    long i;
    char c;
    for (i = start; i < end; i++)
    {
        c = buffer[i];
        if ((c & 0x7f) < 0x20 || (c & 0x7f) == 0x7f)
        {
            result.print("<%02x>", c & 0xff);
        }
        else
        {
            result.append(c);
        }
    }
    return result;
}

StreamBuffer StreamBuffer::
dump() const
{
    StreamBuffer result(256+cap*5);
    result.append("\033[0m");
    long i;
    result.print("%ld,%ld,%ld:\033[37m", offs, len, cap);
    for (i = 0; i < cap; i++)
    {
        if (i == offs) result.append("\033[34m[\033[0m");
        if ((buffer[i] & 0x7f) < 0x20 || (buffer[i] & 0x7f) == 0x7f)
        {
            if (i < offs || i >= offs+len)
            result.print(
                "<%02x>", buffer[i] & 0xff);
            else
            result.print(
                "\033[34m<%02x>\033[37m", buffer[i] & 0xff);
        }
        else
        {
            result.append(buffer[i]);
        }
        if (i == offs+len-1) result.append("\033[34m]\033[37m");
    }
    result.append("\033[0m");
    return result;
}
