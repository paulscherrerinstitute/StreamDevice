/***************************************************************
* Stream Device record interface for int64in records           *
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

#include "int64inRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    int64inRecord *i64i = (int64inRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (format->type == DBF_LONG)
                i64i->val = val;
            else
                i64i->val = (unsigned long)val;
            return OK;
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    int64inRecord *i64i = (int64inRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_ENUM:
        case DBF_LONG:
            return streamPrintf(record, format, (long)i64i->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    int64inRecord *i64i = (int64inRecord *)record;

    return streamInitRecord(record, &i64i->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devint64inStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devint64inStream);
