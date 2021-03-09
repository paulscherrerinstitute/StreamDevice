/*************************************************************************
* This is the StreamDevice interface for EPICS waveform records.
* Please see ../docs/ for detailed documentation.
*
* (C) 1999,2005 Dirk Zimoch (dirk.zimoch@psi.ch)
*
* This file is part of StreamDevice.
*
* StreamDevice is free software: You can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* StreamDevice is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with StreamDevice. If not, see https://www.gnu.org/licenses/.
*************************************************************************/

#include "waveformRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    waveformRecord *wf = (waveformRecord *)record;
    double dval;
    long lval;

    wf->rarm = 0;
    for (wf->nord = 0; wf->nord < wf->nelm; wf->nord++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                if (streamScanf(record, format, &dval) == ERROR)
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
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from double to %s\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
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
#ifdef DBR_INT64
                    case DBF_INT64:
                    case DBF_UINT64:
                        ((epicsInt64 *)wf->bptr)[wf->nord] = (epicsInt64)lval;
                        break;
#endif
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
                        errlogSevPrintf(errlogFatal,
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
                        if (streamScanfN(record, format,
                            (char *)wf->bptr + wf->nord * MAX_STRING_SIZE,
                            MAX_STRING_SIZE) == ERROR)
                        {
                            return wf->nord ? OK : ERROR;
                        }
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                    {
                        ssize_t length;
                        wf->nord = 0;
                        if ((length = streamScanfN(record, format,
                            (char *)wf->bptr, wf->nelm)) == ERROR)
                        {
                            return ERROR;
                        }
                        if (length < (ssize_t)wf->nelm)
                        {
                            ((char*)wf->bptr)[length] = 0;
                        }
                        wf->nord = (long)length;
                        return OK;
                    }
                    default:
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from string to %s\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogMajor,
                    "readData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[format->type].strvalue,
                    pamapdbfType[wf->ftvl].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    waveformRecord *wf = (waveformRecord *)record;
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
#ifdef DBR_INT64
                    case DBF_INT64:
                        dval = (double)((epicsInt64 *)wf->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        dval = (double)((epicsUInt64 *)wf->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        dval = ((epicsInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        dval = ((epicsUInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                    case DBF_ENUM:
                        dval = ((epicsInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                        dval = ((epicsUInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        dval = ((epicsInt8 *)wf->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        dval = ((epicsUInt8 *)wf->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to double\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, dval))
                    return ERROR;
                break;
            }
            case DBF_LONG:
            case DBF_ULONG:
            case DBF_ENUM:
            {
                switch (wf->ftvl)
                {
#ifdef DBR_INT64
                    case DBF_INT64:
                        lval = (long)((epicsInt64 *)wf->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        lval = (long)((epicsUInt64 *)wf->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        lval = ((epicsInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        lval = ((epicsUInt32 *)wf->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                    case DBF_ENUM:
                        lval = ((epicsInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                        lval = ((epicsUInt16 *)wf->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        lval = ((epicsInt8 *)wf->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        lval = ((epicsUInt8 *)wf->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to long\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, lval))
                    return ERROR;
                break;
            }
            case DBF_STRING:
            {
                switch (wf->ftvl)
                {
                    case DBF_STRING:
                        if (streamPrintf(record, format,
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
                        if (streamPrintf(record, format,
                            ((char *)wf->bptr)))
                            return ERROR;
                        return OK;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to string\n",
                            record->name, pamapdbfType[wf->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogFatal,
                    "writeData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[wf->ftvl].strvalue,
                    pamapdbfType[format->type].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long initRecord(dbCommon *record)
{
    waveformRecord *wf = (waveformRecord *)record;

    return streamInitRecord(record, &wf->inp, readData, writeData) == ERROR ?
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
