/***************************************************************
* Stream Device record interface for calcout records           *
*                                                              *
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

#include "calcoutRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    calcoutRecord *co = (calcoutRecord *)record;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            if (streamScanf(record, format, &co->val) == ERROR) return ERROR;
            break;
        }
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long lval;

            if (streamScanf(record, format, &lval) == ERROR) return ERROR;
            if (format->type == DBF_LONG)
                co->val = lval;
            else
                co->val = (unsigned long)lval;
            break;
        }
        default:
            return ERROR;        
    }
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);

    if (!(fabs(co->mlst - co->val) <= co->mdel)) {
        monitor_mask |= DBE_VALUE;
        co->mlst = co->val;
    }
    if (!(fabs(co->mlst - co->val) <= co->adel)) {
        monitor_mask |= DBE_LOG;
        co->alst = co->val;
    }
    if (monitor_mask){
        db_post_events(record, &co->val, monitor_mask);
    }
    
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    calcoutRecord *co = (calcoutRecord *)record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamPrintf(record, format, co->oval);
        }
        case DBF_ULONG:
        {
            return streamPrintf(record, format, (unsigned long)co->oval);
        }
        case DBF_LONG:
        case DBF_ENUM:
        {
            return streamPrintf(record, format, (long)co->oval);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    calcoutRecord *co = (calcoutRecord *)record;

    return streamInitRecord(record, &co->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} devcalcoutStream = {
    6,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite,
    NULL
};

epicsExportAddress(dset,devcalcoutStream);
