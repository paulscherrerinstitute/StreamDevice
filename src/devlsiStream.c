/***************************************************************
* Stream Device record interface for string input records      *
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

#include "lsiRecord.h"
#include "epicsExport.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;

    if (format->type == DBF_STRING)
    {
        long len;
        if ((len = streamScanfN(record, format, lsi->val, lsi->sizv) == ERROR))
        {
            lsi->len = 0;
            return ERROR;
        }
        lsi->len = len;
        return OK;
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;

    if (format->type == DBF_STRING)
    {
        return streamPrintf(record, format, lsi->val);
    }
    return ERROR;
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
