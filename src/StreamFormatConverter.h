/*************************************************************************
* This is the format converter header of StreamDevice.
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

#ifndef StreamFormatConverter_h
#define StreamFormatConverter_h

#include "StreamFormat.h"
#include "StreamBuffer.h"
#include "StreamProtocol.h"

#define esc (0x1b)

template <class C>
class StreamFormatConverterRegistrar
{
public:
    StreamFormatConverterRegistrar(const char* name, const char* provided) {
        static C prototype;
        prototype.provides(name, provided);
    }
};

class StreamFormatConverter
{
    static StreamFormatConverter* registered [];
    const char* _name;
public:
    virtual ~StreamFormatConverter();
    static int parseFormat(const char*& source, FormatType, StreamFormat&, StreamBuffer& infoString);
    const char* name() { return _name; }
    void provides(const char* name, const char* provided);
    static StreamFormatConverter* find(unsigned char c);
    virtual int parse(const StreamFormat& fmt,
        StreamBuffer& info, const char*& source, bool scanFormat) = 0;
    virtual bool printLong(const StreamFormat& fmt,
        StreamBuffer& output, long value);
    virtual bool printDouble(const StreamFormat& fmt,
        StreamBuffer& output, double value);
    virtual bool printString(const StreamFormat& fmt,
        StreamBuffer& output, const char* value);
    virtual bool printPseudo(const StreamFormat& fmt,
        StreamBuffer& output);
    virtual ssize_t scanLong(const StreamFormat& fmt,
        const char* input, long& value);
    virtual ssize_t scanDouble(const StreamFormat& fmt,
        const char* input, double& value);
    virtual ssize_t scanString(const StreamFormat& fmt,
        const char* input, char* value, size_t& size);
    virtual ssize_t scanPseudo(const StreamFormat& fmt,
        StreamBuffer& inputLine, size_t& cursor);
};

inline StreamFormatConverter* StreamFormatConverter::
find(unsigned char c) {
    return registered[c];
}

#define RegisterConverter(converter, conversions) \
template class StreamFormatConverterRegistrar<converter>; \
StreamFormatConverterRegistrar<converter> \
registrar_##converter(#converter,conversions); \
void* ref_##converter = &registrar_##converter\

/****************************************************************************
* A user defined converter class inherits public from StreamFormatConverter
* and handles one or more conversion characters.
* Print and scan of the same conversion character must be handled by the
* same class but not both need to be supported.
*
* parse()
* =======
* This function is called when the protocol is parsed (at initialisation)
* whenever one of the conversions handled by your converter is found.
* The fields fmt.conv, fmt.flags, fmt.prec, and fmt.width have
* already been filled in. If a scan format is parsed, scanFormat is true. If
* a print format is parsed, scanFormat is false.
*
* The source pointer points to the character of the format string just after
* the conversion character. You can parse additional characters if they
* belong to the format string handled by your class. Move the source pointer
* so that is points to the first character after the format string.
*
* Example:
*
*   source  source
*   before  after
*       V   V
*  "%39[1-9]constant text"
*      |
*   conversion
*   character
*
* You can write any string to info you need in print*() or scan*(). This will
* probably be necessary if you have taken characters from the format string.
*
* Return long_format, double_format, string_format, or enum_format
* depending on the datatype associated with the conversion character.
* You need not to return the same value for print and for scan formats.
* You can even return different values depending on the format string, but
* I can't imagine why anyone should do that.
*
* If the format is not a real data conversion but does other things with
* the data (append or check a checksum, encode or decode the data,...),
* return pseudo_format.
*
* Return false if there is any parse error or if print or scan is requested
* but not supported by this conversion.
*
* print[Long|Double|String|Pseudo](), scan[Long|Double|String|Pseudo]()
* =================
* Provide a print*() and/or scan*() method appropriate for the data type
* you have returned in the parse() method. That method is called whenever
* the conversion appears in an output or input, resp.
* You only need to implement the flavour of print and/or scan suitable for
* the datatype returned by parse().
*
* Now, fmt.type contains the value returned by parse(). With fmt.info() you
* will get the string you have written to info in parse() (null terminated).
* The length of the info string can be found in fmt.infolen.
*
* In print*(), append the converted value to output. Do not modify what is
* already in output (unless you really know what you're doing).
* Return true on success, false on failure.
*
* In scan*(), read the value from input and return the number of consumed
* bytes.
* In the string version, don't write more bytes than size including a
* mandatory terminating null byte. Update size with the number of bytes
* written into the value, wich may differ from the number of bytes consumed
* from the input (e.g. due to leading space). If the skip_flag is set, the
* input will be discarded. Thus, don't write to value. You also don't need
* to update size.
* Return -1 on failure.
*
*
* Register your class
* ===================
* To register your class, write
*
* RegisterConverter (your_class, "conversions");
*
* in the global context of your file.
* "conversions" is a string containing all conversion characters
* handled by your class.
*
* For examples see StreamFormatConverter.cc.
*
* HINT: Do not branch depending on the conversion character.
*       Provide multiple classes, that's more efficient.
*
****************************************************************************/

#endif
