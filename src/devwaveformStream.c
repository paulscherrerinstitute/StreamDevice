/***************************************************************
* StreamDevice record interface for waveform records           *
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
#include <waveformRecord.h>
#include <string.h>
#include <errlog.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    waveformRecord *wf = (waveformRecord *) record;
    double dval;
    long lval;

    wf->rarm = 0;
    for (wf->nord = 0; wf->nord < wf->nelm; wf->nord++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                if (streamScanf (record, format, &dval) != OK)
                {
                    return wf->nord ? OK : ERROR;
                }
                switch (wf->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)wf->bptr)[wf->nord] = (epicsFloat64)dval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)wf->bptr)[wf->nord] = (epicsFloat32)dval;
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from double to %s\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_LONG:
            case DBF_ENUM:
            {
                if (streamScanf (record, format, &lval) != OK)
                {
                    return wf->nord ? OK : ERROR;
                }
                switch (wf->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)wf->bptr)[wf->nord] = (epicsFloat64)lval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)wf->bptr)[wf->nord] = (epicsFloat32)lval;
                        break;
                    case DBF_LONG:
                    case DBF_ULONG:
                        ((epicsInt32 *)wf->bptr)[wf->nord] = (epicsInt32)lval;
                        break;
                    case DBF_SHORT:
                    case DBF_USHORT:
                    case DBF_ENUM:
                        ((epicsInt16 *)wf->bptr)[wf->nord] = (epicsInt16)lval;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        ((epicsInt8 *)wf->bptr)[wf->nord] = (epicsInt8)lval;
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from long to %s\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_STRING:
            {
                switch (wf->ftvl)
                {
                    case DBF_STRING:
                        if (streamScanfN (record, format,
                            (char *)wf->bptr + wf->nord * MAX_STRING_SIZE,
                            MAX_STRING_SIZE) != OK)
                        {
                            return wf->nord ? OK : ERROR;
                        }
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        memset (wf->bptr, 0, wf->nelm);
                        wf->nord = 0;
                        if (streamScanfN (record, format,
                            (char *)wf->bptr, wf->nelm) != OK)
                        {
                            return ERROR;
                        }
                        ((char*)wf->bptr)[wf->nelm] = 0;
                        for (lval = wf->nelm;
                            lval >= 0 && ((char*)wf->bptr)[lval] == 0;
                            lval--);
                        wf->nord = lval+1;
                        return OK;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from string to %s\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf (errlogMajor,
                    "readData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[format->type].strvalue,
                    pamapdbfType[wf->ftvl].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long writeData (dbCommon *record, format_t *format)
{
    waveformRecord *wf = (waveformRecord *) record;
    double dval;
    long lval;
    unsigned long nowd;

    for (nowd = 0; nowd < wf->nord; nowd++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                switch (wf->ftvl)
                {
                    case DBF_DOUBLE:
                        dval = ((epicsFloat64 *)wf->bptr)[nowd];
                        break;
                    case DBF_FLOAT:
                        dval = ((epicsFloat32 *)wf->bptr)[nowd];
                        break;
                    case DBF_LONG:
                        dval = ((epicsInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        dval = ((epicsUInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        dval = ((epicsInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        dval = ((epicsUInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        dval = ((epicsInt8 *)wf->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        dval = ((epicsUInt8 *)wf->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to double\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf (record, format, dval))
                    return ERROR;
                break;
            }
            case DBF_LONG:
            case DBF_ENUM:
            {
                switch (wf->ftvl)
                {
                    case DBF_LONG:
                        lval = ((epicsInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        lval = ((epicsUInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        lval = ((epicsInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        lval = ((epicsUInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        lval = ((epicsInt8 *)wf->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        lval = ((epicsUInt8 *)wf->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to long\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf (record, format, lval))
                    return ERROR;
                break;
            }
            case DBF_STRING:
            {
                switch (wf->ftvl)
                {
                    case DBF_STRING:
                        if (streamPrintf (record, format,
                            ((char *)wf->bptr) + nowd * MAX_STRING_SIZE))
                            return ERROR;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        /* print waveform as a null-terminated string */
                        if (wf->nord < wf->nelm)
                        {
                            ((char *)wf->bptr)[wf->nord] = 0;
                        }
                        else
                        {
                            ((char *)wf->bptr)[wf->nelm-1] = 0;
                        }
                        if (streamPrintf (record, format,
                            ((char *)wf->bptr)))
                            return ERROR;
                        return OK;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to string\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf (errlogFatal,
                    "writeData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[wf->ftvl].strvalue,
                    pamapdbfType[format->type].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long initRecord (dbCommon *record)
{
    waveformRecord *wf = (waveformRecord *) record;

    return streamInitRecord (record, &wf->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devwaveformStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devwaveformStream);
