/***************************************************************
* Stream Device record interface for long input records        *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
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

#include "devStream.h"
#include <longinRecord.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    longinRecord *li = (longinRecord *) record;

    if (format->type == DBF_LONG || format->type == DBF_ENUM)
    {
        long val;
        if (streamScanf (record, format, &val)) return ERROR;
        li->val = val;
        return OK;
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    longinRecord *li = (longinRecord *) record;

    if (format->type == DBF_LONG || format->type == DBF_ENUM)
    {
        return streamPrintf (record, format, (long) li->val);
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    longinRecord *li = (longinRecord *) record;

    return streamInitRecord (record, &li->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devlonginStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devlonginStream);
