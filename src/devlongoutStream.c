/***************************************************************
* Stream Device record interface for long output records       *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
* Please refer to the HTML files in ../docs/ for a detailed    *
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

#include "longoutRecord.h"
#include "devStream.h"

/* DELTA calculates the absolute difference between its arguments */
#define DELTA(last, val) ((last) > (val) ? (last) - (val) : (val) - (last))

static long readData(dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *)record;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            lo->val = val;
            break;
        }
        default:
            return ERROR;
    }
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (DELTA(lo->mlst, lo->val) > lo->mdel)
    {
        monitor_mask |= DBE_VALUE;
        lo->mlst = lo->val;
    }
    if (DELTA(lo->alst, lo->val) > lo->adel)
    {
        monitor_mask |= DBE_LOG;
        lo->alst = lo->val;
    }
    if (monitor_mask)
        db_post_events(record, &lo->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
            return streamPrintf(record, format, lo->val);
        case DBF_LONG:
        case DBF_ENUM:
            return streamPrintf(record, format, (long)lo->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    longoutRecord *lo = (longoutRecord *)record;

    return streamInitRecord(record, &lo->out, readData, writeData) == ERROR ?
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
