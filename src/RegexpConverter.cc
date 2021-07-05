/*************************************************************************
* This is the regular expression format converter of StreamDevice.
* Please see ../docs/ for detailed documentation.
*
* (C) 1999,2007 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "pcre.h"

#include "StreamFormatConverter.h"
#include "StreamError.h"

#define Z PRINTF_SIZE_T_PREFIX

// Perl regular expressions (PCRE) %/regexp/ and  %#/regexp/subst/

/* Notes:
 - Memory for compiled regexp is allocated in parse but never freed.
   This should not be too much of a problem unless streamReload is
   called really often before the IOC is restarted. It is not a
   run-time leak.
 - A maximum of 9 subexpressions is supported. Only one of them can
   be the result of the match.
*/

class RegexpConverter : public StreamFormatConverter
{
    int parse (const StreamFormat& fmt, StreamBuffer&, const char*&, bool);
    ssize_t scanString(const StreamFormat& fmt, const char*, char*, size_t&);
    ssize_t scanPseudo(const StreamFormat& fmt, StreamBuffer& input, size_t& cursor);
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
        error("Sub-expression index %ld too big (>9)\n", fmt.prec);
        return false;
    }

    StreamBuffer pattern;
    while (*source != '/')
    {
        if (!*source) {
            error("Missing closing '/' after %%/%s format conversion\n", pattern());
            return false;
        }
        if (*source == esc) {          // handle escaped chars
            if (*++source != '/')      // just un-escape /
            {
                pattern.append('\\');
                if ((*source & 0x7f) < 0x30) // handle control chars
                {
                    pattern.print("x%02x", *source++);
                    continue;
                }
                // fall through for PCRE codes like \B
            }
        }
        pattern.append(*source++);
    }
    source++;
    debug("regexp = \"%s\"\n", pattern.expand()());

    const char* errormsg;
    int eoffset;
    int nsubexpr;

    pcre* code = pcre_compile(pattern(), 0, &errormsg, &eoffset, NULL);
    if (!code)
    {
        error("%s after \"%s\"\n", errormsg, pattern.expand(0, eoffset)());
        return false;
    }
    pcre_fullinfo(code, NULL, PCRE_INFO_CAPTURECOUNT, &nsubexpr);
    if (fmt.prec > nsubexpr)
    {
        error("Sub-expression index is %ld but pattern has only %d sub-expression\n", fmt.prec, nsubexpr);
        return false;
    }
    info.append(&code, sizeof(code));

    if (fmt.flags & alt_flag)
    {
        StreamBuffer subst;

        debug("check for subst in \"%s\"\n", StreamBuffer(source).expand()());
        while (*source != '/')
        {
            if (!*source) {
                error("Missing closing '/' after %%#/%s/%s format conversion\n", pattern(), subst());
                return false;
            }
            if (*source == esc)
                subst.append(*source++);
            subst.append(*source++);
        }
        source++;
        debug("subst = \"%s\"\n", subst.expand()());
        info.append(subst).append('\0');
        return pseudo_format;
    }
    return string_format;
}

ssize_t RegexpConverter::
scanString(const StreamFormat& fmt, const char* input,
    char* value, size_t& size)
{
    int ovector[30];
    int rc;
    size_t l;
    const char* info = fmt.info;
    pcre* code = extract<pcre*>(info);
    size_t length = fmt.width > 0 ? fmt.width : strlen(input);
    int subexpr = fmt.prec > 0 ? fmt.prec : 0;

    if (length > INT_MAX)
        length = INT_MAX;
    debug("input = \"%s\"\n", input);
    debug("length=%" Z "u\n", length);

    rc = pcre_exec(code, NULL, input, (int)length, 0, 0, ovector, 30);
    debug("pcre_exec match \"%.*s\" result = %d\n", (int)length, input, rc);
    if ((subexpr && rc <= subexpr) || rc < 0)
    {
        // error or no match or not enough sub-expressions
        return -1;
    }
    if (fmt.flags & skip_flag) return ovector[subexpr*2+1];

    l = ovector[subexpr*2+1] - ovector[subexpr*2];
    if (l >= size) {
        if (!(fmt.flags & sign_flag)) {
            error("Regexp: Matching string \"%s\" too long (%" Z "u>%" Z "u bytes). You may want to try the + flag: \"%%+/.../\"\n",
                StreamBuffer(input + ovector[subexpr*2],l).expand()(),
                l, size-1);
            return -1;
        }
        l = size-1;
    }
    memcpy(value, input + ovector[subexpr*2], l);
    value[l] = '\0';
    size = l+1; // update number of bytes written to value
    return ovector[1]; // consume input until end of match
}

static void regsubst(const StreamFormat& fmt, StreamBuffer& buffer, size_t start)
{
    const char* subst = fmt.info;
    pcre* code = extract<pcre*>(subst);
    size_t length, c;
    int rc, l, r, rl, n;
    int ovector[30];
    StreamBuffer s;

    length = buffer.length() - start;
    if (fmt.width && fmt.width < length)
        length = fmt.width;
    if (length > INT_MAX)
        length = INT_MAX;
    if (fmt.flags & left_flag)
        start = buffer.length() - length;

    debug("regsubst buffer=\"%s\", start=%" Z "u, length=%" Z "u, subst = \"%s\"\n",
        buffer.expand()(), start, length, StreamBuffer(subst).expand()());

    for (c = 0, n = 1; c < length; n++)
    {
        rc = pcre_exec(code, NULL, buffer(start+c), (int)(length-c), 0, 0, ovector, 30);
        debug("pcre_exec match \"%s\" result = %d\n", buffer.expand(start+c, length-c)(), rc);

        if (rc < 0) // no match
        {
            debug("pcre_exec: no match\n");
            break;
        }
        l = ovector[1] - ovector[0];

        // no prec: replace all matches
        // prec with + flag: replace first prec matches
        // prec without + flag: replace only match number prec

        if ((fmt.flags & sign_flag) || n >= fmt.prec)
        {
            // replace subexpressions
            debug("before [%d]= \"%s\"\n", ovector[0], buffer.expand(start+c,ovector[0])());
            debug("match  [%d]= \"%s\"\n", l, buffer.expand(start+c+ovector[0],l)());
            for (r = 1; r < rc; r++)
                debug("sub%d = \"%s\"\n", r, buffer.expand(start+c+ovector[r*2], ovector[r*2+1]-ovector[r*2])());
            debug("after     = \"%s\"\n", buffer.expand(start+c+ovector[1])());
            s = subst;
            debug("subs      = \"%s\"\n", s.expand()());
            for (r = 0; r < (int)s.length(); r++)
            {
                debug("check \"%s\"\n", s.expand(r)());
                if (s[r] == esc)
                {
                    unsigned char ch = s[r+1];
                    if (strchr("ulUL", ch))
                    {
                        unsigned char br = s[r+2] - '0';
                        if (br == (unsigned char)('&'-'0')) br = 0;
                        debug("found case conversion \\%c%u\n", ch, br);
                        if (br >= rc)
                        {
                            s.remove(r, 1);
                            continue;
                        }
                        br *= 2;
                        rl = ovector[br+1] - ovector[br];
                        s.replace(r, 3, buffer(start+c+ovector[br]), rl);
                        switch (ch)
                        {
                            case 'u':
                                if (islower(s[r])) s[r] = toupper(s[r]);
                                break;
                            case 'l':
                                if (isupper(s[r])) s[r] = tolower(s[r]);
                                break;
                            case 'U':
                                for (int i = 0; i < rl; i++)
                                    if (islower(s[r+i])) s[r+i] = toupper(s[r+i]);
                                break;
                            case 'L':
                                for (int i = 0; i < rl; i++)
                                    if (isupper(s[r+i])) s[r+i] = tolower(s[r+i]);
                                break;
                        }
                    }
                    else if (ch != 0 && ch < rc) // escaped 1 - 9 : replace with subexpr
                    {
                        debug("found escaped \\%u\n", ch);
                        ch *= 2;
                        rl = ovector[ch+1] - ovector[ch];
                        debug("yes, replace \\%d: \"%s\"\n", ch/2, buffer.expand(start+c+ovector[ch], rl)());
                        s.replace(r, 2, buffer(start+c+ovector[ch]), rl);
                        r += rl - 1;
                    }
                    else
                    {
                        debug("use literal \\%u\n", ch);
                        s.remove(r, 1); // just remove escape
                    }
                }
                else if (s[r] == '&') // unescaped & : replace with match
                {
                    debug("replace &: \"%s\"\n", buffer.expand(start+c+ovector[0], l)());
                    s.replace(r, 1, buffer(start+c+ovector[0]), l);
                    r += l - 1;
                }
                else continue;
                debug("subs = \"%s\"\n", s.expand()());
            }
            buffer.replace(start+c+ovector[0], l, s);
            length -= l;
            length += s.length();
            c += s.length();
        }
        c += ovector[0];
        if (l == 0)
        {
            debug("pcre_exec: empty match\n");
            c++; // Empty strings may lead to an endless loop. Match them only once.
        }
        if (n == fmt.prec) // max match reached
        {
            debug("pcre_exec: max match %d reached\n", n);
            break;
        }
    }
    debug("pcre_exec converted string: %s\n", buffer.expand()());
}

ssize_t RegexpConverter::
scanPseudo(const StreamFormat& fmt, StreamBuffer& input, size_t& cursor)
{
    /* re-write input buffer */
    regsubst(fmt, input, cursor);
    return 0;
}

bool RegexpConverter::
printPseudo(const StreamFormat& fmt, StreamBuffer& output)
{
    /* re-write output buffer */
    regsubst(fmt, output, 0);
    return true;
}

RegisterConverter (RegexpConverter, "/");
