/***************************************************************
* Stream Device record interface for analog output records     *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
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

#include "aoRecord.h"
#include "menuConvert.h"
#include "cvtTable.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    aoRecord *ao = (aoRecord *)record;
    double val;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            break;
        }
        case DBF_ULONG:
        case DBF_LONG:
        {
            long rval;
            if (streamScanf(record, format, &rval) == ERROR) return ERROR;
            ao->rbv = rval;
            ao->rval = rval;
            if (format->type == DBF_ULONG)
                val = (unsigned long)rval;
            else
                val = rval;
            break;
            val += ao->roff;
            if (ao->linr == menuConvertNO_CONVERSION) {
	        ; /*do nothing*/
            } else if ((ao->linr == menuConvertLINEAR)
#ifndef EPICS_3_13
		    || (ao->linr == menuConvertSLOPE)
#endif
                    ) {
                val = val * ao->eslo + ao->eoff;
            } else {
                if (cvtRawToEngBpt(&val, ao->linr, 0,
		        (void *)&ao->pbrk, &ao->lbrk) == ERROR) return ERROR;
            }
        }
        default:
            return ERROR;
    }
    if (ao->aslo != 0.0) val *= ao->aslo;
    val += ao->aoff;
    ao->val = val;
    if (record->pact) return DO_NOT_CONVERT;
    /* In @init handler, no processing, enforce monitor updates. */
    ao->omod = ao->oval != val;
    ao->orbv = ao->oval = val;
    monitor_mask = recGblResetAlarms(record);
    if (!(fabs(ao->mlst - val) <= ao->mdel))
    {
        monitor_mask |= DBE_VALUE;
        ao->mlst = val;
    }
    if (!(fabs(ao->alst - val) <= ao->adel))
    {
        monitor_mask |= DBE_LOG;
        ao->alst = val;
    }
    if (monitor_mask)
        db_post_events(record, &ao->val, monitor_mask);
    if (ao->omod) monitor_mask |= (DBE_VALUE|DBE_LOG);
    if (monitor_mask)
    {
        ao->omod = FALSE;
        db_post_events (record, &ao->oval, monitor_mask);
        if (ao->oraw != ao->rval)
        {
            db_post_events(record, &ao->rval, monitor_mask | DBE_VALUE | DBE_LOG);
            ao->oraw = ao->rval;
        }
        if (ao->orbv != ao->rbv)
        {
            db_post_events(record, &ao->rbv, monitor_mask | DBE_VALUE | DBE_LOG);
            ao->orbv = ao->rbv;
        }
    }
    return DO_NOT_CONVERT;
}

static long writeData(dbCommon *record, format_t *format)
{
    aoRecord *ao = (aoRecord *)record;

    double val = (INIT_RUN ? ao->val : ao->oval) - ao->aoff;
    if (ao->aslo != 0.0 && ao->aslo != 1.0) val /= ao->aslo;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamPrintf(record, format, val);
        }
        case DBF_ULONG:
        {
            if (ao->linr == 0)
            {
                /* allow integers with more than 32 bits */
                return streamPrintf(record, format, (unsigned long)val);
            }
            return streamPrintf(record, format, (unsigned long)ao->rval);
        }
        case DBF_LONG:
        {
            if (ao->linr == 0)
            {
                /* allow integers with more than 32 bits */
                return streamPrintf(record, format, (long)val);
            }
            return streamPrintf(record, format, (long)ao->rval);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    aoRecord *ao = (aoRecord *)record;

    return streamInitRecord(record, &ao->out, readData, writeData);
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
