/*************************************************************************
* This is the StreamDevice interface for EPICS lso records.
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

#include "lsoRecord.h"
#include "menuPost.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;
    ssize_t length;
    unsigned short monitor_mask;

    if (format->type != DBF_STRING) return ERROR;
    if ((length = streamScanfN(record, format, lso->val, lso->sizv)) == ERROR)
    {
        return ERROR;
    }
    if (length < (ssize_t)lso->sizv)
    {
        lso->val[length] = 0;
    }
    lso->len = (epicsUInt32)length;
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (lso->len != lso->olen ||
        memcmp(lso->oval, lso->val, lso->len)) {
        monitor_mask |= DBE_VALUE | DBE_LOG;
        memcpy(lso->oval, lso->val, lso->len);
    }
    if (lso->len != lso->olen) {
        lso->olen = lso->len;
        db_post_events(record, &lso->len, DBE_VALUE | DBE_LOG);
    }
    if (lso->mpst == menuPost_Always)
        monitor_mask |= DBE_VALUE;
    if (lso->apst == menuPost_Always)
        monitor_mask |= DBE_LOG;
    if (monitor_mask)
        db_post_events(record, lso->val, monitor_mask);
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsoRecord *lso = (lsoRecord *)record;

    if (format->type != DBF_STRING) return ERROR;
    return streamPrintf(record, format, lso->val);
}

static long initRecord(dbCommon *record)
{
    lsoRecord *lso = (lsoRecord *)record;

    return streamInitRecord(record, &lso->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devlsoStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devlsoStream);
