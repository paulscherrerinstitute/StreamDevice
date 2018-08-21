/***************************************************************
* Stream Device record interface for long string out records   *
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

#include "lsoRecord.h"
#include "menuPost.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;
    ssize_t length;
    unsigned short monitor_mask;

    if (format->type != DBF_STRING) return ERROR;
    if ((length = streamScanfN(record, format, lso->val, lso->sizv)) == ERROR)
    {
        return ERROR;
    }
    if (length < (ssize_t)lso->sizv)
    {
        lso->val[length] = 0;
    }
    lso->len = length;
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (lso->len != lso->olen ||
        memcmp(lso->oval, lso->val, lso->len)) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        memcpy(lso->oval, lso->val, lso->len);
    }
    if (lso->len != lso->olen) {
        lso->olen = lso->len;
        db_post_events(record, &lso->len, DBE_VALUE | DBE_LOG);
    }
    if (lso->mpst == menuPost_Always)
        monitor_mask |= DBE_VALUE;
    if (lso->apst == menuPost_Always)
        monitor_mask |= DBE_LOG;
    if (monitor_mask)
        db_post_events(record, lso->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;

    if (format->type != DBF_STRING) return ERROR;
    return streamPrintf(record, format, lso->val);
}

static long initRecord(dbCommon *record)
{
    lsoRecord *lso = (lsoRecord *)record;

    return streamInitRecord(record, &lso->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devlsoStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devlsoStream);
