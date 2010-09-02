/***************************************************************
* StreamDevice Support                                         *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an example application initializer for StreamDevice. *
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

#include <epicsThread.h>
#include <iocsh.h>
#include <devStream.h>

int main(int argc,char *argv[])
{
#ifdef DEBUGFILE
#define STR2(x) #x
#define STR(x) STR2(x)
    StreamDebugFile = fopen(STR(DEBUGFILE), "w");
#endif
    if(argc>=2) {
        iocsh(argv[1]);
        epicsThreadSleep(.2);
    }
    iocsh(NULL);
    return(0);
}
