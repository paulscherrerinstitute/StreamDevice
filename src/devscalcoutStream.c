/***************************************************************
* Stream Device record interface for scalcout records          *
*                                                              *
* (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
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

#include <devStream.h>
#include <sCalcoutRecord.h>
#include <epicsExport.h>

/* scalcout record has a bug: it never calls init_record
   of the device support.
   Fix: sCalcoutRecord.c, end of init_record() add
  
        if(pscalcoutDSET->init_record ) {
	    return (*pscalcoutDSET->init_record)(pcalc);
        }
   The bug has been fixed in version 2-6-1.
*/

static long readData (dbCommon *record, format_t *format)
{
    scalcoutRecord *sco = (scalcoutRecord *) record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamScanf (record, format, &sco->val);
        }
        case DBF_LONG:
        case DBF_ENUM:
        {
            long lval;

            if (streamScanf (record, format, &lval)) return ERROR;
            sco->val = lval;
            return OK;
        }
        case DBF_STRING:
        {
            return (streamScanfN (record, format,
                sco->sval, sizeof(sco->val)));
        }
    }
    return ERROR;
}

static long writeData (dbCommon *record, format_t *format)
{
    scalcoutRecord *sco = (scalcoutRecord *) record;

    switch (format->type)
    {
        case DBF_DOUBLE:
        {
            return streamPrintf (record, format, sco->oval);
        }
        case DBF_LONG:
        case DBF_ENUM:
        {
            return streamPrintf (record, format, (long)sco->oval);
        }
        case DBF_STRING:
        {
            return streamPrintf (record, format, sco->osv);
        }
    }
    return ERROR;
}

static long initRecord (dbCommon *record)
{
    scalcoutRecord *sco = (scalcoutRecord *) record;

    return streamInitRecord (record, &sco->out, readData, writeData);
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
