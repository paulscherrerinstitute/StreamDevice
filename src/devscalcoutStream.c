/*************************************************************************
* This is the StreamDevice interface for EPICS scalcout records.
* Please see ../docs/ for detailed documentation.
*
* (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)
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

#include "sCalcoutRecord.h"
#include "devStream.h"

/* Up to version 2-6-1 of the SynApps calc module
   scalcout record has a bug: it never calls init_record
   of the device support.
   Fix: sCalcoutRecord.c, end of init_record() add

        if (pscalcoutDSET->init_record ) {
            return (*pscalcoutDSET->init_record)(pcalc);
        }
*/

static long readData(dbCommon *record, format_t *format)
{
    scalcoutRecord *sco = (scalcoutRecord *)record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            if (streamScanf(record, format, &sco->val) == ERROR) return ERROR;
            return OK;
        }
        case DBF_LONG:
        case DBF_ULONG:
        case DBF_ENUM:
        {
            long lval;

            if (streamScanf(record, format, &lval) == ERROR) return ERROR;
            sco->val = lval;
            return OK;
        }
        case DBF_STRING:
        {
            if ((streamScanfN(record, format,
                sco->sval, sizeof(sco->val)) == ERROR)) return ERROR;
            return OK;
        }
    }
    return ERROR;
}

static long writeData(dbCommon *record, format_t *format)
{
    scalcoutRecord *sco = (scalcoutRecord *)record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamPrintf(record, format, sco->oval);
        }
        case DBF_LONG:
        case DBF_ULONG:
        case DBF_ENUM:
        {
            return streamPrintf(record, format, (long)sco->oval);
        }
        case DBF_STRING:
        {
            return streamPrintf(record, format, sco->osv);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    scalcoutRecord *sco = (scalcoutRecord *)record;

    return streamInitRecord(record, &sco->out, readData, writeData);
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devscalcoutStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite,
};

epicsExportAddress(dset,devscalcoutStream);
