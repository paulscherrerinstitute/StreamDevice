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
    int parse (const StreamFormat& fmt, StreamBuffer&, const char*&, bool);
    int scanString(const StreamFormat& fmt, const char*, char*, size_t);
    int scanPseudo(const StreamFormat& fmt, StreamBuffer& input, long& cursor);
    bool printPseudo(const StreamFormat& fmt, StreamBuffer& output);
};

int RegexpConverter::
parse(const StreamFormat& fmt, StreamBuffer& info,
    const char*& source, bool scanFormat)
{
    if (!scanFormat && !(fmt.flags & alt_flag))
    {
        error("Format conversion %%/regexp/ is only allowed in input formats\n");
        return false;
    }
    if (fmt.prec > 9)
    {
        error("Subexpression index %d too big (>9)\n", fmt.prec);
        return false;
    }    

    StreamBuffer pattern;
    while (*source != '/')
    {
        if (!*source) {
            error("Missing closing '/' after %%/%s format conversion\n", pattern());
            return false;
        }
        if (*source == esc) {
            source++;
            pattern.append('\\');
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

    if (fmt.flags & alt_flag)
    {
        StreamBuffer subst;
        while (*source != '/')
        {
            if (!*source) {
                error("Missing closing '/' after %%#/%s/%s format conversion\n", pattern(), subst());
                return false;
            }
            if (*source == esc) {
                source++;
                subst.append('\\');
                if (*source <= 9) subst.append('0'+*source++);
                continue;
            }
            subst.append(*source++);
        }
        source++;
        debug("subst = \"%s\"\n", subst());
        info.append(subst).append('\0');
        return pseudo_format;
    }
    return string_format;
}

int RegexpConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t maxlen)
{
    int ovector[30];
    int rc;
    unsigned int l;
    
    const char* info = fmt.info;
    pcre* code = extract<pcre*>(info);
    int length = fmt.width > 0 ? fmt.width : strlen(input);
    int subexpr = fmt.prec > 0 ? fmt.prec : 0;
    
    debug("input = \"%s\"\n", input);
    debug("length=%d\n", length);
    
    rc = pcre_exec(code, NULL, input, length, 0, 0, ovector, 30);
    debug("pcre_exec match \"%.*s\" result = %d\n", length, input, rc);
    if ((subexpr && rc <= subexpr) || rc < 0)
    {
        /* error or no match or not enough sub-expressions */
        return -1;
    }
    if (fmt.flags & skip_flag) return ovector[subexpr*2+1];

    l = ovector[subexpr*2+1] - ovector[subexpr*2];
    if (l >= maxlen) {
        if (!(fmt.flags & sign_flag)) {
            error("Regexp: Matching string \"%s\" too long (%d>%ld bytes). You may want to try the + flag: \"%%+/.../\"\n",
                StreamBuffer(input + ovector[subexpr*2],l).expand()(),
                l, (long)maxlen-1);
            return -1;
        }
        l = maxlen-1;
    }
    memcpy(value, input + ovector[subexpr*2], l);
    value[l] = '\0';
    return ovector[1]; /* consume input until end of match */;
}

static void regsubst(pcre* code, StreamBuffer& buffer, long start, long length, const char* subst, int max)
{
    int rc, l, c, r, rl, n=0;
    int ovector[30];
    StreamBuffer s;
    if (length == 0)
    {
        length = buffer.length() - start;
    }
    else if (length < 0)
    {
        length = -length;
        if (length > buffer.length() - start)
            length = buffer.length() - start;
        start = buffer.length() - length;
    }
    else
    {
        if (length > buffer.length() - start)
            length = buffer.length() - start;
    }
    debug("regsubst buffer=\"%s\", start=%ld, length=%ld, subst = \"%s\", max = %d\n",
        buffer.expand()(), start, length, subst, max);
    for (c = 0; c < length; )
    {
        rc = pcre_exec(code, NULL, buffer(start+c), length-c, 0, 0, ovector, 30);
        debug("pcre_exec match \"%.*s\" result = %d\n", (int)length-c, buffer(start+c), rc);

        if (rc < 0 || (max && n++ == max))
            return; /* no match or maximum substitutions reached */
        /* replace & by match in subst */
        l = ovector[1] - ovector[0];
        debug("start = \"%s\"\n", buffer(start+c));
        debug("match = \"%.*s\"\n", l, buffer(start+c+ovector[0]));
        for (r = 1; r < rc; r++)
            debug("sub%d = \"%.*s\"\n", r, ovector[r*2+1]-ovector[r*2], buffer(start+c+ovector[r*2]));
        debug("rest  = \"%s\"\n", buffer(start+c+ovector[1]));
        s = subst;
        debug("subs = \"%s\"\n", s.expand()());
        for (r = 0; r < s.length(); r++)
        {
            debug("check \"%s\"\n", s(r));
            if (s[r] == '\\')
            {
                unsigned char ch = s[r+1];
                if (ch >= '0' && ch <= '9')
                {
                    ch = (ch - '0')*2;
                    rl = ovector[ch+1] - ovector[ch];
                    debug("replace \\%d: \"%.*s\"\n", ch/2, rl, buffer(start+c+ovector[ch]));
                    s.replace(r, 2, buffer(start+c+ovector[ch]), rl);
                    r += rl - 1;
                }
                else if (ch == '\\' || ch == '&')
                    s.remove(r, 1);
            }
            else if (s[r] == '&')
            {
                debug("replace &: \"%.*s\"\n", l,  buffer(start+c+ovector[0]));
                s.replace(r, 1, buffer(start+c+ovector[0]), l);
                r += l - 1;
            }
            else continue;
            debug("subs = \"%s\"\n", s());
        }
        buffer.replace(start+c+ovector[0], l, s);
        length += s.length() - l;
        c += s.length();
    }
}

int RegexpConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer& input, long& cursor)
{
    /* re-write input buffer */
    const char* info = fmt.info;
    pcre* code;
    long length;
    StreamBuffer subst;
    
    code = extract<pcre*>(info);
    if (fmt.flags & left_flag) length = -fmt.width;
    else length = fmt.width;
    regsubst(code, input, cursor, length, info, fmt.prec);
    return 0;
}

bool RegexpConverter::
printPseudo(const StreamFormat& fmt, StreamBuffer& output)
{
    /* re-write output buffer */
    const char* info = fmt.info;
    pcre* code;
    long length;
    StreamBuffer subst;
    
    code = extract<pcre*>(info);
    if (fmt.flags & left_flag) length = -fmt.width;
    else length = fmt.width;
    regsubst(code, output, 0, length, info, fmt.prec);
    return true;
}

RegisterConverter (RegexpConverter, "/");
