/*************************************************************************
* This is the StreamDevice interface for EPICS ao records.
* Please see ../docs/ for detailed documentation.
*
* (C) 1999,2005 Dirk Zimoch (dirk.zimoch@psi.ch)
*
* This file is part of StreamDevice.
*
* StreamDevice is free software: You can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* StreamDevice is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with StreamDevice. If not, see https://www.gnu.org/licenses/.
*************************************************************************/

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
    ao->orbv = (epicsInt32)(ao->oval = val);
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
