/***************************************************************
* StreamDevice record interface for                            *
* multibit binary input direct records                         *
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

#include "devStream.h"
#include <mbbiDirectRecord.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *) record;
    long val;

    if (format->type == DBF_LONG)
    {
        if (streamScanf (record, format, &val)) return ERROR;
        if (mbbiD->mask)
        {
            val &= mbbiD->mask;
            mbbiD->rval = val;
            return OK;
        }
        else
        {
            /* No MASK, (NOBT = 0): use VAL field */
            mbbiD->val = (short)val;
            return DO_NOT_CONVERT;
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *) record;
    long val;

    if (format->type == DBF_LONG)
    {
        if (mbbiD->mask) val = mbbiD->rval & mbbiD->mask;
        else val = mbbiD->val;
        return streamPrintf (record, format, val);
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    mbbiDirectRecord *mbbiD = (mbbiDirectRecord *) record;

    mbbiD->mask <<= mbbiD->shft;
    return streamInitRecord (record, &mbbiD->inp, readData, writeData) == ERROR ?
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
