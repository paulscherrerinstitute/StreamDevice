/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2007 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the regexp format converter of StreamDevice.         *
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
#include "string.h"
#include "pcre.h"

// Perl regular expressions (PCRE)   %/regexp/

/* Notes:
 - Memory for compiled regexp is allocated in parse but never freed.
   This should not be too much of a problem unless streamReload is
   called really often before the IOC is restarted. It is not a
   run-time leak.
 - A maximum of 9 subexpressions is supported. Only one of them can
   be the result of the match.
 - vxWorks and maybe other OS don't have a PCRE library. Provide one?
*/

class RegexpConverter : public StreamFormatConverter
{
    int parse (const StreamFormat&, StreamBuffer&, const char*&, bool);
    int scanString(const StreamFormat&, const char*, char*, size_t);
};

int RegexpConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (!scanFormat)
    {
        error("Format conversion %%/regexp/ is only allowed in input formats\n");
        return false;
    }
    if (fmt.prec > 9)
    {
        error("Subexpression index %d too big (>9)\n", fmt.prec);
        return false;
    }    
    if (fmt.flags & (left_flag|space_flag|zero_flag|alt_flag))
    {
        error("Use of modifiers '-', ' ', '0', '#'"
            "not allowed with %%/regexp/ conversion\n");
        return false;
    }
    StreamBuffer pattern;
    while (*source != '/')
    {
        if (!*source) {
            error("Missing closing '/' after %%/ format conversion\n");
            return false;
        }
        if (*source == esc) {
            source++;
            pattern.print("\\x%02x", *source++ & 0xFF);
            continue;
        }
        pattern.append(*source++);
    }
    source++;
    debug("regexp = \"%s\"\n", pattern());
    const char* errormsg;
    int eoffset;
    pcre* code = pcre_compile(pattern(), 0, 
        &errormsg, &eoffset, NULL);
    if (!code)
    {
        error("%s after \"%s\"\n", errormsg, pattern.expand(0, eoffset)());
        return false;
    }
    info.append(&code, sizeof(code));
    return string_format;
}

int RegexpConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t maxlen)
{
    pcre* code;
    size_t len;
    int ovector[30];
    int rc;
    int subexpr = 0;
    
    memcpy (&code, fmt.info, sizeof(code));
    
    len = fmt.width > 0 ? fmt.width : strlen(input);
    subexpr = fmt.prec > 0 ? fmt.prec : 0;
    rc = pcre_exec(code, NULL, input, len, 0, 0, ovector, 30);
    if (rc < 1) return -1;
    if (fmt.flags & skip_flag) return ovector[1];
    len = ovector[subexpr*2+1] - ovector[subexpr*2];
    if (len >= maxlen) {
        if (!(fmt.flags & sign_flag)) {
            error("Regexp: Matching string \"%s\" too long (%d>%d bytes). You may want to try the + flag: \"%%+/.../\"\n",
                StreamBuffer(input+ovector[subexpr*2], len).expand()(),
                (int)len, (int)maxlen-1);
            return -1;
        }
        len = maxlen-1;
    }
    memcpy(value, input+ovector[subexpr*2], len);
    value[len]=0;
    return ovector[1];
}

RegisterConverter (RegexpConverter, "/");
