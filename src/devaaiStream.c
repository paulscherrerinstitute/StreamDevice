/***************************************************************
* StreamDevice record interface for aai records                *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
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

#include <string.h>
#include <stdlib.h>
#include "devStream.h"
#include <aaiRecord.h>
#include <errlog.h>
#include <epicsExport.h>

static long readData (dbCommon *record, format_t *format)
{
    aaiRecord *aai = (aaiRecord *) record;
    double dval;
    long lval;

    for (aai->nord = 0; aai->nord < aai->nelm; aai->nord++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                if (streamScanf (record, format, &dval) != OK)
                {
                    return aai->nord ? OK : ERROR;
                }
                switch (aai->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)aai->bptr)[aai->nord] = (epicsFloat64)dval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)aai->bptr)[aai->nord] = (epicsFloat32)dval;
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from double to %s\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_LONG:
            case DBF_ENUM:
            {
                if (streamScanf (record, format, &lval) != OK)
                {
                    return aai->nord ? OK : ERROR;
                }
                switch (aai->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)aai->bptr)[aai->nord] = (epicsFloat64)lval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)aai->bptr)[aai->nord] = (epicsFloat32)lval;
                        break;
                    case DBF_LONG:
                    case DBF_ULONG:
                        ((epicsInt32 *)aai->bptr)[aai->nord] = (epicsInt32)lval;
                        break;
                    case DBF_SHORT:
                    case DBF_USHORT:
                    case DBF_ENUM:
                        ((epicsInt16 *)aai->bptr)[aai->nord] = (epicsInt16)lval;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        ((epicsInt8 *)aai->bptr)[aai->nord] = (epicsInt8)lval;
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from long to %s\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_STRING:
            {
                switch (aai->ftvl)
                {
                    case DBF_STRING:
                        if (streamScanfN (record, format,
                            (char *)aai->bptr + aai->nord * MAX_STRING_SIZE,
                            MAX_STRING_SIZE) != OK)
                        {
                            return aai->nord ? OK : ERROR;
                        }
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        memset (aai->bptr, 0, aai->nelm);
                        aai->nord = 0;
                        if (streamScanfN (record, format,
                            (char *)aai->bptr, aai->nelm) != OK)
                        {
                            return ERROR;
                        }
                        ((char*)aai->bptr)[aai->nelm] = 0;
                        for (lval = aai->nelm;
                            lval >= 0 && ((char*)aai->bptr)[lval] == 0;
                            lval--);
                        aai->nord = lval+1;
                        return OK;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "readData %s: can't convert from string to %s\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf (errlogMajor,
                    "readData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[format->type].strvalue,
                    pamapdbfType[aai->ftvl].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long writeData (dbCommon *record, format_t *format)
{
    aaiRecord *aai = (aaiRecord *) record;
    double dval;
    long lval;
    unsigned long nowd;

    for (nowd = 0; nowd < aai->nord; nowd++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                switch (aai->ftvl)
                {
                    case DBF_DOUBLE:
                        dval = ((epicsFloat64 *)aai->bptr)[nowd];
                        break;
                    case DBF_FLOAT:
                        dval = ((epicsFloat32 *)aai->bptr)[nowd];
                        break;
                    case DBF_LONG:
                        dval = ((epicsInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        dval = ((epicsUInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        dval = ((epicsInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        dval = ((epicsUInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        dval = ((epicsInt8 *)aai->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        dval = ((epicsUInt8 *)aai->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to double\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf (record, format, dval))
                    return ERROR;
                break;
            }
            case DBF_LONG:
            case DBF_ENUM:
            {
                switch (aai->ftvl)
                {
                    case DBF_LONG:
                        lval = ((epicsInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        lval = ((epicsUInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        lval = ((epicsInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        lval = ((epicsUInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        lval = ((epicsInt8 *)aai->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        lval = ((epicsUInt8 *)aai->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to long\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf (record, format, lval))
                    return ERROR;
                break;
            }
            case DBF_STRING:
            {
                switch (aai->ftvl)
                {
                    case DBF_STRING:
                        if (streamPrintf (record, format,
                            ((char *)aai->bptr) + nowd * MAX_STRING_SIZE))
                            return ERROR;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        /* print aai as a null-terminated string */
                        if (aai->nord < aai->nelm)
                        {
                            ((char *)aai->bptr)[aai->nord] = 0;
                        }
                        else
                        {
                            ((char *)aai->bptr)[aai->nelm-1] = 0;
                        }
                        if (streamPrintf (record, format,
                            ((char *)aai->bptr)))
                            return ERROR;
                        return OK;
                    default:
                        errlogSevPrintf (errlogFatal,
                            "writeData %s: can't convert from %s to string\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf (errlogFatal,
                    "writeData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[aai->ftvl].strvalue,
                    pamapdbfType[format->type].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long initRecord (dbCommon *record)
{
    static const int typesize[] = {MAX_STRING_SIZE,1,1,2,2,4,4,4,8,2};
    aaiRecord *aai = (aaiRecord *) record;

    aai->bptr = calloc(aai->nelm, typesize[aai->ftvl]);
    if (aai->bptr == NULL)
    {
        errlogSevPrintf (errlogFatal,
            "initRecord %s: can't allocate memory for data array\n",
            record->name);
        return ERROR;
    }
    return streamInitRecord (record, &aai->inp, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devaaiStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamRead
};

epicsExportAddress(dset,devaaiStream);
