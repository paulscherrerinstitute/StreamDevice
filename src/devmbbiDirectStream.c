/*************************************************************************
* This is the StreamDevice interface for EPICS mbbiDirect records.
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

#include "mbbiDirectRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *)record;
    unsigned long val;

    if (format->type == DBF_ULONG || format->type == DBF_LONG)
    {
        if (streamScanf(record, format, &val) == ERROR) return ERROR;
        if (mbbiD->mask)
        {
            val &= mbbiD->mask;
            mbbiD->rval = val;
            return OK;
        }
        else
        {
            /* No MASK, (NOBT = 0): use VAL field */
            mbbiD->val = val; /* no cast because we cannot be sure about type of VAL */
            return DO_NOT_CONVERT;
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *)record;
    unsigned long val;

    if (format->type == DBF_ULONG || format->type == DBF_LONG)
    {
        if (mbbiD->mask) val = mbbiD->rval & mbbiD->mask;
        else val = mbbiD->val;
        return streamPrintf(record, format, val);
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *)record;

    mbbiD->mask <<= mbbiD->shft;
    return streamInitRecord(record, &mbbiD->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devmbbiDirectStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devmbbiDirectStream);
