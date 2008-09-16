/***************************************************************
* Stream Device record interface for long output records       *
*                                                              *
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
#include <longoutRecord.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *) record;

    if (format->type == DBF_LONG || format->type == DBF_ENUM)
    {
        long val;
        if (streamScanf (record, format, &val)) return ERROR;
        lo->val = val;
        return OK;
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *) record;

    if (format->type == DBF_LONG || format->type == DBF_ENUM)
    {
        return streamPrintf (record, format, (long) lo->val);
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    longoutRecord *lo = (longoutRecord *) record;

    return streamInitRecord (record, &lo->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devlongoutStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devlongoutStream);
