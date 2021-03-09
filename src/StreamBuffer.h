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

#ifndef StreamBuffer_h
#define StreamBuffer_h

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

#if defined(_WIN32)
typedef ptrdiff_t ssize_t;
#endif

class StreamBuffer
{
    char local[64];
    size_t len;
    size_t cap;
    size_t offs;
    char* buffer;

    void init(const void* s, ssize_t minsize);

    void check(size_t size)
        {if (len+offs+size >= cap) grow(len+size);}

    void grow(size_t minsize);

public:
    // Hints:
    // * Any index parameter (ssize_t) can be negative
    //   meaning "count from end" (-1 is the last byte)
    // * Appending negative count deletes from end
    // * Any returned char* pointer becomes invalid when
    //   the StreamBuffer is modified.
    // * End of StreamBuffer always contains 0x00 bytes
    // * Deleting from start and clearing is fast

    StreamBuffer()
        {init(NULL, 0);}

    StreamBuffer(const void* s, ssize_t size)
        {init(s, size);}

    StreamBuffer(const char* s)
        {init(s, s?strlen(s):0);}

    StreamBuffer(const StreamBuffer& s)
        {init(s.buffer+s.offs, s.len);}

    StreamBuffer(ssize_t size)
        {init(NULL, size);}

    ~StreamBuffer()
        {if (buffer != local) delete [] buffer;}

    // operator (): get char* pointing to index
    const char* operator()(ssize_t index=0) const
        {return buffer+offs+(index<0?index+len:index);}

    char* operator()(ssize_t index=0)
        {return buffer+offs+(index<0?index+len:index);}

    // operator []: get byte at index
    char operator[](ssize_t index) const
        {return buffer[offs+(index<0?index+len:index)];}

    char& operator[](ssize_t index)
        {return buffer[offs+(index<0?index+len:index)];}

    // cast to bool: not empty?
    operator bool() const
        {return len>0;}

    // length: get current data length
    size_t length() const
        {return len;}

    // capacity: get current max data length (spare one byte for end)
    size_t capacity() const
        {return cap-1;}

    // end: get pointer to byte after last data byte
    const char* end() const
        {return buffer+offs+len;}

    // clear: set length to 0, don't free or blank memory (fast!)
    StreamBuffer& clear()
        {offs+=len; len=0; return *this;}

    // reserve: reserve size bytes of memory and return
    // pointer to that memory (for copying something to it)
    char* reserve(size_t size)
        {check(size); char* p=buffer+offs+len; len+=size; return p;}

    // append: append data at the end of the buffer
    StreamBuffer& append(char c)
        {check(1); buffer[offs+len++]=c; return *this;}

    StreamBuffer& append(char c, ssize_t count)
        {if (count < 0) truncate(count);
         else {check(count); memset(buffer+offs+len, c, count); len+=count;}
         return *this;}

    StreamBuffer& append(const void* s, ssize_t size);

    StreamBuffer& append(const char* s)
        {return append(s, s?strlen(s):0);}

    StreamBuffer& append(const StreamBuffer& s)
        {return append(s.buffer+s.offs, s.len);}

    // operator += alias for append
    StreamBuffer& operator+=(char c)
        {return append(c);}

    StreamBuffer& operator+=(const char* s)
        {return append(s);}

    StreamBuffer& operator+=(const StreamBuffer& s)
        {return append(s);}

    // set: clear buffer and fill with new data
    StreamBuffer& set(const void* s, size_t size)
        {clear(); return append(s, size);}

    StreamBuffer& set(const char* s)
        {clear(); return append(s, s?strlen(s):0);}

    StreamBuffer& set(const StreamBuffer& s)
        {clear(); return append(s.buffer+s.offs, s.len);}

    // operator = alias for set
    StreamBuffer& operator=(const char* s)
        {return set(s);}

    StreamBuffer& operator=(const StreamBuffer& s)
        {return set(s);}

    // replace: delete part of buffer (pos/length) and insert new data
    StreamBuffer& replace(
        ssize_t pos, ssize_t length, const void* s, ssize_t size);

    StreamBuffer& replace(ssize_t pos, ssize_t length, const char* s)
        {return replace(pos, length, s, s?strlen(s):0);}

    StreamBuffer& replace(ssize_t pos, ssize_t length, const StreamBuffer& s)
        {return replace(pos, length, s.buffer+s.offs, s.len);}

    // remove: delete from start/pos
    StreamBuffer& remove(ssize_t pos, ssize_t length)
        {return replace(pos, length, NULL, 0);}

    // remove from start: no memset, no function call, fast!
    StreamBuffer& remove(size_t length)
        {if (length>len) length=len;
         offs+=length; len-=length; return *this;}

    // truncate: delete end of buffer
    StreamBuffer& truncate(ssize_t pos)
        {return replace(pos, len, NULL, 0);}

    // insert: insert new data into buffer
    StreamBuffer& insert(ssize_t pos, const void* s, ssize_t size)
        {return replace(pos, 0, s, size);}

    StreamBuffer& insert(ssize_t pos, const char* s)
        {return replace(pos, 0, s, s?strlen(s):0);}

    StreamBuffer& insert(ssize_t pos, const StreamBuffer& s)
        {return replace(pos, 0, s.buffer+s.offs, s.len);}

    StreamBuffer& insert(ssize_t pos, char c)
        {return replace(pos, 0, &c, 1);}

    StreamBuffer& print(const char* fmt, ...)
        __attribute__((__format__(__printf__,2,3)));

    // find: get index of data in buffer or -1
    ssize_t find(char c, ssize_t start=0) const
        {if (start < 0 && (start -= len) < 0) start = 0;
         char* p;
         return (p = static_cast<char*>(
            memchr(buffer+offs+start, c, len-start)))? p-(buffer+offs) : -1;}

    ssize_t find(const void* s, size_t size, ssize_t start=0) const;

    ssize_t find(const char* s, ssize_t start=0) const
        {return find(s, s?strlen(s):0, start);}

    ssize_t find(const StreamBuffer& s, ssize_t start=0) const
        {return find(s.buffer+s.offs, s.len, start);}

    // startswith: returns true if first size bytes are equal
    bool startswith(const void* s, size_t size) const
        {return len>=size ? memcmp(buffer+offs, s, size) == 0 : false;}

    // startswith: returns true if first string is equal (empty string matches)
    bool startswith(const char* s) const
        {return len ? strcmp(buffer+offs, s) == 0 : !s || !*s;}

// expand: create copy of StreamBuffer where all nonprintable characters
// are replaced by <xx> with xx being the hex code of the characters
    StreamBuffer expand(ssize_t start, ssize_t length) const;

    StreamBuffer expand(ssize_t start=0) const
        {return expand(start, len);}

// dump: debug function, like expand but also show the 'hidden' memory
// before and after the real data. Uses colours.
    StreamBuffer dump() const;
};

// printf size prefix for size_t and ssize_t
#if defined (__GNUC__) && __GNUC__ >= 3
#define PRINTF_SIZE_T_PREFIX "z"
#elif defined (_WIN32)
#define PRINTF_SIZE_T_PREFIX "I"
#else
#define PRINTF_SIZE_T_PREFIX ""
#endif

#endif
