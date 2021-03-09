/*************************************************************************
* This is the StreamDevice interface for EPICS mbbi records.
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

#include "mbbiRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    mbbiRecord *mbbi = (mbbiRecord *)record;
    unsigned long val;
    int i;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            /* read VAL or RBV? Look if any value is defined */
            if (mbbi->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbi->zrvl)[i])
                {
                    if (mbbi->mask) val &= mbbi->mask;
                    mbbi->rval = (epicsEnum16)val;
                    return OK;
                }
            }
            mbbi->val = (epicsEnum16)val;
            return DO_NOT_CONVERT;
        }
        case DBF_ENUM:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            mbbi->val = (epicsEnum16)val;
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(mbbi->zrst)];
            if (streamScanfN(record, format, buffer, sizeof(buffer)) == ERROR)
                return ERROR;
            for (val = 0; val < 16; val++)
            {
                if (strcmp ((&mbbi->zrst)[val], buffer) == 0)
                {
                    mbbi->val = (epicsEnum16)val;
                    return DO_NOT_CONVERT;
                }
            }
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    mbbiRecord *mbbi = (mbbiRecord *)record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_LONG:
        case DBF_ULONG:
        {
            /* print VAL or RVAL ? Look if any value is defined */
            val = mbbi->val;
            if (mbbi->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbi->zrvl)[i])
                {
                    val = mbbi->rval;
                    if (mbbi->mask) val &= mbbi->mask;
                    break;
                }
            }
            return streamPrintf(record, format, val);
        }
        case DBF_ENUM:
        {
            return streamPrintf(record, format, (long)mbbi->val);
        }
        case DBF_STRING:
        {
            if (mbbi->val >= 16) return ERROR;
            return streamPrintf(record, format,
                mbbi->zrst + sizeof(mbbi->zrst) * mbbi->val);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    mbbiRecord *mbbi = (mbbiRecord *)record;

    mbbi->mask <<= mbbi->shft;
    return streamInitRecord(record, &mbbi->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devmbbiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devmbbiStream);
