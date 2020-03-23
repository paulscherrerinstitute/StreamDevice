/*************************************************************************
* This is the StreamDevice interface for EPICS stringin records.
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

#include "stringinRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    stringinRecord *si = (stringinRecord *)record;

    if (format->type == DBF_STRING)
    {
        if (streamScanfN(record, format, si->val, sizeof(si->val)) == ERROR) return ERROR;
        return OK;
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    stringinRecord *si = (stringinRecord *)record;

    if (format->type == DBF_STRING)
    {
        return streamPrintf(record, format, si->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    stringinRecord *si = (stringinRecord *)record;

    return streamInitRecord(record, &si->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devstringinStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devstringinStream);
