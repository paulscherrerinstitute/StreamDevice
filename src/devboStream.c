/***************************************************************
* Stream Device record interface for binary output records     *
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
#include <boRecord.h>
#include <string.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    boRecord *bo = (boRecord *) record;
    unsigned long val;

    switch (format->type)
    {
        case DBF_LONG:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            if (bo->mask) val &= bo->mask;
            bo->rbv = val;
            if (INIT_RUN) bo->rval = val;
            return OK;
        }
        case DBF_ENUM:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            bo->val = (val != 0);
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(bo->znam)];
            if (streamScanfN (record, format, buffer, sizeof(buffer)))
                return ERROR;
            if (strcmp (bo->znam, buffer) == 0)
            {
                bo->val = 0;
                return DO_NOT_CONVERT;
            }
            if (strcmp (bo->onam, buffer) == 0)
            {
                bo->val = 1;
                return DO_NOT_CONVERT;
            }
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    boRecord *bo = (boRecord *) record;

    switch (format->type)
    {
        case DBF_LONG:
        {
            return streamPrintf (record, format, bo->rval);
        }
        case DBF_ENUM:
        {
            return streamPrintf (record, format, (long) bo->val);
        }
        case DBF_STRING:
        {
            return streamPrintf (record, format,
                bo->val ? bo->onam : bo->znam);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    boRecord *bo = (boRecord *) record;

    return streamInitRecord (record, &bo->out, readData, writeData);
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devboStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devboStream);
