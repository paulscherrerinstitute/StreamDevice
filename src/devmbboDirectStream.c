/***************************************************************
* StreamDevice record interface for                            *
* multibit binary output direct records                        *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
* Please refer to the HTML files in ../docs/ for a detailed    *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

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
