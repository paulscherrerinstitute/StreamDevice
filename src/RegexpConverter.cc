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

#if PCRE == 2
#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"
#else
#include "pcre.h"
#endif

#define GROUP_START(ov, i) (ov[(i)*2])
#define GROUP_END(ov, i) (ov[(i)*2+1])
#define GROUP_LEN(ov, i) (GROUP_END(ov,i)-GROUP_START(ov,i))

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

    int nsubexpr;
#if PCRE == 2
    int errorcode;
    PCRE2_SIZE eoffset;

    pcre2_code* code = pcre2_compile((PCRE2_SPTR)pattern(), PCRE2_ZERO_TERMINATED, 0, &errorcode, &eoffset, NULL);
    if (!code)
    {
        PCRE2_UCHAR errormsg[80];
        pcre2_get_error_message(errorcode, errormsg, sizeof(errormsg));
        error("%s after \"%s\"\n", errormsg, pattern.expand(0, eoffset)());
        return false;
    }
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(code, NULL);
    if (!match_data) {
        error("Out of memory\n");
        pcre2_code_free(code);
        return false;
    }
    nsubexpr = pcre2_get_ovector_count(match_data);
    if (fmt.prec > nsubexpr)
    {
        error("Sub-expression index is %ld but pattern has only %d sub-expression\n", fmt.prec, nsubexpr);
        pcre2_code_free(code);
        return false;
    }
    info.append(&match_data, sizeof(match_data));
#else
    const char* errormsg;
    int eoffset;
    int* ovector;

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
        pcre_free(code);
        return false;
    }
    ovector = (int*)calloc(nsubexpr*3, sizeof(int));
    if (!ovector) {
        error("Out of memory\n");
        pcre_free(code);
        return false;
    }
    info.append(&nsubexpr, sizeof(nsubexpr));
    info.append(&ovector, sizeof(ovector));
#endif
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
    int rc;
    size_t l;
    const char* info = fmt.info;
    size_t length = fmt.width > 0 ? fmt.width : strlen(input);
    int subexpr = fmt.prec > 0 ? fmt.prec : 0;

    if (length > INT_MAX)
        length = INT_MAX;
    debug("input = \"%s\"\n", input);
    debug("length=%" Z "u\n", length);

#if PCRE == 2
    pcre2_match_data* match_data = extract<pcre2_match_data*>(info);
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    pcre2_code* code = extract<pcre2_code*>(info);
    rc = pcre2_match(code, (PCRE2_SPTR8)input, length, 0, 0, match_data, NULL);
#else
    int nsubexpr = extract<int>(info);
    int* ovector = extract<int*>(info);
    pcre* code = extract<pcre*>(info);
    rc = pcre_exec(code, NULL, input, (int)length, 0, 0, ovector, nsubexpr*3);
#endif
    debug("pcre match \"%.*s\" result = %d\n", (int)length, input, rc);
    if (rc <= subexpr)
    {
        // error or no match or not enough sub-expressions
        return -1;
    }
    if (fmt.flags & skip_flag) return GROUP_END(ovector, subexpr);

    l = GROUP_LEN(ovector, subexpr);
    if (l >= size) {
        if (!(fmt.flags & sign_flag)) {
            error("Regexp: Matching string \"%s\" too long (%" Z "u>%" Z "u bytes). You may want to try the + flag: \"%%+/.../\"\n",
                StreamBuffer(input + GROUP_START(ovector, subexpr),l).expand()(),
                l, size-1);
            return -1;
        }
        l = size-1;
    }
    memcpy(value, input + GROUP_START(ovector, subexpr), l);
    value[l] = '\0';
    size = l+1; // update number of bytes written to value
    return ovector[1]; // consume input until end of match
}

static void regsubst(const StreamFormat& fmt, StreamBuffer& buffer, size_t start)
{
    size_t length, c, l, r, rl;
    int rc, n;
    StreamBuffer s;

    length = buffer.length() - start;
    if (fmt.width && fmt.width < length)
        length = fmt.width;
    if (length > INT_MAX)
        length = INT_MAX;
    if (fmt.flags & left_flag)
        start = buffer.length() - length;

    const char* info = fmt.info;
#if PCRE == 2
    pcre2_match_data* match_data = extract<pcre2_match_data*>(info);
    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    pcre2_code* code = extract<pcre2_code*>(info);
    uint32_t options;
    pcre2_pattern_info(code, PCRE2_INFO_ALLOPTIONS, &options);
#else
    int nsubexpr = extract<int>(info);
    int* ovector = extract<int*>(info);
    pcre* code = extract<pcre*>(info);
    int options;
    pcre_fullinfo(code, NULL, PCRE_INFO_OPTIONS, &options);
#endif
    const char* subst = info;

    debug("regsubst buffer=\"%s\", start=%" Z "u, length=%" Z "u, subst = \"%s\"\n",
        buffer.expand()(), start, length, StreamBuffer(subst).expand()());

    for (c = 0, n = 1; c < length; n++)
    {
#if PCRE == 2
        rc = pcre2_match(code, (PCRE2_SPTR8)buffer(start+c), length, 0, 0, match_data, NULL);
#else
        rc = pcre_exec(code, NULL, buffer(start+c), (int)(length-c), 0, 0, ovector, nsubexpr*3);
#endif
        debug("pcre: match \"%s\" result = %d\n", buffer.expand(start+c, length-c)(), rc);

        if (rc < 0) // no match
        {
            debug("pcre: no match\n");
            break;
        }
        l = GROUP_LEN(ovector, 0);

        // no prec: replace all matches
        // prec with + flag: replace first prec matches
        // prec without + flag: replace only match number prec

        if ((fmt.flags & sign_flag) || n >= fmt.prec)
        {
            // replace subexpressions
            debug("before [%" Z "d]= \"%s\"\n", (size_t)ovector[0], buffer.expand(start+c, GROUP_START(ovector, 0))());
            debug("match  [%" Z "d]= \"%s\"\n", l, buffer.expand(start+c+GROUP_START(ovector, 0),l)());
            for (r = 1; r < (size_t)rc; r++)
                debug("sub%" Z "d = \"%s\"\n", r, buffer.expand(start+c+GROUP_START(ovector, r), GROUP_LEN(ovector, r))());
            debug("after     = \"%s\"\n", buffer.expand(start+c+GROUP_END(ovector, 0))());
            s = subst;
            debug("subs      = \"%s\"\n", s.expand()());
            for (r = 0; r < s.length(); r++)
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
                        rl = GROUP_LEN(ovector, br);
                        s.replace(r, 3, buffer(start+c+ovector[br]), rl);
                        switch (ch)
                        {
                            case 'u':
                                if (islower((unsigned char)s[r])) s[r] = toupper((unsigned char)s[r]);
                                break;
                            case 'l':
                                if (isupper((unsigned char)s[r])) s[r] = tolower((unsigned char)s[r]);
                                break;
                            case 'U':
                                for (size_t i = 0; i < rl; i++)
                                    if (islower((unsigned char)s[r+i])) s[r+i] = toupper((unsigned char)s[r+i]);
                                break;
                            case 'L':
                                for (size_t i = 0; i < rl; i++)
                                    if (isupper((unsigned char)s[r+i])) s[r+i] = tolower((unsigned char)s[r+i]);
                                break;
                        }
                    }
                    else if (ch != 0 && ch < rc) // escaped 1 - 9 : replace with subexpr
                    {
                        debug("found escaped \\%u\n", ch);
                        rl = GROUP_LEN(ovector, ch);
                        debug("yes, replace \\%d: \"%s\"\n", ch, buffer.expand(start+c+ovector[ch], rl)());
                        s.replace(r, 2, buffer(start+c+GROUP_START(ovector, ch)), rl);
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
