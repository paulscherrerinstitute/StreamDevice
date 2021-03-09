/*************************************************************************
* This is the StreamDevice interface for EPICS bi records.
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

#include "biRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    biRecord *bi = (biRecord *)record;
    unsigned long val;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (bi->mask) val &= bi->mask;
            bi->rval = val;
            return OK;
        }
        case DBF_ENUM:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            bi->val = (val != 0);
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(bi->znam)];
            if (streamScanfN(record, format, buffer, sizeof(buffer)) == ERROR)
                return ERROR;
            if (strcmp (bi->znam, buffer) == 0)
            {
                bi->val = 0;
                return DO_NOT_CONVERT;
            }
            if (strcmp (bi->onam, buffer) == 0)
            {
                bi->val = 1;
                return DO_NOT_CONVERT;
            }
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    biRecord *bi = (biRecord *)record;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            return streamPrintf(record, format, bi->rval);
        }
        case DBF_ENUM:
        {
            return streamPrintf(record, format, (long)bi->val);
        }
        case DBF_STRING:
        {
            return streamPrintf(record, format,
                bi->val ? bi->onam : bi->znam);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    biRecord *bi = (biRecord *)record;

    return streamInitRecord(record, &bi->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devbiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devbiStream);
