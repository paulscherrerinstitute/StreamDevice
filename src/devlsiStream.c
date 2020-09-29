/*************************************************************************
* This is the StreamDevice interface for EPICS lsi records.
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

#include "lsiRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;
    ssize_t length;

    if (format->type != DBF_STRING) return ERROR;
    if ((length = streamScanfN(record, format, lsi->val, (long)lsi->sizv)) == ERROR)
    {
        return ERROR;
    }
    if (length < (ssize_t)lsi->sizv)
    {
        lsi->val[length] = 0;
    }
    lsi->len = (epicsUInt32)length;
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    lsiRecord *lsi = (lsiRecord *)record;

    if (format->type != DBF_STRING) return ERROR;
    return streamPrintf(record, format, lsi->val);
}

static long initRecord(dbCommon *record)
{
    lsiRecord *lsi = (lsiRecord *)record;

    return streamInitRecord(record, &lsi->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devlsiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devlsiStream);
