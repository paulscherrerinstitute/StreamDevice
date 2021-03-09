/*************************************************************************
* This is the StreamDevice interface for EPICS bo records.
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

#include "boRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    boRecord *bo = (boRecord *)record;
    unsigned long val;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (bo->mask) val &= bo->mask;
            bo->rbv = val;
            bo->rval = val;
            break;
        }
        case DBF_ENUM:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            break;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(bo->znam)];
            if (streamScanfN(record, format, buffer, sizeof(buffer)) == ERROR)
                return ERROR;
            if (strcmp (bo->znam, buffer) == 0)
            {
                val = 0;
                break;
            }
            if (strcmp (bo->onam, buffer) == 0)
            {
                val = 1;
                break;
            }
        }
        default:
            return ERROR;
    }
    bo->val = (val != 0);
    if (bo->pact) return DO_NOT_CONVERT;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (bo->mlst != bo->val)
    {
        monitor_mask |= (DBE_VALUE | DBE_LOG);
        bo->mlst = bo->val;
    }
    if (monitor_mask)
        db_post_events(record, &bo->val, monitor_mask);
    if (bo->oraw != bo->rval)
    {
        db_post_events(record,&bo->rval, monitor_mask | DBE_VALUE | DBE_LOG);
        bo->oraw = bo->rval;
    }
    if (bo->orbv != bo->rbv)
    {
        db_post_events(record, &bo->rbv, monitor_mask | DBE_VALUE | DBE_LOG);
        bo->orbv = bo->rbv;
    }
    return DO_NOT_CONVERT;
}

static long writeData(dbCommon *record, format_t *format)
{
    boRecord *bo = (boRecord *)record;
    long val;

    switch (format->type)
    {
        case DBF_ULONG:
            val = bo->rval;
            break;
        case DBF_LONG:
            val = bo->mask ? (epicsInt32)bo->rval : (epicsInt16)bo->val;
            break;
        case DBF_ENUM:
            val = bo->val;
            break;
        case DBF_STRING:
            return streamPrintf(record, format,
                bo->val ? bo->onam : bo->znam);
        default:
            return ERROR;
    }
    return streamPrintf(record, format, val);
}

static long initRecord(dbCommon *record)
{
    boRecord *bo = (boRecord *)record;

    return streamInitRecord(record, &bo->out, readData, writeData);
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
