/*************************************************************************
* This is the StreamDevice interface for EPICS mbboDirect records.
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

#include "mbboDirectRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *)record;
    unsigned long val;
    unsigned short monitor_mask;
    unsigned int i;
    unsigned char *bit;

    if (format->type != DBF_ULONG && format->type != DBF_LONG)
        return ERROR;
    if (streamScanf(record, format, &val) == ERROR) return ERROR;
    if (mbboD->mask) val &= mbboD->mask;

    mbboD->rbv = val;
    mbboD->rval = val;
    val >>= mbboD->shft;
    mbboD->val = val; /* no cast because we cannot be sure about type of VAL */

    if (record->pact) return DO_NOT_CONVERT;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (mbboD->mlst != mbboD->val)
    {
        monitor_mask |= (DBE_VALUE | DBE_LOG);
        mbboD->mlst = mbboD->val;
    }
    if (monitor_mask)
    {
        db_post_events(record, &mbboD->val, monitor_mask);
    }
    if (mbboD->oraw != mbboD->rval)
    {
        db_post_events(record, &mbboD->rval, monitor_mask | DBE_VALUE | DBE_LOG);
        mbboD->oraw = mbboD->rval;
    }
    if (mbboD->orbv != mbboD->rbv)
    {
        db_post_events(record, &mbboD->rbv, monitor_mask | DBE_VALUE | DBE_LOG);
        mbboD->orbv = mbboD->rbv;
    }
    /* update the bits */
    for (i = 0; i < sizeof(mbboD->val) * 8; i++)
    {
        bit = &(mbboD->b0) + i;
        if ((val & 1) == !*bit)
        {
            *bit = val & 1;
            db_post_events(record, bit, monitor_mask | DBE_VALUE | DBE_LOG);
        }
        else if (monitor_mask)
            db_post_events(record, bit, monitor_mask);
        val >>= 1;
    }
    return DO_NOT_CONVERT;
}

static long writeData(dbCommon *record, format_t *format)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *)record;
    long val;

    switch (format->type)
    {
        case DBF_ULONG:
            val = mbboD->rval;
            if (mbboD->mask) val &= mbboD->mask;
            break;
        case DBF_LONG:
        case DBF_ENUM:
            val = (epicsInt32)mbboD->rval;
            if (mbboD->mask) val &= (epicsInt32)mbboD->mask;
            break;
        default:
            return ERROR;
    }
    return streamPrintf(record, format, val);
}

static long initRecord(dbCommon *record)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *)record;
    mbboD->mask <<= mbboD->shft;

    /* Workaround for bug in mbboDirect record:
       Put to VAL overwrites value to 0 if SEVR is INVALID_ALARM
       Thus first write may send a wrong value.
    */
    mbboD->sevr = 0;
    return streamInitRecord(record, &mbboD->out, readData, writeData);
}

/* Unfortunately the bug also corrupts the next write to VAL after an I/O error.
   Thus make sure the record is never left in INVALID_ALARM status.
*/

static long write_mbbo(dbCommon *record)
{
    long status = streamWrite(record);
    if (record->nsev == INVALID_ALARM) record->nsev = MAJOR_ALARM;
    return status;
}


struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devmbboDirectStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    write_mbbo
};

epicsExportAddress(dset,devmbboDirectStream);
