/*************************************************************************
* This is the StreamDevice interface for EPICS int64out records.
* Please see ../docs/ for detailed documentation.
*
* (C) 2018 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#include "int64outRecord.h"
#include "devStream.h"

/* DELTA calculates the absolute difference between its arguments */
#define DELTA(last, val) ((last) > (val) ? (last) - (val) : (val) - (last))

static long readData(dbCommon *record, format_t *format)
{
    int64outRecord *i64o = (int64outRecord *)record;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (format->type == DBF_LONG)
                i64o->val = val;
            else
                i64o->val = (unsigned long)val;
            break;
        }
        default:
            return ERROR;
    }
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (DELTA(i64o->mlst, i64o->val) > i64o->mdel)
    {
        monitor_mask |= DBE_VALUE;
        i64o->mlst = i64o->val;
    }
    if (DELTA(i64o->alst, i64o->val) > i64o->adel)
    {
        monitor_mask |= DBE_LOG;
        i64o->alst = i64o->val;
    }
    if (monitor_mask)
        db_post_events(record, &i64o->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    int64outRecord *i64o = (int64outRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_ENUM:
        case DBF_LONG:
            return streamPrintf(record, format, (long)i64o->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    int64outRecord *i64o = (int64outRecord *)record;

    return streamInitRecord(record, &i64o->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devint64outStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devint64outStream);
