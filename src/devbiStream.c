/***************************************************************
* Stream Device record interface for binary input records      *
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
#include <biRecord.h>
#include <string.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    biRecord *bi = (biRecord *) record;
    unsigned long val;

    switch (format->type)
    {
        case DBF_LONG:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            if (bi->mask) val &= bi->mask;
            bi->rval = val;
            return OK;
        }
        case DBF_ENUM:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            bi->val = (val != 0);
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(bi->znam)];
            if (streamScanfN (record, format, buffer, sizeof(buffer)))
                return ERROR;
            if (strcmp (bi->znam, buffer) == 0)
            {
                bi->val = 0;
                return DO_NOT_CONVERT;
            }
            if (strcmp (bi->onam, buffer) == 0)
            {
                bi->val = 1;
                return DO_NOT_CONVERT;
            }
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    biRecord *bi = (biRecord *) record;

    switch (format->type)
    {
        case DBF_LONG:
        {
            return streamPrintf (record, format, bi->rval);
        }
        case DBF_ENUM:
        {
            return streamPrintf (record, format, (long) bi->val);
        }
        case DBF_STRING:
        {
            return streamPrintf (record, format,
                bi->val ? bi->onam : bi->znam);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    biRecord *bi = (biRecord *) record;

    return streamInitRecord (record, &bi->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devbiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devbiStream);
