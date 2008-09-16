/***************************************************************
* Stream Device record interface for analog output records     *
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
#include <aoRecord.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    aoRecord *ao = (aoRecord *) record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            double val;
            if (streamScanf (record, format, &val)) return ERROR;
            if (ao->aslo != 0) val *= ao->aslo;
            ao->val = val + ao->aoff;
            return DO_NOT_CONVERT;
        }
        case DBF_LONG:
        {
            long rval;
            if (streamScanf (record, format, &rval)) return ERROR;
            ao->rbv = rval;
            if (INIT_RUN) ao->rval = rval;
            return OK;
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    aoRecord *ao = (aoRecord *) record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            double val;
            if (INIT_RUN) val = ao->val;
            else val = ao->oval;
            val -= ao->aoff;
            if (ao->aslo != 0) val /= ao->aslo;
            return streamPrintf (record, format, val);
        }
        case DBF_LONG:
        {
            return streamPrintf (record, format, (long) ao->rval);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    aoRecord *ao = (aoRecord *) record;

    return streamInitRecord (record, &ao->out, readData, writeData);
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} devaoStream = {
    6,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite,
    NULL
};

epicsExportAddress(dset,devaoStream);
