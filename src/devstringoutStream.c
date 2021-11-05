/*************************************************************************
* This is the StreamDevice interface for EPICS stringout records.
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

#include "stringoutRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    stringoutRecord *so = (stringoutRecord *)record;
    unsigned short monitor_mask;

    if (format->type != DBF_STRING) return ERROR;
    if (streamScanfN(record, format, so->val, sizeof(so->val)) == ERROR) return ERROR;
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
#ifndef EPICS_3_13
    if (so->mpst == stringoutPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (so->apst == stringoutPOST_Always)
        monitor_mask |= DBE_LOG;
#endif
    if (monitor_mask != (DBE_VALUE|DBE_LOG) &&
        strncmp(so->oval, so->val, sizeof(so->val)))
    {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        strncpy(so->oval, so->val, sizeof(so->oval));
    }
    if (monitor_mask)
        db_post_events(record, so->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    stringoutRecord *so = (stringoutRecord *)record;

    if (format->type == DBF_STRING)
    {
        return streamPrintf(record, format, so->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    stringoutRecord *so = (stringoutRecord *)record;

    return streamInitRecord(record, &so->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devstringoutStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devstringoutStream);
