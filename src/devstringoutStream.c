/***************************************************************
* Stream Device record interface for string output records     *
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

#include "stringoutRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    stringoutRecord *so = (stringoutRecord *)record;
    unsigned short monitor_mask;

    if (format->type != DBF_STRING) return ERROR;
    if (streamScanfN(record, format, so->val, sizeof(so->val)) == ERROR) return ERROR;
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
#ifndef EPICS_3_13
    if (so->mpst == stringoutPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (so->apst == stringoutPOST_Always)
        monitor_mask |= DBE_LOG;
#endif
    if (monitor_mask != (DBE_VALUE|DBE_LOG) &&
        strncmp(so->oval, so->val, sizeof(so->val)))
    {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(so->oval, so->val, sizeof(so->val));
    }
    if (monitor_mask)
        db_post_events(record, so->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    stringoutRecord *so = (stringoutRecord *)record;

    if (format->type == DBF_STRING)
    {
        return streamPrintf(record, format, so->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    stringoutRecord *so = (stringoutRecord *)record;

    return streamInitRecord(record, &so->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devstringoutStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devstringoutStream);
