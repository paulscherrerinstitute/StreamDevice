/***************************************************************
* StreamDevice record interface for                            *
* multibit binary output records                               *
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
#include <mbboRecord.h>
#include <string.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    mbboRecord *mbbo = (mbboRecord *) record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_LONG:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            /* read VAL or RBV? Look if any value is defined */
            if (mbbo->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbo->zrvl)[i])
                {
                    if (mbbo->mask) val &= mbbo->mask;
                    mbbo->rbv = val;
                    if (INIT_RUN) mbbo->rval = val;
                    return OK;
                }
            }
            mbbo->val = (short)val;
            return DO_NOT_CONVERT;
        }
        case DBF_ENUM:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            mbbo->val = (short)val;
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(mbbo->zrst)];
            if (streamScanfN (record, format, buffer, sizeof(buffer)))
                return ERROR;
            for (val = 0; val < 16; val++)
            {
                if (strcmp ((&mbbo->zrst)[val], buffer) == 0)
                {
                    mbbo->val = (short)val;
                    return DO_NOT_CONVERT;
                }
            }
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    mbboRecord *mbbo = (mbboRecord *) record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_LONG:
        {
            /* print VAL or RVAL ? */
            val = mbbo->val;
            if (mbbo->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbo->zrvl)[i])
                {
                    /* any values defined ? */
                    val = mbbo->rval;
                    if (mbbo->mask) val &= mbbo->mask;
                    break;
                }
            }
            return streamPrintf (record, format, val);
        }
        case DBF_ENUM:
        {
            return streamPrintf (record, format, (long) mbbo->val);
        }
        case DBF_STRING:
        {
            if (mbbo->val >= 16) return ERROR;
            return streamPrintf (record, format,
                mbbo->zrst + sizeof(mbbo->zrst) * mbbo->val);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    mbboRecord *mbbo = (mbboRecord *) record;

    mbbo->mask <<= mbbo->shft;
    return streamInitRecord (record, &mbbo->out, readData, writeData);
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
