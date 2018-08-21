/***************************************************************
* StreamDevice record interface for aai records                *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
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

#include "aaiRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    aaiRecord *aai = (aaiRecord *)record;
    double dval;
    long lval;

    for (aai->nord = 0; aai->nord < aai->nelm; aai->nord++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                if (streamScanf(record, format, &dval) == ERROR)
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
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from double to %s\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_ULONG:
            case DBF_LONG:
            case DBF_ENUM:
            {
                if (streamScanf(record, format, &lval) == ERROR)
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
#ifdef DBF_INT64
                    case DBF_INT64:
                    case DBF_UINT64:
                        ((epicsInt64 *)aai->bptr)[aai->nord] = (epicsInt64)lval;
                        break;
#endif
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
                        errlogSevPrintf(errlogFatal,
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
                        if (streamScanfN(record, format,
                            (char *)aai->bptr + aai->nord * MAX_STRING_SIZE,
                            MAX_STRING_SIZE) == ERROR)
                        {
                            return aai->nord ? OK : ERROR;
                        }
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                    {
                        ssize_t length;
                        aai->nord = 0;
                        if ((length = streamScanfN(record, format,
                            (char *)aai->bptr, aai->nelm)) == ERROR)
                        {
                            return ERROR;
                        }
                        if (length < (ssize_t)aai->nelm)
                        {
                            ((char*)aai->bptr)[length] = 0;
                        }
                        aai->nord = (long)length;
                        return OK;
				    }
                    default:
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from string to %s\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogMajor,
                    "readData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[format->type].strvalue,
                    pamapdbfType[aai->ftvl].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    aaiRecord *aai = (aaiRecord *)record;
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
#ifdef DBF_INT64
                    case DBF_INT64:
                        dval = ((epicsInt64 *)aai->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        dval = ((epicsUInt64 *)aai->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        dval = ((epicsInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        dval = ((epicsUInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                    case DBF_ENUM:
                        dval = ((epicsInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                        dval = ((epicsUInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        dval = ((epicsInt8 *)aai->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        dval = ((epicsUInt8 *)aai->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to double\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, dval))
                    return ERROR;
                break;
            }
            case DBF_ULONG:
            case DBF_LONG:
            case DBF_ENUM:
            {
                switch (aai->ftvl)
                {
#ifdef DBF_INT64
                    case DBF_INT64:
                        lval = ((epicsInt64 *)aao->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        lval = ((epicsUInt64 *)aao->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        lval = ((epicsInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        lval = ((epicsUInt32 *)aai->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                    case DBF_ENUM:
                        lval = ((epicsInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                        lval = ((epicsUInt16 *)aai->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        lval = ((epicsInt8 *)aai->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        lval = ((epicsUInt8 *)aai->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to long\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, lval))
                    return ERROR;
                break;
            }
            case DBF_STRING:
            {
                switch (aai->ftvl)
                {
                    case DBF_STRING:
                        if (streamPrintf(record, format,
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
                        if (streamPrintf(record, format,
                            ((char *)aai->bptr)))
                            return ERROR;
                        return OK;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to string\n",
                            record->name, pamapdbfType[aai->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogFatal,
                    "writeData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[aai->ftvl].strvalue,
                    pamapdbfType[format->type].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long initRecord(dbCommon *record)
{
    aaiRecord *aai = (aaiRecord *)record;

    aai->bptr = calloc(aai->nelm, dbValueSize(aai->ftvl));
    if (aai->bptr == NULL)
    {
        errlogSevPrintf(errlogFatal,
            "initRecord %s: can't allocate memory for data array\n",
            record->name);
        return ERROR;
    }
    return streamInitRecord(record, &aai->inp, readData, writeData) == ERROR ?
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
