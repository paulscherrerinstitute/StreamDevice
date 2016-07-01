/***************************************************************
* StreamDevice record interface for                            *
* multibit binary output direct records                        *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
* Please refer to the HTML files in ../doc/ for a detailed     *
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

#include <mbboDirectRecord.h>
#include "alarm.h"
#include "devStream.h"
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *) record;
    unsigned long val;

    if (format->type == DBF_ULONG || format->type == DBF_LONG)
    {
        if (streamScanf (record, format, &val)) return ERROR;
        if (mbboD->mask)
        {
            val &= mbboD->mask;
            mbboD->rbv = val;
            if (INIT_RUN) mbboD->rval = val;
            return OK;
        }
        else
        {
            /* No MASK, (NOBT = 0): use VAL field */
            mbboD->val = (short)val;
            return DO_NOT_CONVERT;
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *) record;
    long val;

    if (format->type == DBF_ULONG || format->type == DBF_LONG)
    {
        if (mbboD->mask) val = mbboD->rval & mbboD->mask;
        else val = mbboD->val;
        return streamPrintf (record, format, val);
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    mbboDirectRecord *mbboD = (mbboDirectRecord *) record;

    mbboD->mask <<= mbboD->shft;
    
    /* Workaround for bug in mbboDirect record:
       Put to VAL overwrites value to 0 if SEVR is INVALID_ALARM
       Thus first write may send a wrong value.
    */
    mbboD->sevr = 0;    
    return streamInitRecord (record, &mbboD->out, readData, writeData);
}

/* Unfortunately the bug also corrupts the next write to VAL after an I/O error.
   Thus make sure the record is never left in INVALID_ALARM status.
*/

static long write(dbCommon *record)
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
    write
};

epicsExportAddress(dset,devmbboDirectStream);
