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

#include <menuConvert.h>
#include <aoRecord.h>
#include "devStream.h"
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
            if (ao->aslo != 0.0 && ao->aslo != 1.0) val *= ao->aslo;
            ao->val = val + ao->aoff;
            return DO_NOT_CONVERT;
        }
        case DBF_ULONG:
        case DBF_LONG:
        {
            long rval;
            if (streamScanf (record, format, &rval)) return ERROR;
            ao->rbv = rval;
            ao->rval = rval;
            if (ao->linr == menuConvertNO_CONVERSION)
            {
                /* allow more bits than 32 */
                if (format->type == DBF_ULONG)
                    ao->val = (unsigned long)rval;
                else    
                    ao->val = rval;
                return DO_NOT_CONVERT;
            }
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
            double val = (INIT_RUN ? ao->val : ao->oval) - ao->aoff;
            if (ao->aslo != 0.0 && ao->aslo != 1.0) val /= ao->aslo;
            return streamPrintf (record, format, val);
        }
        case DBF_ULONG:
        {
            if (ao->linr == menuConvertNO_CONVERSION)
            {
                /* allow more bits than 32 */
                return streamPrintf (record, format, (unsigned long)(INIT_RUN ? ao->val : ao->oval));
            }
            return streamPrintf (record, format, (unsigned long)ao->rval);
        }
        case DBF_LONG:
        {
            if (ao->linr == menuConvertNO_CONVERSION)
            {
                /* allow more bits than 32 */
                return streamPrintf (record, format, (long)(INIT_RUN ? ao->val : ao->oval));
            }
            return streamPrintf (record, format, (long)ao->rval);
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
