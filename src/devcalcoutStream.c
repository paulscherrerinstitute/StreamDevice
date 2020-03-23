/*************************************************************************
* This is the StreamDevice interface for EPICS calcout records.
* Please see ../docs/ for detailed documentation.
*
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)
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
    if (monitor_mask) {
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
