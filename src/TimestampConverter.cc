/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2010 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is the time stamp converter of StreamDevice.            *
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

#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#define tzset() _tzset()
#define timezone _timezone
#define localtime_r(timet,tm) localtime_s(tm,timet) /* Windows sucks */
#endif

#ifdef __rtems__
#define timezone _timezone
#endif

#ifdef vxWorks
int timezone = 0;
#define tzset() do {\
    (void) (sscanf(getenv("TIMEZONE"), "%*[^:]::%d", &timezone) || \
            sscanf(getenv("TIMEZONE"), "%*[^:]:%*[^:]:%d", &timezone) || \
            sscanf(getenv("EPICS_TS_MIN_WEST"), "%d", &timezone));\
    timezone*=60;\
} while (0)
#endif

class TimestampConverter : public StreamFormatConverter
{
    int parse(const StreamFormat&, StreamBuffer&, const char*&, bool);
    bool printDouble(const StreamFormat&, StreamBuffer&, double);
    int scanDouble(const StreamFormat&, const char*, double&);
};

int TimestampConverter::
parse(const StreamFormat&, StreamBuffer& info,
    const char*& source, bool)
{
    unsigned int n;
    char* c;

    if (*source == '(')
    {
        while (*++source != ')')
        {
            switch (*source)
            {
                case 0:
                    error ("missing ')' after %%T format\n");
                    return false;
                case esc:
                    info.append(*++source);
                    if (*source == '%') info.append('%');
                    break;
                case '%':
                    source++;
                    /* look for formatted fractions like %3f */
                    if (isdigit(*source))
                    {
                        n = strtoul(source, &c, 10);
                        if (*c == 'f')
                        {
                            source = c;
                            info.print("%%0%uf", n);
                            break;
                        }
                    }
                    /* look for nanoseconds %N of %f */
                    if (*source == 'N' || *source == 'f')
                    {
                        info.print("%%09f");
                        break;
                    }
                    /* look for seconds with fractions like %.3S */
                    if (*source == '.')
                    {
                        c = (char*) source+1;
                        n = isdigit(*c) ? strtoul(c, &c, 10) : 9;
                        if (toupper(*c) == 'S')
                        {
                            source = c;
                            info.print("%%%c.%%0%uf", *c, n);
                            break;
                        }
                    }
                    /* else normal format */
                    info.append('%');
                default:
                    info.append(*source);
            }
        }
        source++;
        info.append('\0');
    }
    else
    {
        info.append("%Y-%m-%d %H:%M:%S").append('\0');
    }
    return double_format;
}

bool TimestampConverter::
printDouble(const StreamFormat& format, StreamBuffer& output, double value)
{
    struct tm brokenDownTime;
    char buffer [40];
    char fracbuffer [15];
    int length;
    time_t sec;
    double frac;
    int i, n;
    char* c;
    char* p;

    sec = (time_t) value;
    frac = value - sec;
    localtime_r(&sec, &brokenDownTime);
    debug ("TimestampConverter::printDouble %f, '%s'\n", value, format.info);
    length = strftime(buffer, sizeof(buffer), format.info, &brokenDownTime);
    i = output.length();
    output.append(buffer, length);

    /* look for fractional seconds */
    while ((i = output.find("%0",i)) != -1)
    {
        n = strtol(output(i+1), &c, 10);
        if (*c++ != 'f') return false;
        /* print fractional part */
        sprintf(fracbuffer, "%.*f", n, frac);
        p = strchr(fracbuffer, '.')+1;
        output.replace(i, c-output(i), p);
    }
    return true;
}

/* many OS don't have strptime or strptime does not fully support
   all fields, e.g. %z.
 */

static int strmatch(const char*& input, const char** strings, int minlen)
{
    int i;
    int c;

    for (i=0; strings[i]; i++) {
        for (c=0; ; c++)
        {
            if (strings[i][c] == 0) {
                input += c;
                return i;
            }
            if (tolower(input[c]) != strings[i][c]) {
                if (c >= minlen) {
                    input += c;
                    return i;
                }
                break;
            }
        }
    }
    return -1;
}

static int nummatch(const char*& input, int min, int max)
{
    int i;
    char *c;

    i = strtol(input, &c, 10);
    if (c == input) return -10000;
    if (i < min || i > max) return -10000;
    input = c;
    return i;
}

static const char* scantime(const char* input, const char* format, struct tm *tm, unsigned long *ns)
{
    static const char* months[] = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "november", "december", 0 };
    static const char* ampm[] = {
        "am", "pm", 0 };

    int i, n;
    int pm = -1;
    int century = -1;
    int zone = 0;

    tzset();
    zone = timezone/60;
    debug ("TimestampConverter::scantime: native time zone = %d\n", zone);

    while (*format)
    {
        switch (*format)
        {
            case '%':
                debug ("TimestampConverter::scantime: input = '%s'\n", input);
                format++;
startover:
                switch (*format++)
                {
                /* Modifiers (ignore) */
                    case 'E':
                    case 'O':
                        goto startover;
                /* constants */
                    case 0: /* stray % at end of format string */
                        format--;
                    case '%':
                        if (*input++ != '%') return NULL;
                        break;
                    case 'n':
                        if (*input++ != '\n') return NULL;
                        break;
                    case 't':
                        if (*input++ != '\t') return NULL;
                        break;
                /* ignored */
                    case 'A': /* day of week name */
                    case 'a':
                        while (isalpha((int)*input)) input++;
                        /* ignore */
                        break;
                    case 'u': /* day of week number (Monday = 1 to Sunday = 7) */
                    case 'w': /* day of week number (Sunday = 0 to Saturday = 6) */
                        i = nummatch(input, 0, 7);
                        if (i < 0)
                        {
                            error ("error parsing day of week: '%.20s'\n", input);
                            return NULL;
                        }
                        debug ("TimestampConverter::scantime: day of week = %d\n", i);
                        /* ignore */
                        break;
                    case 'U': /* week number */
                    case 'W':
                    case 'V':
                        i = nummatch(input, 0, 53);
                        if (i < 0)
                        {
                            error ("error parsing week number: '%.20s'\n", input);
                            return NULL;
                        }
                        debug ("TimestampConverter::scantime: week number = %d\n", i);
                        /* ignore */
                        break;
                    case 'j': /* day of year */
                        i = nummatch(input, 0, 366);
                        if (i < 0)
                        {
                            error ("error parsing day of year: '%.20s'\n", input);
                            return NULL;
                        }
                        debug ("TimestampConverter::scantime: day of year = %d\n", i);
                        /* ignore */
                        break;
                    case 'Z': /* time zone name */
                        while (isalpha((int)*input)) input++;
                        /* ignore */
                        break;
                /* date */
                    case 'b': /* month */
                    case 'h':
                    case 'B':
                    case 'm':
                        i = strmatch(input, months, 3);
                        if (i < 0)
                        {
                            i = nummatch(input, 1, 12);
                            if (i < 0)
                            {
                                error ("error parsing month: '%.20s'\n", input);
                                return NULL;
                            }
                            i--;  /* Jan = 0 */
                        }
                        tm->tm_mon = i;
                        debug ("TimestampConverter::scantime: month = %d (%s)\n", tm->tm_mon+1, months[tm->tm_mon]);
                        break;
                    case 'd': /* day of month */
                    case 'e':
                        i = nummatch(input, 1, 31);
                        if (i < 0)
                        {
                            error ("error parsing day of month: '%.20s'\n", input);
                            return NULL;
                        }
                        tm->tm_mday = i;
                        debug ("TimestampConverter::scantime: day = %d\n", tm->tm_mday);
                        break;
                    case 'Y': /* year */
                    case 'y':
                        i = strtol(input, (char**)&input, 10);
                        if (i < 100)
                        { /* 2 digit year */
                            if (century == -1) century = (i >= 69);
                            tm->tm_year = i + century * 100; /* 0 = 1900 */
                        }
                        else
                        { /* 4 digit year */
                            tm->tm_year = i - 1900; /* 0 = 1900 */
                        }
                        debug ("TimestampConverter::scantime: year = %d\n", tm->tm_year + 1900);
                        break;
                    case 'C': /* century */
                        i = nummatch(input, 0, 99);
                        if (i < 0)
                        {
                            error ("error parsing century: '%.20s'\n", input);
                            return NULL;
                        }
                        century = i - 19;
                        tm->tm_year = tm->tm_year%100 + 100 * i; /* 0 = 1900 */
                        debug ("TimestampConverter::scantime: year = %d\n", tm->tm_year + 1900);
                        break;

                /* time */
                    case 'H': /* hour */
                    case 'k':
                    case 'I':
                    case 'l':
                        i = nummatch(input, 0, 23);
                        if (i < 0)
                        {
                            error ("error parsing hour: '%.20s'\n", input);
                            return NULL;
                        }
                        if ((pm == 0) && (i == 12)) i = 0;
                        if ((pm == 1) && (i < 12)) i += 12;
                        tm->tm_hour = i;
                        debug ("TimestampConverter::scantime: hour = %d\n", tm->tm_hour);
                        break;
                    case 'P': /* AM / PM */
                    case 'p':
                        i = strmatch(input, ampm, 1);
                        if (i < 0)
                        {
                            error ("error parsing am/pm: '%.20s'\n", input);
                            return NULL;
                        }
                        pm = i;
                        if ((pm == 0) && (tm->tm_hour == 12)) tm->tm_hour = 0;
                        if ((pm == 1) && (tm->tm_hour < 12)) tm->tm_hour += 12;
                        debug ("TimestampConverter::scantime: %s hour = %d\n", pm?"PM":"AM", tm->tm_hour);
                        break;
                    case 'M': /* minute */
                        i = nummatch(input, 1, 59);
                        if (i < 0)
                        {
                            error ("error parsing minute: '%.20s'\n", input);
                            return NULL;
                        }
                        tm->tm_min = i;
                        debug ("TimestampConverter::scantime: min = %d\n", tm->tm_min);
                        break;
                    case 'S': /* second */
                        i = nummatch(input, 1, 60);
                        if (i < 0)
                        {
                            error ("error parsing week second: '%.20s'\n", input);
                            return NULL;
                        }
                        tm->tm_sec = i;
                        debug ("TimestampConverter::scantime: sec = %d\n", tm->tm_sec);
                        break;
                    case 's': /* second since 1970 */
                        i = strtol(input, (char**)&input, 10);
                        tm->tm_sec = i;
                        tm->tm_mon = -1;
                        tm->tm_isdst = 0;
                        debug ("TimestampConverter::scantime: sec = %d\n", tm->tm_sec);
                        break;
                    case '0': /* fractions of seconds like %09f */
                        n = strtol(format-1, (char**)&format, 10);
                        if (*format++ != 'f') return NULL;
                        debug ("max %d digits fraction in '%s'\n", n, input);
                        i = 0;
                        while (n-- && isdigit(*input))
                        {
                            i *= 10;
                            i += *input++ - '0';
                        }
                        while (i < 100000000) i *= 10;
                        *ns = i;
                        debug ("TimestampConverter::scantime: nanosec = %d, rest '%s'\n", i, input);
                        break;
                    case 'z': /* time zone offset */
                        i = nummatch(input, -2400, 2400);
                        if (i < -2400)
                        {
                            error ("error parsing time zone: '%.20s'\n", input);
                            return NULL;
                        }
                        zone = i / 100 * 60 + i % 100;
                        tm->tm_isdst = 0;
                        debug ("TimestampConverter::scantime: zone = %d\n", zone);
                        break;
                    case '+': /* set time zone in format string */
                    case '-':
                        format--;
                        i = nummatch(format, -2400, 2400);
                        zone = i / 100 * 60 + i % 100;
                        tm->tm_isdst = 0;
                        debug ("TimestampConverter::scantime: zone = %d\n", zone);
                        break;
                /* shortcuts */
                    case 'c':
                        if ((input = scantime(input, "%a %b %d %H:%M:%S %Y", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'D':
                        if ((input = scantime(input, "%m/%d/%y", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'F':
                        if ((input = scantime(input, "%Y-%m-%d", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'R':
                        if ((input = scantime(input, "%H:%M", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'T':
                        if ((input = scantime(input, "%H:%M:%S", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'x':
                        if ((input = scantime(input, "%m/%d/%y", tm, ns)) == NULL)
                            return NULL;
                        break;
                    case 'X':
                    case 'r':
                        if ((input = scantime(input, "%I:%M:%S %p", tm, ns)) == NULL)
                            return NULL;
                        break;
                    default:
                        error ("unknown time format %%%s\n", --format);
                        return NULL;
                }
                break;
            case ' ':
                format++;
                while (isspace(*input)) input++;
                break;
            default:
                if (*format++ != *input++)
                {
                    error("input '%.20s' does not match constant '%.20s'\n", --input, --format);
                    return NULL;
                }
        }
    }
    zone -= timezone/60;
    tm->tm_min += zone;
    tm->tm_hour += tm->tm_min / 60;
    tm->tm_min %= 60;
    if (tm->tm_min < 0)
    {
        tm->tm_min += 60;
        tm->tm_hour -= 1;
    }
    tm->tm_mday -= tm->tm_hour / 24;
    tm->tm_hour %= 24;
    if (tm->tm_hour < 0)
    {
        tm->tm_min += 24;
        tm->tm_mday -= 1;
    }
    return input;
}



int TimestampConverter::
scanDouble(const StreamFormat& format, const char* input, double& value)
{
    struct tm brokenDownTime;
    time_t seconds;
    unsigned long nanoseconds;
    const char* end;

    /* Init time stamp with "today" */
    time (&seconds);
    localtime_r(&seconds, &brokenDownTime);
    brokenDownTime.tm_sec = 0;
    brokenDownTime.tm_min = 0;
    brokenDownTime.tm_hour = 0;
    brokenDownTime.tm_yday = 0;
    brokenDownTime.tm_isdst = -1;
    nanoseconds = 0;

    end = scantime(input, format.info, &brokenDownTime, &nanoseconds);
    if (end == NULL) {
        error ("error parsing time\n");
        return -1;
    }
    if (brokenDownTime.tm_mon == -1) {
        seconds = brokenDownTime.tm_sec;
    } else {
        seconds = mktime(&brokenDownTime);
        if (seconds == (time_t) -1 && brokenDownTime.tm_yday == 0)
        {
            error ("mktime failed for %02d/%02d/%04d %02d:%02d:%02d\n",
                brokenDownTime.tm_mon+1,
                brokenDownTime.tm_mday,
                brokenDownTime.tm_year+1900,
                brokenDownTime.tm_hour,
                brokenDownTime.tm_min,
                brokenDownTime.tm_sec);
            return -1;
        }
    }
    value = seconds + nanoseconds*1e-9;
    return end-input;
}

RegisterConverter (TimestampConverter, "T");
