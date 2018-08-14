/***************************************************************
* Stream Device record interface for int64out records          *
*                                                              *
* (C) 2018 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
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

#include "int64outRecord.h"
#include "devStream.h"

/* DELTA calculates the absolute difference between its arguments */
#define DELTA(last, val) ((last) > (val) ? (last) - (val) : (val) - (last))

static long readData(dbCommon *record, format_t *format)
{
    int64outRecord *i64o = (int64outRecord *)record;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (format->type == DBF_LONG)
                i64o->val = val;
            else
                i64o->val = (unsigned long)val;
            break;
        }
        default:
            return ERROR;
    }
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (DELTA(i64o->mlst, i64o->val) > i64o->mdel)
    {
        monitor_mask |= DBE_VALUE;
        i64o->mlst = i64o->val;
    }
    if (DELTA(i64o->alst, i64o->val) > i64o->adel)
    {
        monitor_mask |= DBE_LOG;
        i64o->alst = i64o->val;
    }
    if (monitor_mask)
        db_post_events(record, &i64o->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    int64outRecord *i64o = (int64outRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_ENUM:
        case DBF_LONG:
            return streamPrintf(record, format, (long)i64o->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    int64outRecord *i64o = (int64outRecord *)record;

    return streamInitRecord(record, &i64o->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devint64outStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devint64outStream);
