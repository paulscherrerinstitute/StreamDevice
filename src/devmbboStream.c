/*************************************************************************
* This is the StreamDevice interface for EPICS mbbo records.
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

#include "mbboRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    mbboRecord *mbbo = (mbboRecord *)record;
    unsigned long val;
    int i;
    unsigned short monitor_mask;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_LONG:
        case DBF_ENUM:
        {
            if (streamScanf(record, format, &val) == ERROR) return ERROR;
            if (mbbo->mask) val &= mbbo->mask;
            mbbo->rbv = val;
            mbbo->rval = val;
            if (mbbo->shft > 0) val >>= mbbo->shft;
            /* read VAL or RBV? Look if any value is defined */
            if (mbbo->sdef)
            {
                mbbo->val = 65535; /* initalize to unknown state*/
                for (i = 0; i < 16; i++)
                {
                    if ((&mbbo->zrvl)[i] == val)
                    {
                        mbbo->val = i;
                        break;
                    }
                }
                break;
            }
            mbbo->val = (epicsEnum16)val;
            break;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(mbbo->zrst)];
            if (streamScanfN(record, format, buffer, sizeof(buffer)) == ERROR)
                return ERROR;
            mbbo->val = 65535; /* initalize to unknown state*/
            for (val = 0; val < 16; val++)
            {
                if (strcmp ((&mbbo->zrst)[val], buffer) == 0)
                {
                    mbbo->val = (epicsEnum16)val;
                    break;
                }
            }
            break;
        }
        default:
            return ERROR;
    }
    if (record->pact) return DO_NOT_CONVERT;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(record);
    if (mbbo->val > 15) {
        recGblSetSevr(record, STATE_ALARM, mbbo->unsv);
    } else {
        recGblSetSevr(record, STATE_ALARM, (&(mbbo->zrsv))[mbbo->val]);
    }
    mbbo->lalm = mbbo->val;
    if (mbbo->val != mbbo->lalm) {
        if (!recGblSetSevr(record, COS_ALARM, mbbo->cosv)) mbbo->lalm = mbbo->val;
    }
    if (mbbo->mlst != mbbo->val)
    {
        monitor_mask |= (DBE_VALUE | DBE_LOG);
        mbbo->mlst = mbbo->val;
    }
    if (monitor_mask) {
        db_post_events(record, &mbbo->val, monitor_mask);
    }
    if (mbbo->oraw != mbbo->rval) {
        db_post_events(record, &mbbo->rval, monitor_mask | DBE_VALUE);
        mbbo->oraw = mbbo->rval;
    }
    if (mbbo->orbv != mbbo->rbv) {
        db_post_events(record, &mbbo->rbv, monitor_mask | DBE_VALUE);
        mbbo->orbv = mbbo->rbv;
    }
    return DO_NOT_CONVERT;
}

static long writeData(dbCommon *record, format_t *format)
{
    mbboRecord *mbbo = (mbboRecord *)record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_ULONG:
        case DBF_ENUM:
            /* print VAL or RVAL ? */
            val = mbbo->val;
            if (mbbo->shft > 0) val <<= mbbo->shft;
            if (mbbo->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbo->zrvl)[i])
                {
                    /* any values defined ? */
                    val = mbbo->rval;
                    break;
                }
            }
            if (mbbo->mask) val &= mbbo->mask;
            return streamPrintf(record, format, val);
        case DBF_LONG:
        {
            /* print VAL or RVAL ? */
            val = (epicsInt16)mbbo->val;
            if (mbbo->shft > 0) val <<= mbbo->shft;
            if (mbbo->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbo->zrvl)[i])
                {
                    /* any values defined ? */
                    val = (epicsInt32)mbbo->rval;
                    break;
                }
            }
            if (mbbo->mask) val &= mbbo->mask;
            return streamPrintf(record, format, val);
        }
        case DBF_STRING:
        {
            if (mbbo->val >= 16) return ERROR;
            return streamPrintf(record, format,
                mbbo->zrst + sizeof(mbbo->zrst) * mbbo->val);
        }
    }
    return ERROR;
}

static long initRecord(dbCommon *record)
{
    mbboRecord *mbbo = (mbboRecord *)record;

    mbbo->mask <<= mbbo->shft;
    return streamInitRecord(record, &mbbo->out, readData, writeData);
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
} devmbboStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devmbboStream);
