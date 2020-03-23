/*************************************************************************
* This is the StreamDevice interface for EPICS int64in records.
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

#include "int64inRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    int64inRecord *i64i = (int64inRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            long val;
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (format->type == DBF_LONG)
                i64i->val = val;
            else
                i64i->val = (unsigned long)val;
            return OK;
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    int64inRecord *i64i = (int64inRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_ENUM:
        case DBF_LONG:
            return streamPrintf(record, format, (long)i64i->val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    int64inRecord *i64i = (int64inRecord *)record;

    return streamInitRecord(record, &i64i->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devint64inStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devint64inStream);
