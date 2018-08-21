/***************************************************************
* Stream Device record interface for long string in records    *
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

#include "lsiRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;
    ssize_t length;

    if (format->type != DBF_STRING) return ERROR;
    if ((length = streamScanfN(record, format, lsi->val, (long)lsi->sizv)) == ERROR)
    {
        return ERROR;
    }
    if (length < (ssize_t)lsi->sizv)
    {
        lsi->val[length] = 0;
    }
    lsi->len = length;
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;

    if (format->type != DBF_STRING) return ERROR;
    return streamPrintf(record, format, lsi->val);
}

static long initRecord(dbCommon *record)
{
    lsiRecord *lsi = (lsiRecord *)record;

    return streamInitRecord(record, &lsi->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devlsiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devlsiStream);
