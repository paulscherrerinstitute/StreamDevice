/***************************************************************
* StreamDevice record interface for                            *
* multibit binary input records                                *
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
#include <mbbiRecord.h>
#include <string.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    mbbiRecord *mbbi = (mbbiRecord *) record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_LONG:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            /* read VAL or RBV? Look if any value is defined */
            if (mbbi->sdef) for (i=0; i<16; i++)
            {
                if ((&mbbi->zrvl)[i])
                {
                    if (mbbi->mask) val &= mbbi->mask;
                    mbbi->rval = val;
                    return OK;
                }
            }
            mbbi->val = (short)val;
            return DO_NOT_CONVERT;
        }
        case DBF_ENUM:
        {
            if (streamScanf (record, format, &val)) return ERROR;
            mbbi->val = (short)val;
            return DO_NOT_CONVERT;
        }
        case DBF_STRING:
        {
            char buffer[sizeof(mbbi->zrst)];
            if (streamScanfN (record, format, buffer, sizeof(buffer)))
                return ERROR;
            for (val = 0; val < 16; val++)
            {
                if (strcmp ((&mbbi->zrst)[val], buffer) == 0)
                {
                    mbbi->val = (short)val;
                    return DO_NOT_CONVERT;
                }
            }
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    mbbiRecord *mbbi = (mbbiRecord *) record;
    long val;
    int i;

    switch (format->type)
    {
        case DBF_LONG:
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
            return streamPrintf (record, format, val);
        }
        case DBF_ENUM:
        {
            return streamPrintf (record, format, (long) mbbi->val);
        }
        case DBF_STRING:
        {
            if (mbbi->val >= 16) return ERROR;
            return streamPrintf (record, format,
                mbbi->zrst + sizeof(mbbi->zrst) * mbbi->val);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    mbbiRecord *mbbi = (mbbiRecord *) record;

    mbbi->mask <<= mbbi->shft;
    return streamInitRecord (record, &mbbi->inp, readData, writeData) == ERROR ?
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
