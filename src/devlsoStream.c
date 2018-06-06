/***************************************************************
* Stream Device record interface for string output records     *
*                                                              *
* (C) 2018 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
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

#include "lsoRecord.h"
#include "epicsExport.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;

    if (format->type == DBF_STRING)
    {
        long len;
        if ((len = streamScanfN(record, format, lso->val, lso->sizv) == ERROR))
        {
            lso->len = 0;
            return ERROR;
        }
        lso->len = len;
        return OK;
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;

    if (format->type == DBF_STRING)
    {
        return streamPrintf(record, format, lso->val);
    }
    return ERROR;
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
