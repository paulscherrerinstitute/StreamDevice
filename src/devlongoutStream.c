/*************************************************************************
* This is the StreamDevice interface for EPICS longout records.
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

#include "longoutRecord.h"
#include "devStream.h"

/* DELTA calculates the absolute difference between its arguments */
#define DELTA(last, val) ((last) > (val) ? (last) - (val) : (val) - (last))

static long readData(dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *)record;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            lo->val = val;
            break;
        }
        default:
            return ERROR;
    }
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (DELTA(lo->mlst, lo->val) > lo->mdel)
    {
        monitor_mask |= DBE_VALUE;
        lo->mlst = lo->val;
    }
    if (DELTA(lo->alst, lo->val) > lo->adel)
    {
        monitor_mask |= DBE_LOG;
        lo->alst = lo->val;
    }
    if (monitor_mask)
        db_post_events(record, &lo->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    longoutRecord *lo = (longoutRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
            return streamPrintf(record, format, lo->val);
        case DBF_LONG:
        case DBF_ENUM:
            return streamPrintf(record, format, (long)lo->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    longoutRecord *lo = (longoutRecord *)record;

    return streamInitRecord(record, &lo->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devlongoutStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devlongoutStream);
