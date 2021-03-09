/*************************************************************************
* This is the StreamDevice interface for EPICS ai records.
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

#include "aiRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    aiRecord *ai = (aiRecord *)record;
    double val;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            break;
        }
        case DBF_ULONG:
        case DBF_LONG:
        {
            long rval;
            if (streamScanf(record, format, &rval) == ERROR) return ERROR;
            ai->rval = rval;
            if (ai->linr == 0)
            {
                /* allow integers with more than 32 bits */
                if (format->type == DBF_ULONG)
                    val = (unsigned long)rval;
                else
                    val = rval;
                break;
            }
            return OK;
        }
        default:
            return ERROR;
    }
    if (ai->aslo != 0.0 && ai->aslo != 1.0) val *= ai->aslo;
    val += ai->aoff;
    if (!(ai->smoo == 0.0 || ai->init || ai->udf || isinf(ai->val) || isnan(ai->val)))
        val = ai->val * ai->smoo + val * (1.0 - ai->smoo);
    ai->val = val;
    return DO_NOT_CONVERT;
}

static long writeData(dbCommon *record, format_t *format)
{
    aiRecord *ai = (aiRecord *)record;

    double val = ai->val - ai->aoff;
    if (ai->aslo != 0.0 && ai->aslo != 1.0) val /= ai->aslo;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamPrintf(record, format, val);
        }
        case DBF_ULONG:
        {
            if (ai->linr == 0)
            {
                /* allow more bits than 32 */
                return streamPrintf(record, format, (unsigned long)val);
            }
            return streamPrintf(record, format, (unsigned long)ai->rval);
        }
        case DBF_LONG:
        {
            if (ai->linr == 0)
            {
                /* allow more bits than 32 */
                return streamPrintf(record, format, (long)val);
            }
            return streamPrintf(record, format, (long)ai->rval);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    aiRecord *ai = (aiRecord *)record;

    return streamInitRecord(record, &ai->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} devaiStream = {
    6,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead,
    NULL
};

epicsExportAddress(dset,devaiStream);
