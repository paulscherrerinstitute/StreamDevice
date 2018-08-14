/***************************************************************
* StreamDevice record interface for analog input records       *
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
