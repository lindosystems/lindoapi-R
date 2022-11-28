/*  rLindo.c
    The R interface to LINDO API 11.0 - 14.0
    This file includes all C wrapper functions for LINDO API C functions.
    Copyright (C) 2017-2022 LINDO Systems.
*/


#include "rLindo.h"

#define LSASSERT(a) {if ((errorcode = (a)) != 0) goto ErrorReturn;}

#define DCL_BUF(Nx) \
    int errorcode = 0;\
    int    nz, m, n;\
    char  *sbuf[Nx];\
    int    ibuf[Nx];\
    double dbuf[Nx];\
    double *dvecptr[Nx];\
    int    *ivecptr[Nx];\
    char   *svecptr[Nx];


#define ZERO_BUF(Nx) {\
    memset(dvecptr, 0, Nx * sizeof(double*));\
    memset(ivecptr, 0, Nx * sizeof(int*));\
    memset(svecptr, 0, Nx * sizeof(char*));\
    memset(sbuf, 0, Nx * sizeof(char*));\
    memset(ibuf, 0, Nx * sizeof(int));\
    memset(dbuf, 0, Nx * sizeof(double));\
    errorcode = nz = m = n = 0;\
}

#ifdef _WIN32
#include <windows.h>
void Rwin_fpset();
BOOL WINAPI DllMain(HINSTANCE module_handle, DWORD reason_for_call, LPVOID reserved)
{
	if (reason_for_call == DLL_PROCESS_ATTACH)
	{
		Rwin_fpset();
	}

    return TRUE;
}
#endif

#define INI_ERR_CODE \
PROTECT(spnErrorCode = NEW_INTEGER(1));\
nProtect += 1;\
pnErrorCode = INTEGER_POINTER(spnErrorCode);\
*pnErrorCode = LSERR_NO_ERROR;\

#define CHECK_MODEL_ERROR \
if(sModel != R_NilValue && R_ExternalPtrTag(sModel) == tagLSprob)\
{\
    prModel = (prLSmodel)R_ExternalPtrAddr(sModel);\
    if(prModel == NULL)\
    {\
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
        goto ErrorReturn;\
    }\
    pModel = prModel->pModel;\
}\
else\
{\
    *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
    goto ErrorReturn;\
}\

#define CHECK_ENV_ERROR \
if(sEnv != R_NilValue && R_ExternalPtrTag(sEnv) == tagLSenv)\
{\
    prEnv = (prLSenv)R_ExternalPtrAddr(sEnv);\
    if(prEnv == NULL)\
    {\
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
        goto ErrorReturn;\
    }\
    pEnv = prEnv->pEnv;\
}\
else\
{\
    *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
    goto ErrorReturn;\
}\

#define CHECK_SAMPLE_ERROR \
if(sSample != R_NilValue && R_ExternalPtrTag(sSample) == tagLSsample)\
{\
    prSample = (prLSsample)R_ExternalPtrAddr(sSample);\
    if(prSample == NULL)\
    {\
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
        goto ErrorReturn;\
    }\
    pSample = prSample->pSample;\
}\
else\
{\
    *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
    goto ErrorReturn;\
}\

#define CHECK_RG_ERROR \
if(sRG != R_NilValue && R_ExternalPtrTag(sRG) == tagLSrandGen)\
{\
    prRG = (prLSrandGen)R_ExternalPtrAddr(sRG);\
    if(prRG == NULL)\
    {\
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
        goto ErrorReturn;\
    }\
    pRG = prRG->pRG;\
}\
else\
{\
    *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;\
    goto ErrorReturn;\
}\

#define CHECK_ERRCODE \
if(*pnErrorCode != LSERR_NO_ERROR)\
{\
    goto ErrorReturn;\
}\

#define MAKE_CHAR_ARRAY(carrayname, scarrayname)\
if(scarrayname == R_NilValue)\
{\
    carrayname = NULL;\
}\
else\
{\
    carrayname = (char *) CHAR(STRING_ELT(scarrayname,0));\
}\

#define MAKE_CHAR_CHAR_ARRAY(ccarrayname,sccarrayname)\
if (sccarrayname == R_NilValue) \
{\
    ccarrayname = NULL;\
}\
else\
{\
    int k;\
    int numcn = Rf_length(sccarrayname);\
    ccarrayname = R_Calloc(numcn, char *);\
    for(k = 0; k < numcn; k++)\
    {\
        ccarrayname[k] = (char *) CHAR(STRING_ELT(sccarrayname, k));\
    }\
}\

#define MAKE_REAL_ARRAY(carrayname, scarrayname)\
if(scarrayname == R_NilValue)\
{\
    carrayname = NULL;\
}\
else\
{\
    carrayname = REAL(scarrayname);\
}\

#define MAKE_INT_ARRAY(carrayname, scarrayname)\
if(scarrayname == R_NilValue)\
{\
    carrayname = NULL;\
}\
else\
{\
    carrayname = INTEGER(scarrayname);\
}\

#define SET_UP_LIST \
PROTECT(rList = allocVector(VECSXP, nNumItems));\
PROTECT(ListNames = allocVector(STRSXP,nNumItems));\
for(nIdx = 0; nIdx < nNumItems; nIdx++)\
    SET_STRING_ELT(ListNames,nIdx,mkChar(Names[nIdx]));\
setAttrib(rList, R_NamesSymbol, ListNames);\

#define SET_PRINT_LOG(pModel,nErrorCode)\
nErrorCode = LSsetModelLogfunc(pModel,(printLOG_t)rPrintLog,NULL);\
if(nErrorCode)\
{\
    Rprintf("Failed to set printing log (error %d)\n",nErrorCode);\
    R_FlushConsole();\
}\

#define SET_PRINT_LOG_NULL(pModel,nErrorCode)\
nErrorCode = LSsetModelLogfunc(pModel,NULL,NULL);\
if(nErrorCode)\
{\
    Rprintf("Failed to set printing log (error %d)\n",nErrorCode);\
    R_FlushConsole();\
}\

#define SET_MODEL_CALLBACK(pModel,nErrorCode)\
nErrorCode = LSsetCallback(pModel,(cbFunc_t)rCallBack,NULL);\
if(nErrorCode != LSERR_NO_ERROR)\
{\
    Rprintf("Failed to set callback (error %d)\n",nErrorCode);\
    R_FlushConsole();\
}\

SEXP tagLSprob;
SEXP tagLSenv;
SEXP tagLSsample;
SEXP tagLSrandGen;

static void LS_CALLTYPE rPrintLog(pLSmodel model,
                                  char     *line,
                                  void     *notting)
{
    Rprintf("%s",line);
    R_FlushConsole();
}

int CALLBACKTYPE rCallBack(pLSmodel model,
                           int      nLocation,
                           void     *pData)
{
    R_ProcessEvents();
    return 0;
}

int  CALLBACKTYPE rMipCallBack(pLSmodel model,
                               void     *pvUserData,
                               double   dObjval,
                               double   *padPrimal)
{
    return 0;
}

/******************************************************
 * Structure Creation and Deletion Routines (5)       *
 ******************************************************/
SEXP rcLScreateEnv()
{
    int      nErrorCode = LSERR_NO_ERROR;
    char     MY_LICENSE_KEY[1024];
    pLSenv   pEnv;
    prLSenv  prEnv = NULL;
    char     pachLicPath[256];
    SEXP     sEnv = R_NilValue;

    tagLSprob = Rf_install("TYPE_LSPROB");
    tagLSenv = Rf_install("TYPE_LSENV");
    tagLSsample = Rf_install("TYPE_LSSAMP");
    tagLSrandGen = Rf_install("TYPE_LSRG");

    prEnv = (prLSenv)malloc(sizeof(rLSenv)*1);
    if(prEnv == NULL)
    {
        return R_NilValue;
    }

    sprintf(pachLicPath,"%s//license//lndapi%d%d.lic",getenv("LINDOAPI_HOME"),LS_MAJOR_VER_NUMBER,LS_MINOR_VER_NUMBER);
    fprintf(stdout,"Loading license file '%s'\n",pachLicPath); fflush(stdout);
    nErrorCode = LSloadLicenseString(pachLicPath,MY_LICENSE_KEY);

    if(nErrorCode != LSERR_NO_ERROR)
    {
        Rprintf("Failed to load license key (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    pEnv = LScreateEnv(&nErrorCode, MY_LICENSE_KEY);
    if(nErrorCode)
    {
        Rprintf("Failed to create enviroment object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prEnv->pEnv = pEnv;

    sEnv = R_MakeExternalPtr(prEnv,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sEnv,tagLSenv);

    return sEnv;
}

SEXP rcLScreateModel(SEXP sEnv)
{
    int       nErrorCode = LSERR_NO_ERROR;
    prLSenv   prEnv = NULL;
    pLSenv    pEnv = NULL;
    prLSmodel prModel = NULL;
    pLSmodel  pModel = NULL;
    SEXP      sModel = R_NilValue;

    if(sEnv != R_NilValue && R_ExternalPtrTag(sEnv) == tagLSenv)
    {
        prEnv = (prLSenv)R_ExternalPtrAddr(sEnv);
        if(prEnv == NULL)
        {
            nErrorCode = LSERR_ILLEGAL_NULL_POINTER;
            Rprintf("Failed to create model object (error %d)\n",nErrorCode);
            R_FlushConsole();
            return R_NilValue;
        }
        pEnv = prEnv->pEnv;
    }
    else
    {
        nErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        Rprintf("Failed to create model object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prModel = (prLSmodel)malloc(sizeof(rLSmodel)*1);
    if(prModel == NULL)
    {
        return R_NilValue;
    }

    pModel = LScreateModel(pEnv,&nErrorCode);
    if(nErrorCode)
    {
        Rprintf("Failed to create model object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    SET_PRINT_LOG(pModel,nErrorCode);
    if(nErrorCode)
    {
        return R_NilValue;
    }

    SET_MODEL_CALLBACK(pModel,nErrorCode);
    if(nErrorCode)
    {
        return R_NilValue;
    }

    prModel->pModel = pModel;

    sModel = R_MakeExternalPtr(prModel,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sModel,tagLSprob);

    return sModel;
}

SEXP rcLSdeleteEnv(SEXP sEnv)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSdeleteEnv(&pEnv);

    prEnv->pEnv = NULL;

    R_ClearExternalPtr(sEnv);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteModel(SEXP sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteModel(&pModel);

    prModel->pModel = NULL;

    R_ClearExternalPtr(sModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLScopyParam(SEXP ssourceModel,
                   SEXP stargetModel,
                   SEXP smSolverType)
{
    prLSmodel prsourceModel;
    pLSmodel  sourceModel;
    prLSmodel prtargetModel;
    pLSmodel  targetModel;
    int       mSolverType = Rf_asInteger(smSolverType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    if(ssourceModel != R_NilValue && stargetModel != R_NilValue &&
        R_ExternalPtrTag(ssourceModel) == tagLSprob &&
        R_ExternalPtrTag(stargetModel) == tagLSprob)
    {
        prsourceModel = (prLSmodel)R_ExternalPtrAddr(ssourceModel);
        prtargetModel = (prLSmodel)R_ExternalPtrAddr(stargetModel);
        sourceModel = prsourceModel->pModel;
        targetModel = prtargetModel->pModel;
    }
    else
    {
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        goto ErrorReturn;
    }

    *pnErrorCode = LScopyParam(sourceModel,targetModel,mSolverType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
 * Model I-O Routines (18)                              *
 ********************************************************/
SEXP rcLSreadMPSFile(SEXP sModel,
                     SEXP spszFname,
                     SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadMPSFile(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteMPSFile(SEXP sModel,
                      SEXP spszFname,
                      SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteMPSFile(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadLINDOFile(SEXP sModel,
                       SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadLINDOFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteLINDOFile(SEXP sModel,
                        SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteLINDOFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadLINDOStream(SEXP sModel,
                         SEXP spszStream,
                         SEXP snStreamLen)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszStream = (char *) CHAR(STRING_ELT(spszStream,0));
    int       nStreamLen = Rf_asInteger(snStreamLen);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadLINDOStream(pModel,pszStream,nStreamLen);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteLINGOFile(SEXP sModel,
                        SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteLINGOFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteDualMPSFile(SEXP sModel,
                          SEXP spszFname,
                          SEXP snFormat,
                          SEXP snObjSense)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);
    int       nObjSense = Rf_asInteger(snObjSense);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteDualMPSFile(pModel,pszFname,nFormat,nObjSense);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteSolution(SEXP sModel,
                       SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteSolution(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteSolutionOfType(SEXP sModel,
                             SEXP spszFname,
                             SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteSolutionOfType(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteIIS(SEXP sModel,
                  SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteIIS(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteIUS(SEXP sModel,
                  SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteIUS(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadMPIFile(SEXP sModel,
                     SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadMPIFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteMPIFile(SEXP sModel,
                      SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteMPIFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteWithSetsAndSC(SEXP sModel,
                            SEXP spszFname,
                            SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteWithSetsAndSC(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadBasis(SEXP sModel,
                   SEXP spszFname,
                   SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadBasis(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSwriteBasis(SEXP sModel,
                    SEXP spszFname,
                    SEXP snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteBasis(pModel,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSreadLPFile(SEXP sModel,
                    SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadLPFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);

    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSreadLPStream(SEXP sModel,
                      SEXP spszStream,
                      SEXP snStreamLen)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszStream = (char *) CHAR(STRING_ELT(spszStream,0));
    int       nStreamLen = Rf_asInteger(snStreamLen);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadLPStream(pModel,pszStream,nStreamLen);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);

    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSreadSDPAFile(SEXP sModel,
                      SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadSDPAFile(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);

    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSsetPrintLogNull(SEXP sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    SET_PRINT_LOG_NULL(pModel,*pnErrorCode);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);

    UNPROTECT(nProtect + 2);


    return rList;
}

/********************************************************
 * Error Handling Routines (3)                          *
 ********************************************************/
SEXP rcLSgetErrorMessage(SEXP   sEnv,
                         SEXP   snErrorCode)
{
    prLSenv       prEnv;
    pLSenv        pEnv;
    int           nErrorCode = Rf_asInteger(snErrorCode);

    int           *pnErrorCode;
    SEXP          spnErrorCode = R_NilValue;
    char          pachMessage[LS_MAX_ERROR_MESSAGE_LENGTH];
    SEXP          spachMessage = R_NilValue;
    SEXP          rList = R_NilValue;
    char          *Names[2] = {"ErrorCode", "pachMessage"};
    SEXP          ListNames = R_NilValue;
    int           nNumItems = 2;
    int           nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    //description item
    PROTECT(spachMessage = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetErrorMessage(pEnv,nErrorCode,pachMessage);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachMessage,0,mkChar(pachMessage));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachMessage);
    }

    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSgetFileError(SEXP sModel)
{
    prLSmodel     prModel;
    pLSmodel      pModel;

    int           *pnErrorCode;
    SEXP          spnErrorCode = R_NilValue;
    char          pachLinetxt[256];
    SEXP          spachLinetxt = R_NilValue;
    int           *pnLinenum;
    SEXP          spnLinenum = R_NilValue;
    SEXP          rList = R_NilValue;
    char          *Names[3] = {"ErrorCode", "pnLinenum", "pachLinetxt"};
    SEXP          ListNames = R_NilValue;
    int           nNumItems = 3;
    int           nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnLinenum = NEW_INTEGER(1));
    nProtect += 1;
    pnLinenum = INTEGER_POINTER(spnLinenum);

    //description item
    PROTECT(spachLinetxt = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetFileError(pModel,pnLinenum,pachLinetxt);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachLinetxt,0,mkChar(pachLinetxt));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnLinenum);
        SET_VECTOR_ELT(rList, 2, spachLinetxt);
    }

    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSgetErrorRowIndex(SEXP sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piRow;
    SEXP      spiRow = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "piRow"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiRow = NEW_INTEGER(1));
    nProtect += 1;
    piRow = INTEGER_POINTER(spiRow);

    *pnErrorCode = LSgetErrorRowIndex(pModel,piRow);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiRow);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/***********************************************************
 * Routines for Setting and Retrieving Parameter Values(17)*
 ***********************************************************/
SEXP rcLSsetModelDouParameter(SEXP sModel,
                              SEXP snParameter,
                              SEXP sdValue)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);
    double    dValue = Rf_asReal(sdValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelDouParameter(pModel,nParameter,dValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelDouParameter(SEXP sModel,
                              SEXP snParameter)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValue;
    SEXP      spdValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pdValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdValue = NEW_NUMERIC(1));
    nProtect += 1;
    pdValue = NUMERIC_POINTER(spdValue);

    *pnErrorCode = LSgetModelDouParameter(pModel,nParameter,pdValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelIntParameter(SEXP sModel,
                              SEXP snParameter,
                              SEXP snValue)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);
    int       nValue = Rf_asInteger(snValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelIntParameter(pModel,nParameter,nValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelIntParameter(SEXP sModel,
                              SEXP snParameter)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnValue;
    SEXP      spnValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnValue = NEW_INTEGER(1));
    nProtect += 1;
    pnValue = INTEGER_POINTER(spnValue);

    *pnErrorCode = LSgetModelIntParameter(pModel,nParameter,pnValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetEnvDouParameter(SEXP sEnv,
                            SEXP snParameter,
                            SEXP sdValue)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParameter = Rf_asInteger(snParameter);
    double    dValue = Rf_asReal(sdValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSsetEnvDouParameter(pEnv,nParameter,dValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetEnvDouParameter(SEXP sEnv,
                            SEXP snParameter)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValue;
    SEXP      spdValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pdValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spdValue = NEW_NUMERIC(1));
    nProtect += 1;
    pdValue = NUMERIC_POINTER(spdValue);

    *pnErrorCode = LSgetEnvDouParameter(pEnv,nParameter,pdValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetEnvIntParameter(SEXP sEnv,
                            SEXP snParameter,
                            SEXP snValue)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParameter = Rf_asInteger(snParameter);
    int       nValue = Rf_asInteger(snValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSsetEnvIntParameter(pEnv,nParameter,nValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetEnvIntParameter(SEXP sEnv,
                            SEXP snParameter)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnValue;
    SEXP      spnValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spnValue = NEW_INTEGER(1));
    nProtect += 1;
    pnValue = INTEGER_POINTER(spnValue);

    *pnErrorCode = LSgetEnvIntParameter(pEnv,nParameter,pnValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadModelParameter(SEXP sModel,
                            SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadModelParameter(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadEnvParameter(SEXP sEnv,
                          SEXP spszFname)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSreadEnvParameter(pEnv,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteModelParameter(SEXP sModel,
                             SEXP spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteModelParameter(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetIntParameterRange(SEXP sModel,
                              SEXP snParameter)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnValMIN;
    SEXP      spnValMIN = R_NilValue;
    int       *pnValMAX;
    SEXP      spnValMAX = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "pnValMIN", "pnValMAX"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnValMIN = NEW_INTEGER(1));
    nProtect += 1;
    pnValMIN = INTEGER_POINTER(spnValMIN);

    PROTECT(spnValMAX = NEW_INTEGER(1));
    nProtect += 1;
    pnValMAX = INTEGER_POINTER(spnValMAX);

    *pnErrorCode = LSgetIntParameterRange(pModel,nParameter,pnValMIN,pnValMAX);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnValMIN);
        SET_VECTOR_ELT(rList, 2, spnValMAX);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDouParameterRange(SEXP sModel,
                              SEXP snParameter)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nParameter = Rf_asInteger(snParameter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValMIN;
    SEXP      spdValMIN = R_NilValue;
    double    *pdValMAX;
    SEXP      spdValMAX = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "pdValMIN", "pdValMAX"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdValMIN = NEW_NUMERIC(1));
    nProtect += 1;
    pdValMIN = NUMERIC_POINTER(spdValMIN);

    PROTECT(spdValMAX = NEW_NUMERIC(1));
    nProtect += 1;
    pdValMAX = NUMERIC_POINTER(spdValMAX);

    *pnErrorCode = LSgetDouParameterRange(pModel,nParameter,pdValMIN,pdValMAX);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValMIN);
        SET_VECTOR_ELT(rList, 2, spdValMAX);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetParamShortDesc(SEXP sEnv,
                           SEXP snParam)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      szDescription[256];
    SEXP      sszDescription = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "szDescription"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    //description item
    PROTECT(sszDescription = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetParamShortDesc(pEnv,nParam,szDescription);

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        strcpy(szDescription,"");
    }

    CHECK_ERRCODE;

    SET_STRING_ELT(sszDescription,0,mkChar(szDescription));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, sszDescription);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetParamLongDesc(SEXP sEnv,
                          SEXP snParam)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      szDescription[1024];
    SEXP      sszDescription = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "szDescription"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    //description item
    PROTECT(sszDescription = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetParamLongDesc(pEnv,nParam,szDescription);
    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        strcpy(szDescription,"");
    }

    CHECK_ERRCODE;

    SET_STRING_ELT(sszDescription,0,mkChar(szDescription));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, sszDescription);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetParamMacroName(SEXP sEnv,
                           SEXP snParam)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      szParam[256];
    SEXP      sszParam = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "szParam"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    //description item
    PROTECT(sszParam = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetParamMacroName(pEnv,nParam,szParam);
    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        strcpy(szParam,"");
    }

    CHECK_ERRCODE;

    SET_STRING_ELT(sszParam,0,mkChar(szParam));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, sszParam);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetParamMacroID(SEXP sEnv,
                         SEXP sszParam)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    char      *szParam = (char *) CHAR(STRING_ELT(sszParam,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnParamType;
    SEXP      spnParamType = R_NilValue;
    int       *pnParam;
    SEXP      spnParam = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "pnParamType", "pnParam"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spnParamType = NEW_INTEGER(1));
    nProtect += 1;
    pnParamType = INTEGER_POINTER(spnParamType);

    PROTECT(spnParam = NEW_INTEGER(1));
    nProtect += 1;
    pnParam = INTEGER_POINTER(spnParam);

    *pnErrorCode = LSgetParamMacroID(pEnv,szParam,pnParamType,pnParam);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnParamType);
        SET_VECTOR_ELT(rList, 2, spnParam);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetQCEigs(SEXP sModel,
                   SEXP siRow,
                   SEXP spachWhich,
                   SEXP snEigval,
                   SEXP snCV,
                   SEXP sdTol,
                   SEXP snMaxIter)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       iRow = Rf_asInteger(siRow);
    char      *pachWhich = (char *) CHAR(STRING_ELT(spachWhich,0));
    int       nEigval = Rf_asInteger(snEigval);
    int       nCV = Rf_asInteger(snCV);
    double    dTol = Rf_asReal(sdTol);
    int       nMaxIter = Rf_asInteger(snMaxIter);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      spadEigVal = R_NilValue;
    double    *padEigVal;
    SEXP      spaEigVect = R_NilValue;
    double    **paEigVect;
    SEXP      spadEigVect = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","padEigVal","paEigVec"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadEigVal = NEW_NUMERIC(nEigval));
    nProtect += 1;
    padEigVal = NUMERIC_POINTER(spadEigVal);

    PROTECT(spaEigVect = allocVector(VECSXP, nEigval));
    nProtect += 1;

    for(nIdx = 0; nIdx < nEigval; nIdx++)
    {
        PROTECT(spadEigVect = NEW_NUMERIC(nVars));
        nProtect += 1;
        SET_VECTOR_ELT(spaEigVect, nIdx, spadEigVect);
    }

    paEigVect = R_Calloc(nEigval, double *);

    for(nIdx = 0; nIdx < nEigval; nIdx++)
    {
        paEigVect[nIdx] = (double *) REAL(VECTOR_ELT(spaEigVect, nIdx));
    }

    *pnErrorCode = LSgetQCEigs(pModel,iRow,pachWhich,padEigVal,paEigVect,
                               nEigval,nCV,dTol,nMaxIter);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadEigVal);
        SET_VECTOR_ELT(rList, 2, spaEigVect);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}


/********************************************************
* Model Loading Routines (19)                           *
*********************************************************/
SEXP rcLSloadLPData(SEXP      sModel,
                    SEXP      snCons,
                    SEXP      snVars,
                    SEXP      snObjSense,
                    SEXP      sdObjConst,
                    SEXP      spadC,
                    SEXP      spadB,
                    SEXP      spszConTypes,
                    SEXP      snAnnz,
                    SEXP      spaiAcols,
                    SEXP      spanAcols,
                    SEXP      spadAcoef,
                    SEXP      spaiArows,
                    SEXP      spadL,
                    SEXP      spadU)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       nCons = Rf_asInteger(snCons);
    int       nVars = Rf_asInteger(snVars);
    int       nObjSense = Rf_asInteger(snObjSense);
    double    dObjConst = Rf_asReal(sdObjConst);
    double    *padC = REAL(spadC);
    double    *padB = REAL(spadB);
    char      *pszConTypes = (char *) CHAR(STRING_ELT(spszConTypes,0));
    int       nAnnz = Rf_asInteger(snAnnz);
    int       *paiAcols = INTEGER(spaiAcols);
    int       *panAcols;
    double    *padAcoef = REAL(spadAcoef);
    int       *paiArows = INTEGER(spaiArows);
    double    *padL;
    double    *padU;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_INT_ARRAY(panAcols,spanAcols);
    MAKE_REAL_ARRAY(padL,spadL);
    MAKE_REAL_ARRAY(padU,spadU);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadLPData(pModel,
                                nCons,
                                nVars,
                                nObjSense,
                                dObjConst,
                                padC,
                                padB,
                                pszConTypes,
                                nAnnz,
                                paiAcols,
                                panAcols,
                                padAcoef,
                                paiArows,
                                padL,
                                padU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadQCData(SEXP      sModel,
                    SEXP      snQCnnz,
                    SEXP      spaiQCrows,
                    SEXP      spaiQCcols1,
                    SEXP      spaiQCcols2,
                    SEXP      spadQCcoef)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQCnnz = Rf_asInteger(snQCnnz);
    int       *paiQCrows = INTEGER(spaiQCrows);
    int       *paiQCcols1 = INTEGER(spaiQCcols1);
    int       *paiQCcols2 = INTEGER(spaiQCcols2);
    double    *padQCcoef = REAL(spadQCcoef);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadQCData(pModel,
                                nQCnnz,
                                paiQCrows,
                                paiQCcols1,
                                paiQCcols2,
                                padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadConeData(SEXP      sModel,
                      SEXP      snCone,
                      SEXP      spszConeTypes,
                      SEXP      spadConeAlpha, // newarg: alpha values of power cones
                      SEXP      spaiConebegcone,
                      SEXP      spaiConecols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCone = Rf_asInteger(snCone);
    char      *pszConeTypes = (char *) CHAR(STRING_ELT(spszConeTypes,0));
    int       *paiConebegcone = INTEGER(spaiConebegcone);
    int       *paiConecols = INTEGER(spaiConecols);
    double    *padConeAlpha= REAL(spadConeAlpha);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadConeData(pModel,
                                  nCone,
                                  pszConeTypes,
                                  padConeAlpha,
                                  paiConebegcone,
                                  paiConecols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadPOSDData(SEXP      sModel,
                      SEXP      snPOSD,
                      SEXP      spaiPOSDdim,
                      SEXP      spaiPOSDbeg,
                      SEXP      spaiPOSDrowndx,
                      SEXP      spaiPOSDcolndx,
                      SEXP      spaiPOSDvarndx)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nPOSD = Rf_asInteger(snPOSD);
    int       *paiPOSDdim = INTEGER(spaiPOSDdim);
    int       *paiPOSDbeg = INTEGER(spaiPOSDbeg);
    int       *paiPOSDrowndx = INTEGER(spaiPOSDrowndx);
    int       *paiPOSDcolndx = INTEGER(spaiPOSDcolndx);
    int       *paiPOSDvarndx = INTEGER(spaiPOSDvarndx);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadPOSDData(pModel,
                                  nPOSD,
                                  paiPOSDdim,
                                  paiPOSDbeg,
                                  paiPOSDrowndx,
                                  paiPOSDcolndx,
                                  paiPOSDvarndx);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadSETSData(SEXP      sModel,
                      SEXP      snSETS,
                      SEXP      spszSETStype,
                      SEXP      spaiCARDnum,
                      SEXP      spaiSETSbegcol,
                      SEXP      spaiSETScols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSETS = Rf_asInteger(snSETS);
    char      *pszSETStype = (char *) CHAR(STRING_ELT(spszSETStype,0));
    int       *paiCARDnum = INTEGER(spaiCARDnum);
    int       *paiSETSbegcol = INTEGER(spaiSETSbegcol);
    int       *paiSETScols = INTEGER(spaiSETScols);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadSETSData(pModel,
                                  nSETS,
                                  pszSETStype,
                                  paiCARDnum,
                                  paiSETSbegcol,
                                  paiSETScols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadSemiContData(SEXP      sModel,
                          SEXP      snSCVars,
                          SEXP      spaiVars,
                          SEXP      spadL,
                          SEXP      spadU)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSCVars = Rf_asInteger(snSCVars);
    int       *paiVars = INTEGER(spaiVars);
    double    *padL = REAL(spadL);
    double    *padU = REAL(spadU);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadSemiContData(pModel,
                                      nSCVars,
                                      paiVars,
                                      padL,
                                      padU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadVarType(SEXP      sModel,
                     SEXP      spszVarTypes)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszVarTypes = (char *) CHAR(STRING_ELT(spszVarTypes,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadVarType(pModel,
                                 pszVarTypes);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSloadNameData(SEXP      sModel,
                      SEXP      spszTitle,
                      SEXP      spszObjName,
                      SEXP      spszRhsName,
                      SEXP      spszRngName,
                      SEXP      spszBndname,
                      SEXP      spaszConNames,
                      SEXP      spaszVarNames,
                      SEXP      spaszConeNames)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszTitle = NULL;
    char      *pszObjName = NULL;
    char      *pszRhsName = NULL;
    char      *pszRngName = NULL;
    char      *pszBndname = NULL;
    char      **paszConNames = NULL;
    char      **paszVarNames = NULL;
    char      **paszConeNames = NULL;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_ARRAY(pszTitle,spszTitle);
    MAKE_CHAR_ARRAY(pszObjName,spszObjName);
    MAKE_CHAR_ARRAY(pszRhsName,spszRhsName);
    MAKE_CHAR_ARRAY(pszRngName,spszRngName);
    MAKE_CHAR_ARRAY(pszBndname,spszBndname);
    MAKE_CHAR_CHAR_ARRAY(paszConNames,spaszConNames);
    MAKE_CHAR_CHAR_ARRAY(paszVarNames,spaszVarNames);
    MAKE_CHAR_CHAR_ARRAY(paszConeNames,spaszConeNames);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadNameData(pModel,
                                  pszTitle,
                                  pszObjName,
                                  pszRhsName,
                                  pszRngName,
                                  pszBndname,
                                  paszConNames,
                                  paszVarNames,
                                  paszConeNames);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSloadNLPData(SEXP      sModel,
                     SEXP      spaiNLPcols,
                     SEXP      spanNLPcols,
                     SEXP      spadNLPcoef,
                     SEXP      spaiNLProws,
                     SEXP      snNLPobj,
                     SEXP      spaiNLPobj,
                     SEXP      spadNLPobj)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *paiNLPcols = INTEGER(spaiNLPcols);
    int       *panNLPcols = INTEGER(spanNLPcols);
    double    *padNLPcoef = NULL;
    int       *paiNLProws = INTEGER(spaiNLProws);
    int       nNLPobj = Rf_asInteger(snNLPobj);
    int       *paiNLPobj = INTEGER(spaiNLPobj);
    double    *padNLPobj = NULL;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_REAL_ARRAY(padNLPcoef,spadNLPcoef);
    MAKE_REAL_ARRAY(padNLPobj,spadNLPobj);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadNLPData(pModel,
                                 paiNLPcols,
                                 panNLPcols,
                                 padNLPcoef,
                                 paiNLProws,
                                 nNLPobj,
                                 paiNLPobj,
                                 padNLPobj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSloadInstruct(SEXP      sModel,
                      SEXP      snCons,
                      SEXP      snObjs,
                      SEXP      snVars,
                      SEXP      snNumbers,
                      SEXP      spanObjSense,
                      SEXP      spszConType,
                      SEXP      spszVarType,
                      SEXP      spanInstruct,
                      SEXP      snInstruct,
                      SEXP      spaiVars,
                      SEXP      spadNumVal,
                      SEXP      spadVarVal,
                      SEXP      spaiObjBeg,
                      SEXP      spanObjLen,
                      SEXP      spaiConBeg,
                      SEXP      spanConLen,
                      SEXP      spadLB,
                      SEXP      spadUB)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCons = Rf_asInteger(snCons);
    int       nObjs = Rf_asInteger(snObjs);
    int       nVars = Rf_asInteger(snVars);
    int       nNumbers = Rf_asInteger(snNumbers);
    int       *panObjSense = INTEGER(spanObjSense);
    char      *pszConType = (char *) CHAR(STRING_ELT(spszConType,0));
    char      *pszVarType = NULL;
    int       *panInstruct = INTEGER(spanInstruct);
    int       nInstruct = Rf_asInteger(snInstruct);
    int       *paiVars = NULL;
    double    *padNumVal = REAL(spadNumVal);
    double    *padVarVal = REAL(spadVarVal);
    int       *paiObjBeg = INTEGER(spaiObjBeg);
    int       *panObjLen = INTEGER(spanObjLen);
    int       *paiConBeg = INTEGER(spaiConBeg);
    int       *panConLen = INTEGER(spanConLen);
    double    *padLB = NULL;
    double    *padUB = NULL;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_ARRAY(pszVarType,spszVarType);
    MAKE_INT_ARRAY(paiVars,spaiVars);
    MAKE_REAL_ARRAY(padLB,spadLB);
    MAKE_REAL_ARRAY(padUB,spadUB);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadInstruct(pModel,
                                  nCons,
                                  nObjs,
                                  nVars,
                                  nNumbers,
                                  panObjSense,
                                  pszConType,
                                  pszVarType,
                                  panInstruct,
                                  nInstruct,
                                  paiVars,
                                  padNumVal,
                                  padVarVal,
                                  paiObjBeg,
                                  panObjLen,
                                  paiConBeg,
                                  panConLen,
                                  padLB,
                                  padUB);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddInstruct(SEXP      sModel,
                     SEXP      snCons,
                     SEXP      snObjs,
                     SEXP      snVars,
                     SEXP      snNumbers,
                     SEXP      spanObjSense,
                     SEXP      spszConType,
                     SEXP      spszVarType,
                     SEXP      spanInstruct,
                     SEXP      snInstruct,
                     SEXP      spaiCons,
                     SEXP      spadNumVal,
                     SEXP      spadVarVal,
                     SEXP      spaiObjBeg,
                     SEXP      spanObjLen,
                     SEXP      spaiConBeg,
                     SEXP      spanConLen,
                     SEXP      spadLB,
                     SEXP      spadUB)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCons = Rf_asInteger(snCons);
    int       nObjs = Rf_asInteger(snObjs);
    int       nVars = Rf_asInteger(snVars);
    int       nNumbers = Rf_asInteger(snNumbers);
    int       *panObjSense = INTEGER(spanObjSense);
    char      *pszConType = (char *) CHAR(STRING_ELT(spszConType,0));
    char      *pszVarType = NULL;
    int       *panInstruct = INTEGER(spanInstruct);
    int       nInstruct = Rf_asInteger(snInstruct);
    int       *paiCons = NULL;
    double    *padNumVal = REAL(spadNumVal);
    double    *padVarVal = REAL(spadVarVal);
    int       *paiObjBeg = INTEGER(spaiObjBeg);
    int       *panObjLen = INTEGER(spanObjLen);
    int       *paiConBeg = INTEGER(spaiConBeg);
    int       *panConLen = INTEGER(spanConLen);
    double    *padLB = NULL;
    double    *padUB = NULL;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_ARRAY(pszVarType,spszVarType);
    MAKE_INT_ARRAY(paiCons,spaiCons);
    MAKE_REAL_ARRAY(padLB,spadLB);
    MAKE_REAL_ARRAY(padUB,spadUB);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddInstruct(pModel,
                                 nCons,
                                 nObjs,
                                 nVars,
                                 nNumbers,
                                 panObjSense,
                                 pszConType,
                                 pszVarType,
                                 panInstruct,
                                 nInstruct,
                                 paiCons,
                                 padNumVal,
                                 padVarVal,
                                 paiObjBeg,
                                 panObjLen,
                                 paiConBeg,
                                 panConLen,
                                 padLB,
                                 padUB);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadStringData(SEXP      sModel,
                        SEXP      snStrings,
                        SEXP      spaszStringData)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nStrings = Rf_asInteger(snStrings);
    char      **paszStringData = NULL;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_CHAR_ARRAY(paszStringData,spaszStringData);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadStringData(pModel,
                                    nStrings,
                                    paszStringData);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadString(SEXP      sModel,
                    SEXP      spszString)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszString = (char *) CHAR(STRING_ELT(spszString,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadString(pModel,
                                pszString);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteStringData(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteStringData(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteString(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteString(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStringValue(SEXP      sModel,
                        SEXP      siString)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iString = Rf_asInteger(siString);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValue;
    SEXP      spdValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pdValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdValue = NEW_NUMERIC(1));
    nProtect += 1;
    pdValue = NUMERIC_POINTER(spdValue);

    *pnErrorCode = LSgetStringValue(pModel,iString,pdValue);

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        *pdValue = 0;
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConstraintProperty(SEXP      sModel,
                               SEXP      sndxCons)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       ndxCons = Rf_asInteger(sndxCons);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnConptype;
    SEXP      spnConptype = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnConptype"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnConptype = NEW_NUMERIC(1));
    nProtect += 1;
    pnConptype = INTEGER_POINTER(spnConptype);

    *pnErrorCode = LSgetConstraintProperty(pModel,ndxCons,pnConptype);

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        *pnConptype = 0;
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnConptype);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetConstraintProperty(SEXP      sModel,
                               SEXP      sndxCons,
                               SEXP      snConptype)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       ndxCons = Rf_asInteger(sndxCons);
    int       nConptype = Rf_asInteger(snConptype);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetConstraintProperty(pModel,ndxCons,nConptype);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSloadMultiStartSolution(SEXP      sModel,
                                SEXP      snIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadMultiStartSolution(pModel,nIndex);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadGASolution(SEXP      sModel,
                        SEXP      snIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadGASolution(pModel,nIndex);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddQCShift(SEXP      sModel,
                    SEXP      siRow,
                    SEXP      sdShift)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iRow = Rf_asInteger(siRow);
    double    dShift = Rf_asReal(sdShift);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddQCShift(pModel,iRow,dShift);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetQCShift(SEXP      sModel,
                    SEXP      siRow)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iRow = Rf_asInteger(siRow);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdShift;
    SEXP      spdShift = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pdShift"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdShift = NEW_NUMERIC(1));
    nProtect += 1;
    pdShift = NUMERIC_POINTER(spdShift);

    *pnErrorCode = LSgetQCShift(pModel,iRow,pdShift);

ErrorReturn:
   //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdShift);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSresetQCShift(SEXP      sModel,
                      SEXP      siRow)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iRow = Rf_asInteger(siRow);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSresetQCShift(pModel,iRow);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Solver Initialization Routines (9)                    *
*********************************************************/
SEXP rcLSloadBasis(SEXP      sModel,
                   SEXP      spanCstatus,
                   SEXP      spanRstatus)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panCstatus = INTEGER(spanCstatus);
    int       *panRstatus = INTEGER(spanRstatus);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadBasis(pModel,panCstatus,panRstatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadVarPriorities(SEXP      sModel,
                           SEXP      spanCprior)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panCprior = INTEGER(spanCprior);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadVarPriorities(pModel,panCprior);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadVarPriorities(SEXP      sModel,
                           SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadVarPriorities(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadVarStartPoint(SEXP      sModel,
                           SEXP      spadPrimal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    double    *padPrimal = REAL(spadPrimal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadVarStartPoint(pModel,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadVarStartPointPartial(SEXP      sModel,
                                  SEXP      snCols,
                                  SEXP      spaiCols,
                                  SEXP      spadPrimal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCols = Rf_asInteger(snCols);
    int       *paiCols = INTEGER(spaiCols);
    double    *padPrimal = REAL(spadPrimal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadVarStartPointPartial(pModel,nCols,paiCols,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadMIPVarStartPoint(SEXP      sModel,
                              SEXP      spadPrimal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    double    *padPrimal = REAL(spadPrimal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadMIPVarStartPoint(pModel,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadMIPVarStartPointPartial(SEXP      sModel,
                                     SEXP      snCols,
                                     SEXP      spaiCols,
                                     SEXP      spaiPrimal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCols = Rf_asInteger(snCols);
    int       *paiCols = INTEGER(spaiCols);
    int       *paiPrimal = INTEGER(spaiPrimal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadMIPVarStartPointPartial(pModel,nCols,paiCols,paiPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadVarStartPoint(SEXP      sModel,
                           SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadVarStartPoint(pModel,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSloadBlockStructure(SEXP      sModel,
                            SEXP      snBlock,
                            SEXP      spanRblock,
                            SEXP      spanCblock,
                            SEXP      snType)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nBlock = Rf_asInteger(snBlock);
    int       *panRblock = INTEGER(spanRblock);
    int       *panCblock = INTEGER(spanCblock);
    int       nType = Rf_asInteger(snType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadBlockStructure(pModel,
                                        nBlock,
                                        panRblock,
                                        panCblock,
                                        nType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

/********************************************************
 * Optimization Routines (6)                            *
 ********************************************************/
SEXP rcLSoptimize(SEXP  sModel,
                  SEXP  snMethod)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       nMethod = Rf_asInteger(snMethod);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSoptimize(pModel,nMethod,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsolveMIP(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;
#if KILL
    *pnErrorCode = LSsetMIPCallback(pModel,rMipCallBack,NULL);
    CHECK_ERRCODE;
#endif
    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveMIP(pModel,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsolveGOP(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveGOP(pModel,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSoptimizeQP(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSoptimizeQP(pModel,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLScheckConvexity(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LScheckConvexity(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsolveSBD(SEXP  sModel,
                  SEXP  snStages,
                  SEXP  spanRowStage,
                  SEXP  spanColStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nStages = Rf_asInteger(snStages);
    int       *panRowStage = INTEGER(spanRowStage);
    int       *panColStage = INTEGER(spanColStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveSBD(pModel,
                              nStages,
                              panRowStage,
                              panColStage,
                              pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Solution Query Routines (15)                          *
*********************************************************/
SEXP rcLSgetIInfo(SEXP  sModel,
                  SEXP  snQuery)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSgetInfo(pModel,nQuery,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDInfo(SEXP  sModel,
                  SEXP  snQuery)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetInfo(pModel,nQuery,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetProfilerInfo(SEXP  sModel,
                         SEXP  smContext)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       mContext = Rf_asInteger(smContext);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnCalls;
    SEXP      spnCalls = R_NilValue;
    double    *pdElapsedTime;
    SEXP      spdElapsedTime = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "pnCalls", "pdElapsedTime"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnCalls = NEW_INTEGER(1));
    nProtect += 1;
    pnCalls = INTEGER_POINTER(spnCalls);

    PROTECT(spdElapsedTime = NEW_NUMERIC(1));
    nProtect += 1;
    pdElapsedTime = NUMERIC_POINTER(spdElapsedTime);

    *pnErrorCode = LSgetProfilerInfo(pModel,mContext,
                                     pnCalls,pdElapsedTime);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnCalls);
        SET_VECTOR_ELT(rList, 2, spdElapsedTime);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetProfilerContext(SEXP  sModel,
                            SEXP  smContext)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       mContext = Rf_asInteger(smContext);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *szDescription;
    SEXP      sszDescription = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "szDescription"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    szDescription = LSgetProfilerContext(pModel,mContext);

    PROTECT(sszDescription = NEW_CHARACTER(1));
    nProtect += 1;

    SET_STRING_ELT(sszDescription,0,mkChar(szDescription));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, sszDescription);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetPrimalSolution(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padPrimal;
    SEXP      spadPrimal = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadPrimal = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimal = NUMERIC_POINTER(spadPrimal);

    *pnErrorCode = LSgetPrimalSolution(pModel,padPrimal);


ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadPrimal);
    }
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDualSolution(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padDual;
    SEXP      spadDual = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padDual"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadDual = NEW_NUMERIC(nCons));
    nProtect += 1;
    padDual = NUMERIC_POINTER(spadDual);

    *pnErrorCode = LSgetDualSolution(pModel,padDual);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadDual);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetReducedCosts(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padReducedCost;
    SEXP      spadReducedCost = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padReducedCost"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadReducedCost = NEW_NUMERIC(nVars));
    nProtect += 1;
    padReducedCost = NUMERIC_POINTER(spadReducedCost);

    *pnErrorCode = LSgetReducedCosts(pModel,padReducedCost);


ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadReducedCost);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetReducedCostsCone(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padReducedCost;
    SEXP      spadReducedCost = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padReducedCost"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadReducedCost = NEW_NUMERIC(nVars));
    nProtect += 1;
    padReducedCost = NUMERIC_POINTER(spadReducedCost);

    *pnErrorCode = LSgetReducedCostsCone(pModel,padReducedCost);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadReducedCost);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSlacks(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padSlack;
    SEXP      spadSlack = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padSlack"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadSlack = NEW_NUMERIC(nCons));
    nProtect += 1;
    padSlack = NUMERIC_POINTER(spadSlack);

    *pnErrorCode = LSgetDualSolution(pModel,padSlack);


ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadSlack);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetBasis(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panCstatus;
    SEXP      spanCstatus = R_NilValue;
    int       *panRstatus;
    SEXP      spanRstatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "panCstatus", "panRstatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nCons, nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spanCstatus = NEW_INTEGER(nVars));
    nProtect += 1;
    panCstatus = INTEGER_POINTER(spanCstatus);

    PROTECT(spanRstatus = NEW_INTEGER(nCons));
    nProtect += 1;
    panRstatus = INTEGER_POINTER(spanRstatus);

    *pnErrorCode = LSgetBasis(pModel,panCstatus,panRstatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanCstatus);
        SET_VECTOR_ELT(rList, 2, spanRstatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSolution(SEXP  sModel,
                     SEXP  snWhich)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nWhich = Rf_asInteger(snWhich);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padResult;
    SEXP      spadResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadResult = NEW_NUMERIC(nVars));
    nProtect += 1;
    padResult = NUMERIC_POINTER(spadResult);

    *pnErrorCode = LSgetSolution(pModel,nWhich,padResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPPrimalSolution(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padPrimal;
    SEXP      spadPrimal = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadPrimal = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimal = NUMERIC_POINTER(spadPrimal);

    *pnErrorCode = LSgetMIPPrimalSolution(pModel,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadPrimal);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPDualSolution(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padDual;
    SEXP      spadDual = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padDual"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadDual = NEW_NUMERIC(nCons));
    nProtect += 1;
    padDual = NUMERIC_POINTER(spadDual);

    *pnErrorCode = LSgetMIPDualSolution(pModel,padDual);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadDual);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPReducedCosts(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padReducedCost;
    SEXP      spadReducedCost = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padReducedCost"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadReducedCost = NEW_NUMERIC(nVars));
    nProtect += 1;
    padReducedCost = NUMERIC_POINTER(spadReducedCost);

    *pnErrorCode = LSgetMIPReducedCosts(pModel,padReducedCost);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadReducedCost);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPSlacks(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padSlack;
    SEXP      spadSlack = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padSlack"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadSlack = NEW_NUMERIC(nCons));
    nProtect += 1;
    padSlack = NUMERIC_POINTER(spadSlack);

    *pnErrorCode = LSgetMIPSlacks(pModel,padSlack);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadSlack);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPBasis(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panCstatus;
    SEXP      spanCstatus = R_NilValue;
    int       *panRstatus;
    SEXP      spanRstatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "panCstatus", "panRstatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nCons, nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spanCstatus = NEW_INTEGER(nVars));
    nProtect += 1;
    panCstatus = INTEGER_POINTER(spanCstatus);

    PROTECT(spanRstatus = NEW_INTEGER(nCons));
    nProtect += 1;
    panRstatus = INTEGER_POINTER(spanRstatus);

    *pnErrorCode = LSgetMIPBasis(pModel,panCstatus,panRstatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanCstatus);
        SET_VECTOR_ELT(rList, 2, spanRstatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNextBestMIPSol(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSgetNextBestMIPSol(pModel,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Model Query Routines    (44)                          *
*********************************************************/
SEXP rcLSgetLPData(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnObjSense;
    SEXP      spnObjSense = R_NilValue;
    double    *pdObjConst;
    SEXP      spdObjConst = R_NilValue;
    double    *padC;
    SEXP      spadC = R_NilValue;
    double    *padB;
    SEXP      spadB = R_NilValue;
    char      *pachConTypes = NULL;
    SEXP      spachConTypes = R_NilValue;
    int       *paiAcols;
    SEXP      spaiAcols = R_NilValue;
    int       *panAcols;
    SEXP      spanAcols = R_NilValue;
    double    *padAcoef;
    SEXP      spadAcoef = R_NilValue;
    int       *paiArows;
    SEXP      spaiArows = R_NilValue;
    double    *padL;
    SEXP      spadL = R_NilValue;
    double    *padU;
    SEXP      spadU = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[12] = {"ErrorCode", "pnObjSense","pdObjConst","padC","padB",
                            "pachConTypes", "paiAcols", "panAcols", "padAcoef",
                            "paiArows", "padL", "padU"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 11;
    int       nIdx, nProtect = 0;
    int       nCons, nVars, nNnz;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_NONZ,&nNnz);
    CHECK_ERRCODE;

    PROTECT(spnObjSense = NEW_INTEGER(1));
    nProtect += 1;
    pnObjSense = INTEGER_POINTER(spnObjSense);

    PROTECT(spdObjConst = NEW_NUMERIC(1));
    nProtect += 1;
    pdObjConst = NUMERIC_POINTER(spdObjConst);

    PROTECT(spadC = NEW_NUMERIC(nVars));
    nProtect += 1;
    padC = NUMERIC_POINTER(spadC);

    PROTECT(spadB = NEW_NUMERIC(nCons));
    nProtect += 1;
    padB = NUMERIC_POINTER(spadB);

    PROTECT(spachConTypes = NEW_CHARACTER(1));
    nProtect += 1;
    pachConTypes = (char *)malloc(sizeof(char)*(nCons+1));
    pachConTypes[nCons] = '\0';

    PROTECT(spaiAcols = NEW_INTEGER(nVars));
    nProtect += 1;
    paiAcols = INTEGER_POINTER(spaiAcols);

    PROTECT(spanAcols = NEW_INTEGER(nVars));
    nProtect += 1;
    panAcols = INTEGER_POINTER(spanAcols);

    PROTECT(spadAcoef = NEW_NUMERIC(nNnz));
    nProtect += 1;
    padAcoef = NUMERIC_POINTER(spadAcoef);

    PROTECT(spaiArows = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiArows = INTEGER_POINTER(spaiArows);

    PROTECT(spadL = NEW_NUMERIC(nVars));
    nProtect += 1;
    padL = NUMERIC_POINTER(spadL);

    PROTECT(spadU = NEW_NUMERIC(nVars));
    nProtect += 1;
    padU = NUMERIC_POINTER(spadU);

    *pnErrorCode = LSgetLPData(pModel,
                               pnObjSense,
                               pdObjConst,
                               padC,
                               padB,
                               pachConTypes,
                               paiAcols,
                               panAcols,
                               padAcoef,
                               paiArows,
                               padL,
                               padU);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachConTypes,0,mkChar(pachConTypes));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnObjSense);
        SET_VECTOR_ELT(rList, 2, spdObjConst);
        SET_VECTOR_ELT(rList, 3, spadC);
        SET_VECTOR_ELT(rList, 4, spadB);
        SET_VECTOR_ELT(rList, 5, spachConTypes);
        SET_VECTOR_ELT(rList, 6, spaiAcols);
        SET_VECTOR_ELT(rList, 6, spanAcols);
        SET_VECTOR_ELT(rList, 7, spadAcoef);
        SET_VECTOR_ELT(rList, 8, spaiArows);
        SET_VECTOR_ELT(rList, 9, spadL);
        SET_VECTOR_ELT(rList, 10, spadU);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetQCData(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *paiQCrows;
    SEXP      spaiQCrows = R_NilValue;
    int       *paiQCcols1;
    SEXP      spaiQCcols1 = R_NilValue;
    int       *paiQCcols2;
    SEXP      spaiQCcols2 = R_NilValue;
    double    *padQCcoef;
    SEXP      spadQCcoef = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "paiQCrows","paiQCcols1","paiQCcols2","padQCcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nNnz;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_NONZ,&nNnz);
    CHECK_ERRCODE;

    PROTECT(spaiQCrows = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiQCrows = INTEGER_POINTER(spaiQCrows);

    PROTECT(spaiQCcols1 = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiQCcols1 = INTEGER_POINTER(spaiQCcols1);

    PROTECT(spaiQCcols2 = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiQCcols2 = INTEGER_POINTER(spaiQCcols2);

    PROTECT(spadQCcoef = NEW_NUMERIC(nNnz));
    nProtect += 1;
    padQCcoef = NUMERIC_POINTER(spadQCcoef);

    *pnErrorCode = LSgetQCData(pModel,
                               paiQCrows,
                               paiQCcols1,
                               paiQCcols2,
                               padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spaiQCrows);
        SET_VECTOR_ELT(rList, 2, spaiQCcols1);
        SET_VECTOR_ELT(rList, 3, spaiQCcols2);
        SET_VECTOR_ELT(rList, 4, spadQCcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetQCDatai(SEXP  sModel,
                    SEXP  siCon)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCon = Rf_asInteger(siCon);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnQCnnz;
    SEXP      spnQCnnz = R_NilValue;
    int       *paiQCcols1;
    SEXP      spaiQCcols1 = R_NilValue;
    int       *paiQCcols2;
    SEXP      spaiQCcols2 = R_NilValue;
    double    *padQCcoef;
    SEXP      spadQCcoef = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "pnQCnnz","paiQCcols1","paiQCcols2","padQCcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nNnz;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_NONZ,&nNnz);
    CHECK_ERRCODE;

    PROTECT(spnQCnnz = NEW_INTEGER(1));
    nProtect += 1;
    pnQCnnz = INTEGER_POINTER(spnQCnnz);

    PROTECT(spaiQCcols1 = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiQCcols1 = INTEGER_POINTER(spaiQCcols1);

    PROTECT(spaiQCcols2 = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiQCcols2 = INTEGER_POINTER(spaiQCcols2);

    PROTECT(spadQCcoef = NEW_NUMERIC(nNnz));
    nProtect += 1;
    padQCcoef = NUMERIC_POINTER(spadQCcoef);

    *pnErrorCode = LSgetQCDatai(pModel,
                                iCon,
                                pnQCnnz,
                                paiQCcols1,
                                paiQCcols2,
                                padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnQCnnz);
        SET_VECTOR_ELT(rList, 2, spaiQCcols1);
        SET_VECTOR_ELT(rList, 3, spaiQCcols2);
        SET_VECTOR_ELT(rList, 4, spadQCcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVarType(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachVarTypes = NULL;
    SEXP      spachVarTypes = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pachVarTypes"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spachVarTypes = NEW_CHARACTER(1));
    nProtect += 1;
    pachVarTypes = (char *)malloc(sizeof(char)*(nVars+1));
    pachVarTypes[nVars] = '\0';

    *pnErrorCode = LSgetVarType(pModel,pachVarTypes);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachVarTypes,0,mkChar(pachVarTypes));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachVarTypes);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVarStartPoint(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padPrimal;
    SEXP      spadPrimal = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadPrimal = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimal = NUMERIC_POINTER(spadPrimal);

    *pnErrorCode = LSgetVarStartPoint(pModel,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadPrimal);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVarStartPointPartial(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnCols;
    SEXP      spnCols = R_NilValue;
    int       *paiCols;
    SEXP      spaiCols = R_NilValue;
    double    *padPrimal;
    SEXP      spadPrimal = R_NilValue;
    int       *paiCols2;
    SEXP      spaiCols2 = R_NilValue;
    double    *padPrimal2;
    SEXP      spadPrimal2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pnCols", "paiCols", "padPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spnCols = NEW_INTEGER(1));
    nProtect += 1;
    pnCols = INTEGER_POINTER(spnCols);

    PROTECT(spaiCols = NEW_INTEGER(nVars));
    nProtect += 1;
    paiCols = INTEGER_POINTER(spaiCols);

    PROTECT(spadPrimal = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimal = NUMERIC_POINTER(spadPrimal);

    *pnErrorCode = LSgetVarStartPointPartial(pModel,pnCols,paiCols,padPrimal);
    CHECK_ERRCODE;

    PROTECT(spaiCols2 = NEW_INTEGER(*pnCols));
    nProtect += 1;
    paiCols2 = INTEGER_POINTER(spaiCols2);
    memcpy(paiCols2,paiCols,sizeof(int)*(*pnCols));

    PROTECT(spadPrimal2 = NEW_NUMERIC(*pnCols));
    nProtect += 1;
    padPrimal2 = NUMERIC_POINTER(spadPrimal2);
    memcpy(padPrimal2,padPrimal,sizeof(double)*(*pnCols));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnCols);
        SET_VECTOR_ELT(rList, 2, spaiCols2);
        SET_VECTOR_ELT(rList, 3, spadPrimal2);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPVarStartPointPartial(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnCols;
    SEXP      spnCols = R_NilValue;
    int       *paiCols;
    SEXP      spaiCols = R_NilValue;
    int       *panPrimal;
    SEXP      spanPrimal = R_NilValue;
    int       *paiCols2;
    SEXP      spaiCols2 = R_NilValue;
    int       *panPrimal2;
    SEXP      spanPrimal2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pnCols", "paiCols", "panPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spnCols = NEW_INTEGER(1));
    nProtect += 1;
    pnCols = INTEGER_POINTER(spnCols);

    PROTECT(spaiCols = NEW_INTEGER(nVars));
    nProtect += 1;
    paiCols = INTEGER_POINTER(spaiCols);

    PROTECT(spanPrimal = NEW_INTEGER(nVars));
    nProtect += 1;
    panPrimal = INTEGER_POINTER(spanPrimal);

    *pnErrorCode = LSgetMIPVarStartPointPartial(pModel,pnCols,paiCols,panPrimal);
    CHECK_ERRCODE;

    PROTECT(spaiCols2 = NEW_INTEGER(*pnCols));
    nProtect += 1;
    paiCols2 = INTEGER_POINTER(spaiCols2);
    memcpy(paiCols2,paiCols,sizeof(int)*(*pnCols));

    PROTECT(spanPrimal2 = NEW_NUMERIC(*pnCols));
    nProtect += 1;
    panPrimal2 = INTEGER_POINTER(spanPrimal2);
    memcpy(panPrimal2,panPrimal,sizeof(int)*(*pnCols));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnCols);
        SET_VECTOR_ELT(rList, 2, spaiCols2);
        SET_VECTOR_ELT(rList, 3, spanPrimal2);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetMIPVarStartPoint(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padPrimal;
    SEXP      spadPrimal = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padPrimal"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadPrimal = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimal = NUMERIC_POINTER(spadPrimal);

    *pnErrorCode = LSgetMIPVarStartPoint(pModel,padPrimal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadPrimal);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSETSData(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piNsets;
    SEXP      spiNsets = R_NilValue;
    int       *piNtnz;
    SEXP      spiNtnz = R_NilValue;
    char      *pachSETtype, *pachSETtype2;
    SEXP      spachSETtype = R_NilValue;
    int       *piCardnum, *piCardnum2;
    SEXP      spiCardnum = R_NilValue, spiCardnum2 = R_NilValue;
    int       *piNnz, *piNnz2;
    SEXP      spiNnz = R_NilValue, spiNnz2 = R_NilValue;
    int       *piBegset, *piBegset2;
    SEXP      spiBegset = R_NilValue, spiBegset2 = R_NilValue;
    int       *piVarndx, *piVarndx2;
    SEXP      spiVarndx = R_NilValue, spiVarndx2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[8] = {"ErrorCode", "piNsets", "piNtnz", "pachSETtype",
                           "piCardnum", "piNnz", "piBegset", "piVarndx"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 8;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spiNsets = NEW_INTEGER(1));
    nProtect += 1;
    piNsets = INTEGER_POINTER(spiNsets);

    PROTECT(spiNtnz = NEW_INTEGER(1));
    nProtect += 1;
    piNtnz = INTEGER_POINTER(spiNtnz);

    PROTECT(spachSETtype = NEW_CHARACTER(1));
    nProtect += 1;
    pachSETtype = (char *)malloc(sizeof(char)*(nVars+1));

    PROTECT(spiCardnum = NEW_INTEGER(nVars));
    nProtect += 1;
    piCardnum = INTEGER_POINTER(spiCardnum);

    PROTECT(spiNnz = NEW_INTEGER(nVars));
    nProtect += 1;
    piNnz = INTEGER_POINTER(spiNnz);

    PROTECT(spiBegset = NEW_INTEGER(nVars));
    nProtect += 1;
    piBegset = INTEGER_POINTER(spiBegset);

    PROTECT(spiVarndx = NEW_INTEGER(nVars));
    nProtect += 1;
    piVarndx = INTEGER_POINTER(spiVarndx);

    *pnErrorCode = LSgetSETSData(pModel,
                                 piNsets,
                                 piNtnz,
                                 pachSETtype,
                                 piCardnum,
                                 piNnz,
                                 piBegset,
                                 piVarndx);
    CHECK_ERRCODE;

    pachSETtype2 = (char *)malloc(sizeof(char)*(*piNsets+1));
    memcpy(pachSETtype2,pachSETtype,sizeof(char)*(*piNsets));
    pachSETtype2[*piNsets] = '\0';
    SET_STRING_ELT(spachSETtype,0,mkChar(pachSETtype2));

    PROTECT(spiCardnum2 = NEW_INTEGER(*piNsets));
    nProtect += 1;
    piCardnum2 = INTEGER_POINTER(spiCardnum2);
    memcpy(piCardnum2,piCardnum,sizeof(int)*(*piNsets));

    PROTECT(spiNnz2 = NEW_INTEGER(*piNsets));
    nProtect += 1;
    piNnz2 = INTEGER_POINTER(spiNnz2);
    memcpy(piNnz2,piNnz,sizeof(int)*(*piNsets));

    PROTECT(spiBegset2 = NEW_INTEGER(*piNsets+1));
    nProtect += 1;
    piBegset2 = INTEGER_POINTER(spiBegset2);
    memcpy(piBegset2,piBegset,sizeof(int)*(*piNsets+1));

    PROTECT(spiVarndx2 = NEW_INTEGER(*piNtnz));
    nProtect += 1;
    piVarndx2 = INTEGER_POINTER(spiVarndx2);
    memcpy(piVarndx2,piVarndx,sizeof(int)*(*piNtnz));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiNsets);
        SET_VECTOR_ELT(rList, 2, spiNtnz);
        SET_VECTOR_ELT(rList, 3, spachSETtype);
        SET_VECTOR_ELT(rList, 4, spiCardnum2);
        SET_VECTOR_ELT(rList, 5, spiNnz2);
        SET_VECTOR_ELT(rList, 6, spiBegset2);
        SET_VECTOR_ELT(rList, 7, spiVarndx2);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSETSDatai(SEXP  sModel,
                      SEXP  siSet)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iSet = Rf_asInteger(siSet);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachSETtype;
    SEXP      spachSETtype = R_NilValue;
    int       *piCardnum;
    SEXP      spiCardnum = R_NilValue;
    int       *piNnz;
    SEXP      spiNnz = R_NilValue;
    int       *piVarndx, *piVarndx2;
    SEXP      spiVarndx = R_NilValue, spiVarndx2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "pachSETtype", "piCardnum", "piNnz", "piVarndx"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spachSETtype = NEW_CHARACTER(1));
    nProtect += 1;
    pachSETtype = (char *)malloc(sizeof(char)*(2));
    pachSETtype[1] = '\0';

    PROTECT(spiCardnum = NEW_INTEGER(1));
    nProtect += 1;
    piCardnum = INTEGER_POINTER(spiCardnum);

    PROTECT(spiNnz = NEW_INTEGER(1));
    nProtect += 1;
    piNnz = INTEGER_POINTER(spiNnz);

    PROTECT(spiVarndx = NEW_INTEGER(nVars));
    nProtect += 1;
    piVarndx = INTEGER_POINTER(spiVarndx);

    *pnErrorCode = LSgetSETSDatai(pModel,
                                  iSet,
                                  pachSETtype,
                                  piCardnum,
                                  piNnz,
                                  piVarndx);
    CHECK_ERRCODE;

    SET_STRING_ELT(spachSETtype,0,mkChar(pachSETtype));

    PROTECT(spiVarndx2 = NEW_INTEGER(*piNnz));
    nProtect += 1;
    piVarndx2 = INTEGER_POINTER(spiVarndx2);
    memcpy(piVarndx2,piVarndx,sizeof(int)*(*piNnz));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachSETtype);
        SET_VECTOR_ELT(rList, 2, spiCardnum);
        SET_VECTOR_ELT(rList, 3, spiNnz);
        SET_VECTOR_ELT(rList, 4, spiVarndx2);

    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSemiContData(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piNvars;
    SEXP      spiNvars = R_NilValue;
    int       *panVarndx, *panVarndx2;
    SEXP      spanVarndx = R_NilValue, spanVarndx2 = R_NilValue;
    double    *padL, *padL2, *padU, *padU2;
    SEXP      spadL = R_NilValue, spadL2 = R_NilValue;
    SEXP      spadU = R_NilValue, spadU2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "piNvars", "panVarndx", "padL", "padU"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spiNvars = NEW_INTEGER(1));
    nProtect += 1;
    piNvars = INTEGER_POINTER(spiNvars);

    PROTECT(spanVarndx = NEW_INTEGER(nVars));
    nProtect += 1;
    panVarndx = INTEGER_POINTER(spanVarndx);

    PROTECT(spadL = NEW_NUMERIC(nVars));
    nProtect += 1;
    padL = NUMERIC_POINTER(spadL);

    PROTECT(spadU = NEW_NUMERIC(nVars));
    nProtect += 1;
    padU = NUMERIC_POINTER(spadU);

    *pnErrorCode = LSgetSemiContData(pModel,
                                     piNvars,
                                     panVarndx,
                                     padL,
                                     padU);
    CHECK_ERRCODE;

    PROTECT(spanVarndx2 = NEW_INTEGER(*piNvars));
    nProtect += 1;
    panVarndx2 = INTEGER_POINTER(spanVarndx2);
    memcpy(panVarndx2,panVarndx,sizeof(int)*(*piNvars));

    PROTECT(spadL2 = NEW_NUMERIC(*piNvars));
    nProtect += 1;
    padL2 = NUMERIC_POINTER(spadL2);
    memcpy(padL2,padL,sizeof(double)*(*piNvars));

    PROTECT(spadU2 = NEW_NUMERIC(*piNvars));
    nProtect += 1;
    padU2 = NUMERIC_POINTER(spadU2);
    memcpy(padU2,padU,sizeof(double)*(*piNvars));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiNvars);
        SET_VECTOR_ELT(rList, 2, spanVarndx2);
        SET_VECTOR_ELT(rList, 3, spadL2);
        SET_VECTOR_ELT(rList, 4, spadU2);

    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetLPVariableDataj(SEXP  sModel,
                            SEXP  siVar)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar = Rf_asInteger(siVar);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pchVartype;
    SEXP      spchVartype = R_NilValue;
    double    *pdC;
    SEXP      spdC = R_NilValue;
    double    *pdL;
    SEXP      spdL = R_NilValue;
    double    *pdU;
    SEXP      spdU = R_NilValue;
    int       *pnAnnz;
    SEXP      spnAnnz = R_NilValue;
    int       *paiArows, *paiArows2;
    SEXP      spaiArows = R_NilValue, spaiArows2 = R_NilValue;
    double    *padAcoef, *padAcoef2;
    SEXP      spadAcoef = R_NilValue, spadAcoef2 = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[8] = {"ErrorCode", "pchVartype", "pdC", "pdL", "pdU",
                           "pnAnnz","paiArows", "padAcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 8;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spchVartype = NEW_CHARACTER(1));
    nProtect += 1;
    pchVartype = (char *)malloc(sizeof(char)*(2));
    pchVartype[1] = '\0';

    PROTECT(spdC = NEW_NUMERIC(1));
    nProtect += 1;
    pdC = NUMERIC_POINTER(spdC);

    PROTECT(spdL = NEW_NUMERIC(1));
    nProtect += 1;
    pdL = NUMERIC_POINTER(spdL);

    PROTECT(spdU = NEW_NUMERIC(1));
    nProtect += 1;
    pdU = NUMERIC_POINTER(spdU);

    PROTECT(spnAnnz = NEW_INTEGER(1));
    nProtect += 1;
    pnAnnz = INTEGER_POINTER(spnAnnz);

    PROTECT(spaiArows = NEW_INTEGER(nCons));
    nProtect += 1;
    paiArows = INTEGER_POINTER(spaiArows);

    PROTECT(spadAcoef = NEW_NUMERIC(nCons));
    nProtect += 1;
    padAcoef = NUMERIC_POINTER(spadAcoef);

    *pnErrorCode = LSgetLPVariableDataj(pModel,
                                        iVar,
                                        pchVartype,
                                        pdC,
                                        pdL,
                                        pdU,
                                        pnAnnz,
                                        paiArows,
                                        padAcoef);

    CHECK_ERRCODE;

    SET_STRING_ELT(spchVartype,0,mkChar(pchVartype));

    PROTECT(spaiArows2 = NEW_INTEGER(*pnAnnz));
    nProtect += 1;
    paiArows2 = INTEGER_POINTER(spaiArows2);
    memcpy(paiArows2,paiArows,sizeof(int)*(*pnAnnz));

    PROTECT(spadAcoef2 = NEW_NUMERIC(*pnAnnz));
    nProtect += 1;
    padAcoef2 = NUMERIC_POINTER(spadAcoef2);
    memcpy(padAcoef2,padAcoef,sizeof(double)*(*pnAnnz));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spchVartype);
        SET_VECTOR_ELT(rList, 2, spdC);
        SET_VECTOR_ELT(rList, 3, spdL);
        SET_VECTOR_ELT(rList, 4, spdU);
        SET_VECTOR_ELT(rList, 5, spnAnnz);
        SET_VECTOR_ELT(rList, 6, spaiArows2);
        SET_VECTOR_ELT(rList, 7, spadAcoef2);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVariableNamej(SEXP  sModel,
                          SEXP  siVar)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar = Rf_asInteger(siVar);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachVarName;
    SEXP      spachVarName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pachVarName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachVarName = NEW_CHARACTER(1));
    nProtect += 1;
    pachVarName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetVariableNamej(pModel,
                                      iVar,
                                      pachVarName);
    CHECK_ERRCODE;

    SET_STRING_ELT(spachVarName,0,mkChar(pachVarName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachVarName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVariableIndex(SEXP  sModel,
                          SEXP  spszVarName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszVarName = (char *) CHAR(STRING_ELT(spszVarName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piVar;
    SEXP      spiVar = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "piVar"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiVar = NEW_INTEGER(1));
    nProtect += 1;
    piVar = INTEGER_POINTER(spiVar);

    *pnErrorCode = LSgetVariableIndex(pModel,pszVarName,piVar);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiVar);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConstraintNamei(SEXP     sModel,
                            SEXP     siCon)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCon = Rf_asInteger(siCon);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachConName;
    SEXP      spachConName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pachConName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachConName = NEW_CHARACTER(1));
    nProtect += 1;
    pachConName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetConstraintNamei(pModel,
                                        iCon,
                                        pachConName);
    CHECK_ERRCODE;

    SET_STRING_ELT(spachConName,0,mkChar(pachConName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachConName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConstraintIndex(SEXP  sModel,
                            SEXP  spszConName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszConName = (char *) CHAR(STRING_ELT(spszConName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piCon;
    SEXP      spiCon = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "piCon"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiCon = NEW_INTEGER(1));
    nProtect += 1;
    piCon = INTEGER_POINTER(spiCon);

    *pnErrorCode = LSgetConstraintIndex(pModel,pszConName,piCon);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiCon);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConstraintDatai(SEXP      sModel,
                            SEXP      siCon)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCon = Rf_asInteger(siCon);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pchConType;
    SEXP      spchConType = R_NilValue;
    char      *pchIsNlp;
    SEXP      spchIsNlp = R_NilValue;
    double    *pdB;
    SEXP      spdB = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pchConType", "pchIsNlp", "pdB"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spchConType = NEW_CHARACTER(1));
    nProtect += 1;
    pchConType = (char *)malloc(sizeof(char)*(1));

    PROTECT(spchIsNlp = NEW_CHARACTER(1));
    nProtect += 1;
    pchIsNlp = (char *)malloc(sizeof(char)*(1));

    PROTECT(spdB = NEW_NUMERIC(1));
    nProtect += 1;
    pdB = NUMERIC_POINTER(spdB);

    *pnErrorCode = LSgetConstraintDatai(pModel,
                                        iCon,
                                        pchConType,
                                        pchIsNlp,
                                        pdB);
    CHECK_ERRCODE;

    SET_STRING_ELT(spchConType,0,mkChar(pchConType));
    SET_STRING_ELT(spchIsNlp,0,mkChar(pchIsNlp));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spchConType);
        SET_VECTOR_ELT(rList, 2, spchIsNlp);
        SET_VECTOR_ELT(rList, 3, spdB);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetLPConstraintDatai(SEXP      sModel,
                              SEXP      siCon)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCon = Rf_asInteger(siCon);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pchConType;
    SEXP      spchConType = R_NilValue;
    double    *pdB;
    SEXP      spdB = R_NilValue;
    int       *pnNnz;
    SEXP      spnNnz = R_NilValue;
    int       *paiVar;
    SEXP      spaiVar = R_NilValue;
    double    *padAcoef;
    SEXP      spadAcoef = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[6] = {"ErrorCode", "pchConType", "pdB", "pnNnz", "paiVar", "padAcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 6;
    int       nIdx, nProtect = 0;
    int       nNnz;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spchConType = NEW_CHARACTER(1));
    nProtect += 1;
    pchConType = (char *)malloc(sizeof(char)*(1));

    PROTECT(spdB = NEW_NUMERIC(1));
    nProtect += 1;
    pdB = NUMERIC_POINTER(spdB);

    PROTECT(spnNnz = NEW_INTEGER(1));
    nProtect += 1;
    pnNnz = INTEGER_POINTER(spnNnz);

    *pnErrorCode = LSgetLPConstraintDatai(pModel,iCon,NULL,NULL,&nNnz,NULL,NULL);
    CHECK_ERRCODE;

    PROTECT(spaiVar = NEW_INTEGER(nNnz));
    nProtect += 1;
    paiVar = INTEGER_POINTER(spaiVar);

    PROTECT(spadAcoef = NEW_NUMERIC(nNnz));
    nProtect += 1;
    padAcoef = NUMERIC_POINTER(spadAcoef);

    *pnErrorCode = LSgetLPConstraintDatai(pModel,
                                          iCon,
                                          pchConType,
                                          pdB,
                                          pnNnz,
                                          paiVar,
                                          padAcoef);
    CHECK_ERRCODE;

    SET_STRING_ELT(spchConType,0,mkChar(pchConType));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spchConType);
        SET_VECTOR_ELT(rList, 2, spdB);
        SET_VECTOR_ELT(rList, 3, spnNnz);
        SET_VECTOR_ELT(rList, 4, spaiVar);
        SET_VECTOR_ELT(rList, 5, spadAcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConeNamei(SEXP     sModel,
                      SEXP     siCone)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCone = Rf_asInteger(siCone);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachConeName;
    SEXP      spachConeName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pachConeName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachConeName = NEW_CHARACTER(1));
    nProtect += 1;
    pachConeName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetConeNamei(pModel,
                                  iCone,
                                  pachConeName);
    CHECK_ERRCODE;

    SET_STRING_ELT(spachConeName,0,mkChar(pachConeName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachConeName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConeIndex(SEXP  sModel,
                      SEXP  spszConeName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszConeName = (char *) CHAR(STRING_ELT(spszConeName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piCone;
    SEXP      spiCone = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "piCone"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiCone = NEW_INTEGER(1));
    nProtect += 1;
    piCone = INTEGER_POINTER(spiCone);

    *pnErrorCode = LSgetConeIndex(pModel,pszConeName,piCone);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiCone);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConeDatai(SEXP      sModel,
                      SEXP      siCone)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCone = Rf_asInteger(siCone);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pchConeType;
    SEXP      spchConeType = R_NilValue;
    int       *piNnz;
    SEXP      spiNnz = R_NilValue;
    int       *paiCols;
    SEXP      spaiCols = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "pchConeType", "pdConeAlpha", "piNnz", "paiCols"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nNnz;
    double    *pdConeAlpha;
    SEXP      spdConeAlpha = R_NilValue;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spchConeType = NEW_CHARACTER(1));
    nProtect += 1;
    pchConeType = (char *)malloc(sizeof(char)*(1));

    PROTECT(spiNnz = NEW_INTEGER(1));
    nProtect += 1;
    piNnz = INTEGER_POINTER(spiNnz);

    *pnErrorCode = LSgetConeDatai(pModel,iCone,NULL,NULL,&nNnz,NULL);
    CHECK_ERRCODE;

    PROTECT(spdConeAlpha = NEW_NUMERIC(1));
    nProtect += 1;
    pdConeAlpha = NUMERIC_POINTER(spdConeAlpha);

    PROTECT(spaiCols = NEW_INTEGER(1));
    nProtect += 1;
    paiCols = INTEGER_POINTER(spaiCols);


    *pnErrorCode = LSgetConeDatai(pModel,iCone,pchConeType,pdConeAlpha, piNnz,paiCols);
    CHECK_ERRCODE;

    SET_STRING_ELT(spchConeType,0,mkChar(pchConeType));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spchConeType);
        SET_VECTOR_ELT(rList, 2, spdConeAlpha);
        SET_VECTOR_ELT(rList, 3, spiNnz);
        SET_VECTOR_ELT(rList, 4, spaiCols);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNLPData(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *paiNLPcols;
    SEXP      spaiNLPcols = R_NilValue;
    int       *panNLPcols;
    SEXP      spanNLPcols = R_NilValue;
    double    *padNLPcoef;
    SEXP      spadNLPcoef = R_NilValue;
    int       *paiNLProws;
    SEXP      spaiNLProws = R_NilValue;
    int       *pnNLPobj;
    SEXP      spnNLPobj = R_NilValue;
    int       *paiNLPobj;
    SEXP      spaiNLPobj = R_NilValue;
    double    *padNLPobj;
    SEXP      spadNLPobj = R_NilValue;
    char      *pachNLPConTypes;
    SEXP      spachNLPConTypes = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[9] = {"ErrorCode", "paiNLPcols", "panNLPcols", "padNLPcoef",
                           "paiNLProws", "pnNLPobj",
                           "paiNLPobj","padNLPobj", "pachNLPConTypes"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 9;
    int       nIdx, nProtect = 0;
    int       nVars, nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spaiNLPcols = NEW_INTEGER(nVars + 1));
    nProtect += 1;
    paiNLPcols = INTEGER_POINTER(spaiNLPcols);

    PROTECT(spanNLPcols = NEW_INTEGER(nVars + 1));
    nProtect += 1;
    panNLPcols = INTEGER_POINTER(spanNLPcols);

    PROTECT(spnNLPobj = NEW_INTEGER(1));
    nProtect += 1;
    pnNLPobj = INTEGER_POINTER(spnNLPobj);

    PROTECT(spachNLPConTypes = NEW_CHARACTER(1));
    nProtect += 1;
    pachNLPConTypes = (char *)malloc(sizeof(char)*(nCons));

    *pnErrorCode = LSgetNLPData(pModel,paiNLPcols,panNLPcols,NULL,
                                NULL,pnNLPobj,NULL,NULL,pachNLPConTypes);
    CHECK_ERRCODE;

    if(paiNLPcols[nVars] == 0 || *pnNLPobj == 0)
    {
        goto ErrorReturn;
    }

    SET_STRING_ELT(spachNLPConTypes,0,mkChar(pachNLPConTypes));

    PROTECT(spadNLPcoef = NEW_NUMERIC(paiNLPcols[nVars]));
    nProtect += 1;
    padNLPcoef = NUMERIC_POINTER(spadNLPcoef);

    PROTECT(spaiNLProws = NEW_INTEGER(paiNLPcols[nVars]));
    nProtect += 1;
    paiNLProws = INTEGER_POINTER(spaiNLProws);

    PROTECT(spaiNLPobj = NEW_INTEGER(*pnNLPobj));
    nProtect += 1;
    paiNLPobj = INTEGER_POINTER(spaiNLPobj);

    PROTECT(spadNLPobj = NEW_NUMERIC(*pnNLPobj));
    nProtect += 1;
    padNLPobj = NUMERIC_POINTER(spadNLPobj);

    *pnErrorCode = LSgetNLPData(pModel,NULL,NULL,padNLPcoef,
                                paiNLProws,NULL,paiNLPobj,padNLPobj,NULL);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spaiNLPcols);
        SET_VECTOR_ELT(rList, 2, spanNLPcols);
        SET_VECTOR_ELT(rList, 3, spadNLPcoef);
        SET_VECTOR_ELT(rList, 4, spaiNLProws);
        SET_VECTOR_ELT(rList, 5, spnNLPobj);
        SET_VECTOR_ELT(rList, 6, spaiNLPobj);
        SET_VECTOR_ELT(rList, 7, spadNLPobj);
        SET_VECTOR_ELT(rList, 8, spachNLPConTypes);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNLPConstraintDatai(SEXP  sModel,
                               SEXP  siCon)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iCon = Rf_asInteger(siCon);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnNnz;
    SEXP      spnNnz = R_NilValue;
    int       *paiNLPcols;
    SEXP      spaiNLPcols = R_NilValue;
    double    *padNLPcoef;
    SEXP      spadNLPcoef = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pnNnz","paiNLPcols","padNLPcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnNnz = NEW_INTEGER(1));
    nProtect += 1;
    pnNnz = INTEGER_POINTER(spnNnz);

    *pnErrorCode = LSgetNLPConstraintDatai(pModel,iCon,pnNnz,NULL,NULL);
    CHECK_ERRCODE;

    if(*pnNnz == 0)
    {
        goto ErrorReturn;
    }

    PROTECT(spaiNLPcols = NEW_INTEGER(*pnNnz));
    nProtect += 1;
    paiNLPcols = INTEGER_POINTER(spaiNLPcols);

    PROTECT(spadNLPcoef = NEW_NUMERIC(*pnNnz));
    nProtect += 1;
    padNLPcoef = NUMERIC_POINTER(spadNLPcoef);

    *pnErrorCode = LSgetNLPConstraintDatai(pModel,iCon,NULL,paiNLPcols,padNLPcoef);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnNnz);
        SET_VECTOR_ELT(rList, 2, spaiNLPcols);
        SET_VECTOR_ELT(rList, 3, spadNLPcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNLPVariableDataj(SEXP  sModel,
                             SEXP  siVar)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar = Rf_asInteger(siVar);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnNnz;
    SEXP      spnNnz = R_NilValue;
    int       *panNLProws;
    SEXP      spanNLProws = R_NilValue;
    double    *padNLPcoef;
    SEXP      spadNLPcoef = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pnNnz","panNLProws","padNLPcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnNnz = NEW_INTEGER(1));
    nProtect += 1;
    pnNnz = INTEGER_POINTER(spnNnz);

    *pnErrorCode = LSgetNLPVariableDataj(pModel,iVar,pnNnz,NULL,NULL);
    CHECK_ERRCODE;

    if(*pnNnz == 0)
    {
        goto ErrorReturn;
    }

    PROTECT(spanNLProws = NEW_INTEGER(*pnNnz));
    nProtect += 1;
    panNLProws = INTEGER_POINTER(spanNLProws);

    PROTECT(spadNLPcoef = NEW_NUMERIC(*pnNnz));
    nProtect += 1;
    padNLPcoef = NUMERIC_POINTER(spadNLPcoef);

    *pnErrorCode = LSgetNLPVariableDataj(pModel,iVar,NULL,panNLProws,padNLPcoef);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnNnz);
        SET_VECTOR_ELT(rList, 2, spanNLProws);
        SET_VECTOR_ELT(rList, 3, spadNLPcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNLPObjectiveData(SEXP  sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnNLPobjnnz;
    SEXP      spnNLPobjnnz = R_NilValue;
    int       *paiNLPobj;
    SEXP      spaiNLPobj = R_NilValue;
    double    *padNLPobj;
    SEXP      spadNLPobj = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode", "pnNLPobjnnz","paiNLPobj","padNLPobj"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnNLPobjnnz = NEW_INTEGER(1));
    nProtect += 1;
    pnNLPobjnnz = INTEGER_POINTER(spnNLPobjnnz);

    *pnErrorCode = LSgetNLPObjectiveData(pModel,pnNLPobjnnz,NULL,NULL);
    CHECK_ERRCODE;

    if(*pnNLPobjnnz == 0)
    {
        goto ErrorReturn;
    }

    PROTECT(spaiNLPobj = NEW_INTEGER(*pnNLPobjnnz));
    nProtect += 1;
    paiNLPobj = INTEGER_POINTER(spaiNLPobj);

    PROTECT(spadNLPobj = NEW_NUMERIC(*pnNLPobjnnz));
    nProtect += 1;
    padNLPobj = NUMERIC_POINTER(spadNLPobj);

    *pnErrorCode = LSgetNLPObjectiveData(pModel,NULL,paiNLPobj,padNLPobj);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnNLPobjnnz);
        SET_VECTOR_ELT(rList, 2, spaiNLPobj);
        SET_VECTOR_ELT(rList, 3, spadNLPobj);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDualModel(SEXP sModel,
                      SEXP sDualModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    prLSmodel prDualModel;
    pLSmodel  pDualModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    if(sDualModel != R_NilValue && R_ExternalPtrTag(sDualModel) == tagLSprob)
    {
        prDualModel = (prLSmodel)R_ExternalPtrAddr(sDualModel);
        pDualModel = prDualModel->pModel;
    }
    else
    {
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        goto ErrorReturn;
    }

    *pnErrorCode = LSgetDualModel(pModel,pDualModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLScalinfeasMIPsolution(SEXP sModel,
                              SEXP spadPrimalMipsol)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    double    *padPrimalMipsol = REAL(spadPrimalMipsol);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdIntPfeas;
    SEXP      spdIntPfeas = R_NilValue;
    double    *pbConsPfeas;
    SEXP      spbConsPfeas = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "pdIntPfeas", "pbConsPfeas"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    MAKE_REAL_ARRAY(padPrimalMipsol,spadPrimalMipsol);

    PROTECT(spdIntPfeas = NEW_NUMERIC(1));
    nProtect += 1;
    pdIntPfeas = NUMERIC_POINTER(spdIntPfeas);

    PROTECT(spbConsPfeas = NEW_NUMERIC(1));
    nProtect += 1;
    pbConsPfeas = NUMERIC_POINTER(spbConsPfeas);

    *pnErrorCode = LScalinfeasMIPsolution(pModel,pdIntPfeas,
                                          pbConsPfeas,padPrimalMipsol);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdIntPfeas);
        SET_VECTOR_ELT(rList, 2, spbConsPfeas);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetRoundMIPsolution(SEXP      sModel,
                             SEXP      spadPrimal,
                             SEXP      siUseOpti)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    double    *padPrimal;
    int       iUseOpti = Rf_asInteger(siUseOpti);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padPrimalRound;
    SEXP      spadPrimalRound = R_NilValue;
    double    *pdObjRound;
    SEXP      spdObjRound = R_NilValue;
    double    *pdPfeasRound;
    SEXP      spdPfeasRound = R_NilValue;
    int       *pnstatus;
    SEXP      spnstatus = R_NilValue;

    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "padPrimalRound", "pdObjRound", "pdPfeasRound", "pnstatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    MAKE_REAL_ARRAY(padPrimal,spadPrimal);

    PROTECT(spadPrimalRound = NEW_NUMERIC(nVars));
    nProtect += 1;
    padPrimalRound = NUMERIC_POINTER(spadPrimalRound);

    PROTECT(spdObjRound = NEW_NUMERIC(1));
    nProtect += 1;
    pdObjRound = NUMERIC_POINTER(spdObjRound);

    PROTECT(spdPfeasRound = NEW_NUMERIC(1));
    nProtect += 1;
    pdPfeasRound = NUMERIC_POINTER(spdPfeasRound);

    PROTECT(spnstatus = NEW_INTEGER(1));
    nProtect += 1;
    pnstatus = INTEGER_POINTER(spnstatus);

    *pnErrorCode = LSgetRoundMIPsolution(pModel,
                                         padPrimal,
                                         padPrimalRound,
                                         pdObjRound,
                                         pdPfeasRound,
                                         pnstatus,
                                         iUseOpti);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadPrimalRound);
        SET_VECTOR_ELT(rList, 2, spdObjRound);
        SET_VECTOR_ELT(rList, 3, spdPfeasRound);
        SET_VECTOR_ELT(rList, 4, spnstatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetRangeData(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padR;
    SEXP      spadR = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "padR"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadR = NEW_NUMERIC(nCons));
    nProtect += 1;
    padR = NUMERIC_POINTER(spadR);

    *pnErrorCode = LSgetRangeData(pModel,padR);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadR);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Model Modification Routines (25)                      *
*********************************************************/
SEXP rcLSaddConstraints(SEXP      sModel,
                        SEXP      snNumaddcons,
                        SEXP      spszConTypes,
                        SEXP      spaszConNames,
                        SEXP      spaiArows,
                        SEXP      spadAcoef,
                        SEXP      spaiAcols,
                        SEXP      spadB)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nNumaddcons = Rf_asInteger(snNumaddcons);
    char      *pszConTypes = (char *) CHAR(STRING_ELT(spszConTypes,0));
    char      **paszConNames;
    int       *paiArows = INTEGER(spaiArows);
    double    *padAcoef = REAL(spadAcoef);
    int       *paiAcols = INTEGER(spaiAcols);
    double    *padB = REAL(spadB);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_CHAR_ARRAY(paszConNames,spaszConNames);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddConstraints(pModel,
                                    nNumaddcons,
                                    pszConTypes,
                                    paszConNames,
                                    paiArows,
                                    padAcoef,
                                    paiAcols,
                                    padB);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddVariables(SEXP      sModel,
                      SEXP      snNumaddvars,
                      SEXP      spszVarTypes,
                      SEXP      spaszVarNames,
                      SEXP      spaiAcols,
                      SEXP      spanAcols,
                      SEXP      spadAcoef,
                      SEXP      spaiArows,
                      SEXP      spadC,
                      SEXP      spadL,
                      SEXP      spadU)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nNumaddvars = Rf_asInteger(snNumaddvars);
    char      *pszVarTypes = (char *) CHAR(STRING_ELT(spszVarTypes,0));
    char      **paszVarNames;
    int       *paiAcols = INTEGER(spaiAcols);
    int       *panAcols;
    double    *padAcoef = REAL(spadAcoef);
    int       *paiArows = INTEGER(spaiArows);
    double    *padC = REAL(spadC);
    double    *padL;
    double    *padU;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_CHAR_ARRAY(paszVarNames,spaszVarNames);
    MAKE_INT_ARRAY(panAcols,spanAcols);
    MAKE_REAL_ARRAY(padL,spadL);
    MAKE_REAL_ARRAY(padU,spadU);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddVariables(pModel,
                                  nNumaddvars,
                                  pszVarTypes,
                                  paszVarNames,
                                  paiAcols,
                                  panAcols,
                                  padAcoef,
                                  paiArows,
                                  padC,
                                  padL,
                                  padU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddCones(SEXP      sModel,
                  SEXP      snCone,
                  SEXP      spszConeTypes,
                  SEXP      spadConeAlpha,
                  SEXP      spaszConenames,
                  SEXP      spaiConebegcol,
                  SEXP      spaiConecols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCone = Rf_asInteger(snCone);
    char      *pszConeTypes = (char *) CHAR(STRING_ELT(spszConeTypes,0));
    char      **paszConenames;
    int       *paiConebegcol = INTEGER(spaiConebegcol);
    int       *paiConecols = INTEGER(spaiConecols);
    double   *padConeAlpha = REAL(spadConeAlpha);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_CHAR_ARRAY(paszConenames,spaszConenames);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddCones(pModel,
                              nCone,
                              pszConeTypes,
                              padConeAlpha,
                              paszConenames,
                              paiConebegcol,
                              paiConecols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddSETS(SEXP      sModel,
                 SEXP      snSETS,
                 SEXP      spszSETStype,
                 SEXP      spaiCARDnum,
                 SEXP      spaiSETSbegcol,
                 SEXP      spaiSETScols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSETS = Rf_asInteger(snSETS);
    char      *pszSETStype = (char *) CHAR(STRING_ELT(spszSETStype,0));
    int       *paiCARDnum = INTEGER(spaiCARDnum);
    int       *paiSETSbegcol = INTEGER(spaiSETSbegcol);
    int       *paiSETScols = INTEGER(spaiSETScols);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddSETS(pModel,
                             nSETS,
                             pszSETStype,
                             paiCARDnum,
                             paiSETSbegcol,
                             paiSETScols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddQCterms(SEXP      sModel,
                    SEXP      snQCnonzeros,
                    SEXP      spaiQCconndx,
                    SEXP      spaiQCvarndx1,
                    SEXP      spaiQCvarndx2,
                    SEXP      spadQCcoef)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQCnonzeros = Rf_asInteger(snQCnonzeros);
    int       *paiQCconndx = INTEGER(spaiQCconndx);
    int       *paiQCvarndx1 = INTEGER(spaiQCvarndx1);
    int       *paiQCvarndx2 = INTEGER(spaiQCvarndx2);
    double    *padQCcoef = REAL(spadQCcoef);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddQCterms(pModel,
                                nQCnonzeros,
                                paiQCconndx,
                                paiQCvarndx1,
                                paiQCvarndx2,
                                padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteConstraints(SEXP      sModel,
                           SEXP      snCons,
                           SEXP      spaiCons)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCons = Rf_asInteger(snCons);
    int       *paiCons = INTEGER(spaiCons);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteConstraints(pModel,nCons,paiCons);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteSETS(SEXP      sModel,
                    SEXP      snSETS,
                    SEXP      spaiSETS)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSETS = Rf_asInteger(snSETS);
    int       *paiSETS = INTEGER(spaiSETS);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteSETS(pModel,nSETS,paiSETS);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteCones(SEXP      sModel,
                     SEXP      snCones,
                     SEXP      spaiCones)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCones = Rf_asInteger(snCones);
    int       *paiCones = INTEGER(spaiCones);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteCones(pModel,nCones,paiCones);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteSemiContVars(SEXP      sModel,
                            SEXP      snSCVars,
                            SEXP      spaiSCVars)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSCVars = Rf_asInteger(snSCVars);
    int       *paiSCVars = INTEGER(spaiSCVars);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteSemiContVars(pModel,nSCVars,paiSCVars);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteVariables(SEXP      sModel,
                         SEXP      snVars,
                         SEXP      spaiVars)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nVars = Rf_asInteger(snVars);
    int       *paiVars = INTEGER(spaiVars);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteVariables(pModel,nVars,paiVars);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteQCterms(SEXP      sModel,
                       SEXP      snCons,
                       SEXP      spaiCons)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCons = Rf_asInteger(snCons);
    int       *paiCons = INTEGER(spaiCons);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteQCterms(pModel,nCons,paiCons);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteAj(SEXP      sModel,
                  SEXP      siVar1,
                  SEXP      snRows,
                  SEXP      spaiRows)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar1 = Rf_asInteger(siVar1);
    int       nRows = Rf_asInteger(snRows);
    int       *paiRows = INTEGER(spaiRows);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteAj(pModel,iVar1,nRows,paiRows);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyLowerBounds(SEXP      sModel,
                           SEXP      snVars,
                           SEXP      spaiVars,
                           SEXP      spadL)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nVars = Rf_asInteger(snVars);
    int       *paiVars = INTEGER(spaiVars);
    double    *padL = REAL(spadL);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyLowerBounds(pModel,nVars,paiVars,padL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyUpperBounds(SEXP      sModel,
                           SEXP      snVars,
                           SEXP      spaiVars,
                           SEXP      spadU)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nVars = Rf_asInteger(snVars);
    int       *paiVars = INTEGER(spaiVars);
    double    *padU = REAL(spadU);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyUpperBounds(pModel,nVars,paiVars,padU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyRHS(SEXP      sModel,
                   SEXP      snCons,
                   SEXP      spaiCons,
                   SEXP      spadB)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCons = Rf_asInteger(snCons);
    int       *paiCons = INTEGER(spaiCons);
    double    *padB = REAL(spadB);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyRHS(pModel,nCons,paiCons,padB);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyObjective(SEXP      sModel,
                         SEXP      snVars,
                         SEXP      spaiVars,
                         SEXP      spadC)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nVars = Rf_asInteger(snVars);
    int       *paiVars = INTEGER(spaiVars);
    double    *padC = REAL(spadC);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyObjective(pModel,nVars,paiVars,padC);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyAj(SEXP     sModel,
                  SEXP     siVar1,
                  SEXP     snRows,
                  SEXP     spaiRows,
                  SEXP     spadAj)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar1 = Rf_asInteger(siVar1);
    int       nRows = Rf_asInteger(snRows);
    int       *paiRows = INTEGER(spaiRows);
    double    *padAj = REAL(spadAj);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyAj(pModel,iVar1,nRows,paiRows,padAj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyCone(SEXP     sModel,
                    SEXP     scConeType,
                    SEXP     siConeNum,
                    SEXP     siConeNnz,
                    SEXP     spaiConeCols,
                    SEXP     sdConeAlpha)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      cConeType = *((char *)CHAR(STRING_ELT(scConeType,0)));
    int       iConeNum = Rf_asInteger(siConeNum);
    int       iConeNnz = Rf_asInteger(siConeNnz);
    int       *paiConeCols = INTEGER(spaiConeCols);
    double    dConeAlpha = Rf_asReal(sdConeAlpha);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyCone(pModel,cConeType,iConeNum,iConeNnz,paiConeCols,dConeAlpha);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifySET(SEXP     sModel,
                   SEXP     scSETtype,
                   SEXP     siSETnum,
                   SEXP     siSETnnz,
                   SEXP     spaiSETcols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      cSETtype = *((char *)CHAR(STRING_ELT(scSETtype,0)));
    int       iSETnum = Rf_asInteger(siSETnum);
    int       iSETnnz = Rf_asInteger(siSETnnz);
    int       *paiSETcols = INTEGER(spaiSETcols);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifySET(pModel,cSETtype,iSETnum,iSETnnz,paiSETcols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifySemiContVars(SEXP     sModel,
                            SEXP     snSCVars,
                            SEXP     spaiSCVars,
                            SEXP     spadL,
                            SEXP     spadU)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSCVars = Rf_asInteger(snSCVars);
    int       *paiSCVars = INTEGER(spaiSCVars);
    double    *padL = REAL(spadL);
    double    *padU = REAL(spadU);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifySemiContVars(pModel,nSCVars,paiSCVars,padL,padU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyConstraintType(SEXP     sModel,
                              SEXP     snCons,
                              SEXP     spaiCons,
                              SEXP     spszConTypes)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       nCons = Rf_asInteger(snCons);
    int       *paiCons = INTEGER(spaiCons);
    char      *pszConTypes = (char *)CHAR(STRING_ELT(spszConTypes,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyConstraintType(pModel,nCons,paiCons,pszConTypes);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSmodifyVariableType(SEXP     sModel,
                            SEXP     snVars,
                            SEXP     spaiVars,
                            SEXP     spszVarTypes)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       nVars = Rf_asInteger(snVars);
    int       *paiVars = INTEGER(spaiVars);
    char      *pszVarTypes = (char *)CHAR(STRING_ELT(spszVarTypes,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSmodifyVariableType(pModel,nVars,paiVars,pszVarTypes);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddNLPAj(SEXP     sModel,
                  SEXP     siVar1,
                  SEXP     snRows,
                  SEXP     spaiRows,
                  SEXP     spadAj)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iVar1 = Rf_asInteger(siVar1);
    int       nRows = Rf_asInteger(snRows);
    int       *paiRows = INTEGER(spaiRows);
    double    *padAj = REAL(spadAj);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddNLPAj(pModel,iVar1,nRows,paiRows,padAj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddNLPobj(SEXP      sModel,
                   SEXP      snCols,
                   SEXP      spaiCols,
                   SEXP      spadColj)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCols = Rf_asInteger(snCols);
    int       *paiCols = INTEGER(spaiCols);
    double    *padColj = REAL(spadColj);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddNLPobj(pModel,nCols,paiCols,padColj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeleteNLPobj(SEXP      sModel,
                      SEXP      snCols,
                      SEXP      spaiCols)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nCols = Rf_asInteger(snCols);
    int       *paiCols = INTEGER(spaiCols);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSdeleteNLPobj(pModel,nCols,paiCols);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Model & Solution Analysis Routines (10)               *
*********************************************************/
SEXP rcLSgetConstraintRanges(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padDec;
    SEXP      spadDec = R_NilValue;
    double    *padInc;
    SEXP      spadInc = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "padDec", "padInc"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadDec = NEW_NUMERIC(nCons));
    nProtect += 1;
    padDec = NUMERIC_POINTER(spadDec);

    PROTECT(spadInc = NEW_NUMERIC(nCons));
    nProtect += 1;
    padInc = NUMERIC_POINTER(spadInc);

    *pnErrorCode = LSgetConstraintRanges(pModel,padDec,padInc);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadDec);
        SET_VECTOR_ELT(rList, 2, spadInc);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetObjectiveRanges_x(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padDec;
    SEXP      spadDec = R_NilValue;
    double    *padInc;
    SEXP      spadInc = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "padDec", "padInc"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadDec = NEW_NUMERIC(nVars));
    nProtect += 1;
    padDec = NUMERIC_POINTER(spadDec);

    PROTECT(spadInc = NEW_NUMERIC(nVars));
    nProtect += 1;
    padInc = NUMERIC_POINTER(spadInc);

    *pnErrorCode = LSgetObjectiveRanges(pModel,padDec,padInc);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadDec);
        SET_VECTOR_ELT(rList, 2, spadInc);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetBoundRanges(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padDec;
    SEXP      spadDec = R_NilValue;
    double    *padInc;
    SEXP      spadInc = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "padDec", "padInc"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadDec = NEW_NUMERIC(nVars));
    nProtect += 1;
    padDec = NUMERIC_POINTER(spadDec);

    PROTECT(spadInc = NEW_NUMERIC(nVars));
    nProtect += 1;
    padInc = NUMERIC_POINTER(spadInc);

    *pnErrorCode = LSgetBoundRanges(pModel,padDec,padInc);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadDec);
        SET_VECTOR_ELT(rList, 2, spadInc);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetBestBounds(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padBestL;
    SEXP      spadBestL = R_NilValue;
    double    *padBestU;
    SEXP      spadBestU = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode", "padBestL", "padBestU"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadBestL = NEW_NUMERIC(nVars));
    nProtect += 1;
    padBestL = NUMERIC_POINTER(spadBestL);

    PROTECT(spadBestU = NEW_NUMERIC(nVars));
    nProtect += 1;
    padBestU = NUMERIC_POINTER(spadBestU);

    *pnErrorCode = LSgetBestBounds(pModel,padBestL,padBestU);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadBestL);
        SET_VECTOR_ELT(rList, 2, spadBestU);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfindIIS(SEXP      sModel,
                 SEXP      snLevel)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nLevel = Rf_asInteger(snLevel);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSfindIIS(pModel,nLevel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfindIUS(SEXP      sModel,
                 SEXP      snLevel)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nLevel = Rf_asInteger(snLevel);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSfindIUS(pModel,nLevel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfindBlockStructure(SEXP      sModel,
                            SEXP      snBlock,
                            SEXP      snType)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nBlock = Rf_asInteger(snBlock);
    int       nType = Rf_asInteger(snType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSfindBlockStructure(pModel,nBlock,nType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetIIS(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnSuf_r;
    SEXP      spnSuf_r = R_NilValue;
    int       *pnIIS_r;
    SEXP      spnIIS_r = R_NilValue;
    int       *paiCons;
    SEXP      spaiCons = R_NilValue;
    int       *pnSuf_c;
    SEXP      spnSuf_c = R_NilValue;
    int       *pnIIS_c;
    SEXP      spnIIS_c = R_NilValue;
    int       *paiVars;
    SEXP      spaiVars = R_NilValue;
    int       *panBnds;
    SEXP      spanBnds = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[8] = {"ErrorCode","pnSuf_r","pnIIS_r","paiCons",
                           "pnSuf_c","pnIIS_c","paiVars", "panBnds"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 8;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnSuf_r = NEW_INTEGER(1));
    nProtect += 1;
    pnSuf_r = INTEGER_POINTER(spnSuf_r);

    PROTECT(spnIIS_r = NEW_INTEGER(1));
    nProtect += 1;
    pnIIS_r = INTEGER_POINTER(spnIIS_r);

    PROTECT(spnSuf_c = NEW_INTEGER(1));
    nProtect += 1;
    pnSuf_c = INTEGER_POINTER(spnSuf_c);

    PROTECT(spnIIS_c = NEW_INTEGER(1));
    nProtect += 1;
    pnIIS_c = INTEGER_POINTER(spnIIS_c);

    *pnErrorCode = LSgetIIS(pModel,pnSuf_r,pnIIS_r,NULL,pnSuf_c,pnIIS_c,NULL,NULL);
    CHECK_ERRCODE;

    PROTECT(spaiCons = NEW_INTEGER(*pnIIS_r));
    nProtect += 1;
    paiCons = INTEGER_POINTER(spaiCons);

    PROTECT(spaiVars = NEW_INTEGER(*pnIIS_c));
    nProtect += 1;
    paiVars = INTEGER_POINTER(spaiVars);

    PROTECT(spanBnds = NEW_INTEGER(*pnIIS_c));
    nProtect += 1;
    panBnds = INTEGER_POINTER(spanBnds);

    *pnErrorCode = LSgetIIS(pModel,NULL,NULL,paiCons,NULL,NULL,paiVars,panBnds);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnSuf_r);
        SET_VECTOR_ELT(rList, 2, spnIIS_r);
        SET_VECTOR_ELT(rList, 3, spaiCons);
        SET_VECTOR_ELT(rList, 4, spnSuf_c);
        SET_VECTOR_ELT(rList, 5, spnIIS_c);
        SET_VECTOR_ELT(rList, 6, spaiVars);
        SET_VECTOR_ELT(rList, 7, spanBnds);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetIUS(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnSuf;
    SEXP      spnSuf = R_NilValue;
    int       *pnIUS;
    SEXP      spnIUS = R_NilValue;
    int       *paiVars;
    SEXP      spaiVars = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode","pnSuf","pnIUS","paiVars"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnSuf = NEW_INTEGER(1));
    nProtect += 1;
    pnSuf = INTEGER_POINTER(spnSuf);

    PROTECT(spnIUS = NEW_INTEGER(1));
    nProtect += 1;
    pnIUS = INTEGER_POINTER(spnIUS);

    *pnErrorCode = LSgetIUS(pModel,pnSuf,pnIUS,NULL);
    CHECK_ERRCODE;

    PROTECT(spaiVars = NEW_INTEGER(*pnIUS));
    nProtect += 1;
    paiVars = INTEGER_POINTER(spaiVars);

    *pnErrorCode = LSgetIUS(pModel,NULL,NULL,paiVars);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnSuf);
        SET_VECTOR_ELT(rList, 2, spnIUS);
        SET_VECTOR_ELT(rList, 3, spaiVars);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetBlockStructure(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnBlock;
    SEXP      spnBlock = R_NilValue;
    int       *panRblock;
    SEXP      spanRblock = R_NilValue;
    int       *panCblock;
    SEXP      spanCblock = R_NilValue;
    int       *pnType;
    SEXP      spnType = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode","pnBlock","panRblock","panCblock","pnType"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;
    int       nVars, nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    PROTECT(spnBlock = NEW_INTEGER(1));
    nProtect += 1;
    pnBlock = INTEGER_POINTER(spnBlock);

    PROTECT(spanRblock = NEW_INTEGER(nCons));
    nProtect += 1;
    panRblock = INTEGER_POINTER(spanRblock);

    PROTECT(spanCblock = NEW_INTEGER(nVars));
    nProtect += 1;
    panCblock = INTEGER_POINTER(spanCblock);

    PROTECT(spnType = NEW_INTEGER(1));
    nProtect += 1;
    pnType = INTEGER_POINTER(spnType);

    *pnErrorCode = LSgetBlockStructure(pModel,pnBlock,panRblock,panCblock,pnType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnBlock);
        SET_VECTOR_ELT(rList, 2, spanRblock);
        SET_VECTOR_ELT(rList, 3, spanCblock);
        SET_VECTOR_ELT(rList, 4, spnType);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Advanced Routines (0)                                 *
*********************************************************/

/********************************************************
* Callback Management Routines(0)                       *
*********************************************************/

/********************************************************
* Memory Related Routines(9)                            *
*********************************************************/
SEXP rcLSfreeSolverMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeSolverMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfreeHashMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeHashMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfreeSolutionMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeSolutionMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfreeMIPSolutionMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeMIPSolutionMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfreeGOPSolutionMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeGOPSolutionMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetProbAllocSizes(SEXP      sModel,
                           SEXP      sn_vars_alloc,
                           SEXP      sn_cons_alloc,
                           SEXP      sn_QC_alloc,
                           SEXP      sn_Annz_alloc,
                           SEXP      sn_Qnnz_alloc,
                           SEXP      sn_NLPnnz_alloc)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       n_vars_alloc = Rf_asInteger(sn_vars_alloc);
    int       n_cons_alloc = Rf_asInteger(sn_cons_alloc);
    int       n_QC_alloc = Rf_asInteger(sn_QC_alloc);
    int       n_Annz_alloc = Rf_asInteger(sn_Annz_alloc);
    int       n_Qnnz_alloc = Rf_asInteger(sn_Qnnz_alloc);
    int       n_NLPnnz_alloc = Rf_asInteger(sn_NLPnnz_alloc);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetProbAllocSizes(pModel,
                                       n_vars_alloc,
                                       n_cons_alloc,
                                       n_QC_alloc,
                                       n_Annz_alloc,
                                       n_Qnnz_alloc,
                                       n_NLPnnz_alloc);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetProbNameAllocSizes(SEXP      sModel,
                               SEXP      sn_varname_alloc,
                               SEXP      sn_rowname_alloc)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       n_varname_alloc = Rf_asInteger(sn_varname_alloc);
    int       n_rowname_alloc = Rf_asInteger(sn_rowname_alloc);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetProbNameAllocSizes(pModel,
                                           n_varname_alloc,
                                           n_rowname_alloc);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddEmptySpacesAcolumns(SEXP      sModel,
                                SEXP      spaiColnnz)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *paiColnnz = INTEGER(spaiColnnz);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddEmptySpacesAcolumns(pModel,
                                            paiColnnz);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddEmptySpacesNLPAcolumns(SEXP      sModel,
                                   SEXP      spaiColnnz)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *paiColnnz = INTEGER(spaiColnnz);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddEmptySpacesNLPAcolumns(pModel,
                                               paiColnnz);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* new  callback functions from version 5.+ (0)          *
*********************************************************/

/********************************************************
* new  functions from version 5.+ (0)                   *
*********************************************************/

/********************************************************
* Stochastic Programming Interface (88)                 *
*********************************************************/
SEXP rcLSwriteDeteqMPSFile(SEXP      sModel,
                           SEXP      spszFilename,
                           SEXP      snFormat,
                           SEXP      siType)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pszFilename = (char *)CHAR(STRING_ELT(spszFilename,0));
    int       nFormat = Rf_asInteger(snFormat);
    int       iType = Rf_asInteger(siType);
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteDeteqMPSFile(pModel,pszFilename,nFormat,iType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteDeteqLINDOFile(SEXP      sModel,
                             SEXP      spszFilename,
                             SEXP      siType)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pszFilename = (char *)CHAR(STRING_ELT(spszFilename,0));
    int       iType = Rf_asInteger(siType);
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteDeteqLINDOFile(pModel,pszFilename,iType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteSMPSFile(SEXP      sModel,
                       SEXP      spszCorefile,
                       SEXP      spszTimefile,
                       SEXP      spszStocfile,
                       SEXP      snMPStype)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszCorefile = (char *)CHAR(STRING_ELT(spszCorefile,0));
    char      *pszTimefile = (char *)CHAR(STRING_ELT(spszTimefile,0));
    char      *pszStocfile = (char *)CHAR(STRING_ELT(spszStocfile,0));
    int       nMPStype = Rf_asInteger(snMPStype);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteSMPSFile(pModel,pszCorefile,pszTimefile,pszStocfile,nMPStype);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadSMPSFile(SEXP      sModel,
                      SEXP      spszCorefile,
                      SEXP      spszTimefile,
                      SEXP      spszStocfile,
                      SEXP      snMPStype)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszCorefile = (char *)CHAR(STRING_ELT(spszCorefile,0));
    char      *pszTimefile = (char *)CHAR(STRING_ELT(spszTimefile,0));
    char      *pszStocfile = (char *)CHAR(STRING_ELT(spszStocfile,0));
    int       nMPStype = Rf_asInteger(snMPStype);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadSMPSFile(pModel,pszCorefile,pszTimefile,pszStocfile,nMPStype);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteSMPIFile(SEXP      sModel,
                       SEXP      spszCorefile,
                       SEXP      spszTimefile,
                       SEXP      spszStocfile)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszCorefile = (char *)CHAR(STRING_ELT(spszCorefile,0));
    char      *pszTimefile = (char *)CHAR(STRING_ELT(spszTimefile,0));
    char      *pszStocfile = (char *)CHAR(STRING_ELT(spszStocfile,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteSMPIFile(pModel,pszCorefile,pszTimefile,pszStocfile);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSreadSMPIFile(SEXP      sModel,
                      SEXP      spszCorefile,
                      SEXP      spszTimefile,
                      SEXP      spszStocfile)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszCorefile = (char *)CHAR(STRING_ELT(spszCorefile,0));
    char      *pszTimefile = (char *)CHAR(STRING_ELT(spszTimefile,0));
    char      *pszStocfile = (char *)CHAR(STRING_ELT(spszStocfile,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSreadSMPIFile(pModel,pszCorefile,pszTimefile,pszStocfile);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteScenarioSolutionFile(SEXP      sModel,
                                   SEXP      sjScenario,
                                   SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteScenarioSolutionFile(pModel,jScenario,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteNodeSolutionFile(SEXP      sModel,
                               SEXP      sjScenario,
                               SEXP      siStage,
                               SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iStage = Rf_asInteger(siStage);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteNodeSolutionFile(pModel,jScenario,iStage,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteScenarioMPIFile(SEXP      sModel,
                              SEXP      sjScenario,
                              SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteScenarioMPIFile(pModel,jScenario,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteScenarioMPSFile(SEXP      sModel,
                              SEXP      sjScenario,
                              SEXP      spszFname,
                              SEXP      snFormat)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));
    int       nFormat = Rf_asInteger(snFormat);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteScenarioMPSFile(pModel,jScenario,pszFname,nFormat);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSwriteScenarioLINDOFile(SEXP      sModel,
                                SEXP      sjScenario,
                                SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSwriteScenarioLINDOFile(pModel,jScenario,pszFname);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelStocDouParameter(SEXP      sModel,
                                  SEXP      siPar,
                                  SEXP      sdVal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iPar = Rf_asInteger(siPar);
    double    dVal = Rf_asReal(sdVal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelStocDouParameter(pModel,iPar,dVal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelStocDouParameter(SEXP      sModel,
                                  SEXP      siPar)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iPar = Rf_asInteger(siPar);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValue;
    SEXP      spdValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdValue = NEW_NUMERIC(1));
    nProtect += 1;
    pdValue = NUMERIC_POINTER(spdValue);

    *pnErrorCode = LSgetModelStocDouParameter(pModel,iPar,pdValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelStocIntParameter(SEXP      sModel,
                                  SEXP      siPar,
                                  SEXP      siVal)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iPar = Rf_asInteger(siPar);
    int       iVal = Rf_asInteger(siVal);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelStocIntParameter(pModel,iPar,iVal);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelStocIntParameter(SEXP      sModel,
                                  SEXP      siPar)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iPar = Rf_asInteger(siPar);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piValue;
    SEXP      spiValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","piValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiValue = NEW_INTEGER(1));
    nProtect += 1;
    piValue = INTEGER_POINTER(spiValue);

    *pnErrorCode = LSgetModelStocIntParameter(pModel,iPar,piValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioIndex(SEXP      sModel,
                          SEXP      spszName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszName = (char *)CHAR(STRING_ELT(spszName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnIndex;
    SEXP      spnIndex = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnIndex"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnIndex = NEW_INTEGER(1));
    nProtect += 1;
    pnIndex = INTEGER_POINTER(spnIndex);

    *pnErrorCode = LSgetScenarioIndex(pModel,pszName,pnIndex);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnIndex);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStageIndex(SEXP      sModel,
                       SEXP      spszName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszName = (char *)CHAR(STRING_ELT(spszName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnIndex;
    SEXP      spnIndex = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnIndex"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnIndex = NEW_INTEGER(1));
    nProtect += 1;
    pnIndex = INTEGER_POINTER(spnIndex);

    *pnErrorCode = LSgetStageIndex(pModel,pszName,pnIndex);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnIndex);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocParIndex(SEXP      sModel,
                         SEXP      spszName)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *pszName = (char *)CHAR(STRING_ELT(spszName,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnIndex;
    SEXP      spnIndex = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnIndex"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnIndex = NEW_INTEGER(1));
    nProtect += 1;
    pnIndex = INTEGER_POINTER(spnIndex);

    *pnErrorCode = LSgetStocParIndex(pModel,pszName,pnIndex);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnIndex);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocParName(SEXP      sModel,
                        SEXP      snIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachName;
    SEXP      spachName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pachName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachName = NEW_CHARACTER(1));
    nProtect += 1;
    pachName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetStocParName(pModel,nIndex,pachName);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachName,0,mkChar(pachName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioName(SEXP      sModel,
                         SEXP      snIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachName;
    SEXP      spachName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pachName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachName = NEW_CHARACTER(1));
    nProtect += 1;
    pachName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetScenarioName(pModel,nIndex,pachName);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachName,0,mkChar(pachName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStageName(SEXP      sModel,
                      SEXP      snIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pachName;
    SEXP      spachName = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pachName"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachName = NEW_CHARACTER(1));
    nProtect += 1;
    pachName = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetStageName(pModel,nIndex,pachName);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachName,0,mkChar(pachName));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachName);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocIInfo(SEXP      sModel,
                      SEXP      snQuery,
                      SEXP      snParam)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSgetStocInfo(pModel,nQuery,nParam,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocDInfo(SEXP      sModel,
                      SEXP      snQuery,
                      SEXP      snParam)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetStocInfo(pModel,nQuery,nParam,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocSInfo(SEXP      sModel,
                      SEXP      snQuery,
                      SEXP      snParam)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nParam = Rf_asInteger(snParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pszResult;
    SEXP      spszResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pszResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spszResult = NEW_CHARACTER(1));
    nProtect += 1;
    pszResult = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetStocInfo(pModel,nQuery,nParam,pszResult);

    CHECK_ERRCODE;

    SET_STRING_ELT(spszResult,0,mkChar(pszResult));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spszResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocCCPIInfo(SEXP      sModel,
                         SEXP      snQuery,
                         SEXP      snScenarioIndex,
                         SEXP      snCPPIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nScenarioIndex = Rf_asInteger(snScenarioIndex);
    int       nCPPIndex = Rf_asInteger(snCPPIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSgetStocCCPInfo(pModel,nQuery,nScenarioIndex,nCPPIndex,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocCCPDInfo(SEXP      sModel,
                         SEXP      snQuery,
                         SEXP      snScenarioIndex,
                         SEXP      snCPPIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nScenarioIndex = Rf_asInteger(snScenarioIndex);
    int       nCPPIndex = Rf_asInteger(snCPPIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetStocCCPInfo(pModel,nQuery,nScenarioIndex,nCPPIndex,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocCCPSInfo(SEXP      sModel,
                         SEXP      snQuery,
                         SEXP      snScenarioIndex,
                         SEXP      snCPPIndex)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       nScenarioIndex = Rf_asInteger(snScenarioIndex);
    int       nCPPIndex = Rf_asInteger(snCPPIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      *pszResult;
    SEXP      spszResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pszResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spszResult = NEW_CHARACTER(1));
    nProtect += 1;
    pszResult = (char *)malloc(sizeof(char)*(256));

    *pnErrorCode = LSgetStocCCPInfo(pModel,nQuery,nScenarioIndex,nCPPIndex,pszResult);

    CHECK_ERRCODE;

    SET_STRING_ELT(spszResult,0,mkChar(pszResult));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spszResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadSampleSizes(SEXP      sModel,
                         SEXP      spanSampleSize)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panSampleSize = INTEGER(spanSampleSize);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadSampleSizes(pModel,panSampleSize);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadConstraintStages(SEXP      sModel,
                              SEXP      spanStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panStage = INTEGER(spanStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadConstraintStages(pModel,panStage);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadVariableStages(SEXP      sModel,
                            SEXP      spanStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panStage = INTEGER(spanStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadVariableStages(pModel,panStage);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadStageData(SEXP      sModel,
                       SEXP      snumStages,
                       SEXP      spanRstage,
                       SEXP      spanCstage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       numStages = Rf_asInteger(snumStages);
    int       *panRstage = INTEGER(spanRstage);
    int       *panCstage = INTEGER(spanCstage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadStageData(pModel,numStages,panRstage,panCstage);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadStocParData(SEXP      sModel,
                         SEXP      spanSparStage,
                         SEXP      spadSparValue)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panSparStage = INTEGER(spanSparStage);
    double    *padSparValue = REAL(spadSparValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadStocParData(pModel,panSparStage,padSparValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadStocParNames(SEXP      sModel,
                          SEXP      snSvars,
                          SEXP      spaszSVarNames)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSvars = Rf_asInteger(snSvars);
    char      **paszSVarNames;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    MAKE_CHAR_CHAR_ARRAY(paszSVarNames,spaszSVarNames);

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadStocParNames(pModel,nSvars,paszSVarNames);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDeteqModel(SEXP      sModel,
                       SEXP      siDeqType)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iDeqType = Rf_asInteger(siDeqType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    prLSmodel prdModel = NULL;
    SEXP      sprdModel = R_NilValue;
    int       nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    prdModel = (prLSmodel)malloc(sizeof(rLSmodel)*1);
    if(prdModel == NULL)
    {
        return R_NilValue;
    }

    prdModel->pModel = LSgetDeteqModel(pModel,iDeqType,pnErrorCode);
    CHECK_ERRCODE;

    SET_PRINT_LOG(prdModel->pModel,*pnErrorCode);
    CHECK_ERRCODE;

    SET_MODEL_CALLBACK(prdModel->pModel,*pnErrorCode);
    CHECK_ERRCODE;

    sprdModel = R_MakeExternalPtr(prdModel,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sprdModel,tagLSprob);

ErrorReturn:

    UNPROTECT(nProtect);

    if(*pnErrorCode)
    {
        Rprintf("\nFail to get DeteqModel (error %d)\n",*pnErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }
    else
    {
        return sprdModel;
    }
}

SEXP rcLSaggregateStages(SEXP      sModel,
                         SEXP      spanScheme,
                         SEXP      snLength)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       *panScheme = INTEGER(spanScheme);
    int       nLength = Rf_asInteger(snLength);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaggregateStages(pModel,panScheme,nLength);

    prModel->pModel = NULL;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStageAggScheme(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panScheme;
    SEXP      spanScheme = R_NilValue;
    int       *pnLength;
    SEXP      spnLength = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","panScheme","pnLength"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnLength = NEW_INTEGER(1));
    nProtect += 1;
    pnLength = INTEGER_POINTER(spnLength);

    *pnErrorCode = LSgetStageAggScheme(pModel,NULL,pnLength);
    CHECK_ERRCODE;

    PROTECT(spanScheme = NEW_INTEGER(*pnLength));
    nProtect += 1;
    panScheme = INTEGER_POINTER(spanScheme);

    *pnErrorCode = LSgetStageAggScheme(pModel,panScheme,NULL);
    CHECK_ERRCODE;

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanScheme);
        SET_VECTOR_ELT(rList, 2, spnLength);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdeduceStages(SEXP      sModel,
                      SEXP      snMaxStage,
                      SEXP      spanRowStagesIn,
                      SEXP      spanColStagesIn,
                      SEXP      spanSparStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nMaxStage = Rf_asInteger(snMaxStage);
    int       *panRowStagesIn = INTEGER(spanRowStagesIn);
    int       *panColStagesIn = INTEGER(spanColStagesIn);
    int       *panSparStage = INTEGER(spanSparStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panRowStagesOut;
    SEXP      spanRowStagesOut = R_NilValue;
    int       *panColStagesOut;
    SEXP      spanColStagesOut = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","panRowStagseOut","panColStagesOut"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars, nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_VARS,&nVars);
    CHECK_ERRCODE;
    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_CONS,&nCons);
    CHECK_ERRCODE;

    *pnErrorCode = LSdeduceStages(pModel,nMaxStage,panRowStagesIn,panColStagesIn,panSparStage);
    CHECK_ERRCODE;

    PROTECT(spanRowStagesOut = NEW_INTEGER(nCons));
    nProtect += 1;
    panRowStagesOut = INTEGER_POINTER(spanRowStagesOut);
    memcpy(panRowStagesOut,panRowStagesIn,sizeof(int)*nCons);

    PROTECT(spanColStagesOut = NEW_INTEGER(nCons));
    nProtect += 1;
    panColStagesOut = INTEGER_POINTER(spanColStagesOut);
    memcpy(panColStagesOut,panColStagesIn,sizeof(int)*nVars);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanRowStagesOut);
        SET_VECTOR_ELT(rList, 2, spanColStagesOut);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsolveSP(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveSP(pModel,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;

}

SEXP rcLSsolveHS(SEXP      sModel,
                 SEXP      snSearchMethod)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nSearchMethod = Rf_asInteger(snSearchMethod);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveHS(pModel,nSearchMethod,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioObjective(SEXP      sModel,
                              SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdObj;
    SEXP      spdObj = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdObj"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdObj = NEW_NUMERIC(1));
    nProtect += 1;
    pdObj = NUMERIC_POINTER(spdObj);

    *pnErrorCode = LSgetScenarioObjective(pModel,jScenario,pdObj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdObj);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNodePrimalSolution(SEXP      sModel,
                               SEXP      sjScenario,
                               SEXP      siStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iStage = Rf_asInteger(siStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padX;
    SEXP      spadX = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padX"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_COLS_STAGE,iStage,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadX = NEW_NUMERIC(nVars));
    nProtect += 1;
    padX = NUMERIC_POINTER(spadX);

    *pnErrorCode = LSgetNodePrimalSolution(pModel,jScenario,iStage,padX);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadX);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNodeDualSolution(SEXP      sModel,
                             SEXP      sjScenario,
                             SEXP      siStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iStage = Rf_asInteger(siStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padY;
    SEXP      spadY = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padY"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_ROWS_STAGE,iStage,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadY = NEW_NUMERIC(nCons));
    nProtect += 1;
    padY = NUMERIC_POINTER(spadY);

    *pnErrorCode = LSgetNodeDualSolution(pModel,jScenario,iStage,padY);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadY);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNodeReducedCost(SEXP      sModel,
                            SEXP      sjScenario,
                            SEXP      siStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iStage = Rf_asInteger(siStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padX;
    SEXP      spadX = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padX"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_COLS_STAGE,iStage,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadX = NEW_NUMERIC(nVars));
    nProtect += 1;
    padX = NUMERIC_POINTER(spadX);

    *pnErrorCode = LSgetNodeReducedCost(pModel,jScenario,iStage,padX);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadX);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNodeSlacks(SEXP      sModel,
                       SEXP      sjScenario,
                       SEXP      siStage)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iStage = Rf_asInteger(siStage);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padY;
    SEXP      spadY = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padY"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_ROWS_STAGE,iStage,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadY = NEW_NUMERIC(nCons));
    nProtect += 1;
    padY = NUMERIC_POINTER(spadY);

    *pnErrorCode = LSgetNodeSlacks(pModel,jScenario,iStage,padY);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadY);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioPrimalSolution(SEXP      sModel,
                                   SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padX;
    SEXP      spadX = R_NilValue;
    double    *pdObj;
    SEXP      spdObj = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","padX","pdObj"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_COLS_CORE,jScenario,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadX = NEW_NUMERIC(nVars));
    nProtect += 1;
    padX = NUMERIC_POINTER(spadX);

    PROTECT(spdObj = NEW_NUMERIC(1));
    nProtect += 1;
    pdObj = NUMERIC_POINTER(spdObj);

    *pnErrorCode = LSgetScenarioPrimalSolution(pModel,jScenario,padX,pdObj);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadX);
        SET_VECTOR_ELT(rList, 2, spdObj);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioReducedCost(SEXP      sModel,
                                SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padX;
    SEXP      spadX = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padX"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nVars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_COLS_CORE,jScenario,&nVars);
    CHECK_ERRCODE;

    PROTECT(spadX = NEW_NUMERIC(nVars));
    nProtect += 1;
    padX = NUMERIC_POINTER(spadX);

    *pnErrorCode = LSgetScenarioReducedCost(pModel,jScenario,padX);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadX);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioDualSolution(SEXP      sModel,
                                 SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padY;
    SEXP      spadY = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padY"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_ROWS_CORE,jScenario,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadY = NEW_NUMERIC(nCons));
    nProtect += 1;
    padY = NUMERIC_POINTER(spadY);

    *pnErrorCode = LSgetScenarioDualSolution(pModel,jScenario,padY);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadY);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioSlacks(SEXP      sModel,
                           SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padY;
    SEXP      spadY = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","padY"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;
    int       nCons;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetStocInfo(pModel,LS_IINFO_STOC_NUM_ROWS_CORE,jScenario,&nCons);
    CHECK_ERRCODE;

    PROTECT(spadY = NEW_NUMERIC(nCons));
    nProtect += 1;
    padY = NUMERIC_POINTER(spadY);

    *pnErrorCode = LSgetScenarioSlacks(pModel,jScenario,padY);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadY);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetNodeListByScenario(SEXP      sModel,
                               SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *paiNodes;
    SEXP      spaiNodes = R_NilValue;
    int       *pnNodes;
    SEXP      spnNodes = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","paiNodes","pnNodes"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnNodes = NEW_INTEGER(1));
    nProtect += 1;
    pnNodes = INTEGER_POINTER(spnNodes);

    *pnErrorCode = LSgetNodeListByScenario(pModel,jScenario,NULL,pnNodes);
    CHECK_ERRCODE;

    PROTECT(spaiNodes = NEW_INTEGER(*pnNodes));
    nProtect += 1;
    paiNodes = INTEGER_POINTER(spaiNodes);

    *pnErrorCode = LSgetNodeListByScenario(pModel,jScenario,paiNodes,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spaiNodes);
        SET_VECTOR_ELT(rList, 2, spnNodes);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetProbabilityByScenario(SEXP      sModel,
                                  SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdProb;
    SEXP      spdProb = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdProb"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdProb = NEW_NUMERIC(1));
    nProtect += 1;
    pdProb = NUMERIC_POINTER(spdProb);

    *pnErrorCode = LSgetProbabilityByScenario(pModel,jScenario,pdProb);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdProb);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetProbabilityByNode(SEXP      sModel,
                              SEXP      siNode)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iNode = Rf_asInteger(siNode);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdProb;
    SEXP      spdProb = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdProb"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdProb = NEW_NUMERIC(1));
    nProtect += 1;
    pdProb = NUMERIC_POINTER(spdProb);

    *pnErrorCode = LSgetProbabilityByNode(pModel,iNode,pdProb);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdProb);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocParData(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *paiStages;
    SEXP      spaiStages = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","paiStages", "padVals"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nSpars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_SPARS,&nSpars);
    CHECK_ERRCODE;

    PROTECT(spaiStages = NEW_INTEGER(nSpars));
    nProtect += 1;
    paiStages = INTEGER_POINTER(spaiStages);

    PROTECT(spadVals = NEW_NUMERIC(nSpars));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    *pnErrorCode = LSgetStocParData(pModel,paiStages,padVals);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spaiStages);
        SET_VECTOR_ELT(rList, 2, spadVals);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddDiscreteBlocks(SEXP      sModel,
                           SEXP      siStage,
                           SEXP      snRealzBlock,
                           SEXP      spadProb,
                           SEXP      spakStart,
                           SEXP      spaiRows,
                           SEXP      spaiCols,
                           SEXP      spaiStvs,
                           SEXP      spadVals,
                           SEXP      snModifyRule)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iStage = Rf_asInteger(siStage);
    int       nRealzBlock = Rf_asInteger(snRealzBlock);
    double    *padProb = REAL(spadProb);
    int       *pakStart = INTEGER(spakStart);
    int       *paiRows = INTEGER(spaiRows);
    int       *paiCols = INTEGER(spaiCols);
    int       *paiStvs = INTEGER(spaiStvs);
    double    *padVals = REAL(spadVals);
    int       nModifyRule = Rf_asInteger(snModifyRule);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddDiscreteBlocks(pModel,
                                       iStage,
                                       nRealzBlock,
                                       padProb,
                                       pakStart,
                                       paiRows,
                                       paiCols,
                                       paiStvs,
                                       padVals,
                                       nModifyRule);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);


    return rList;
}

SEXP rcLSaddScenario(SEXP      sModel,
                     SEXP      sjScenario,
                     SEXP      siParentScen,
                     SEXP      siStage,
                     SEXP      sdProb,
                     SEXP      snElems,
                     SEXP      spaiRows,
                     SEXP      spaiCols,
                     SEXP      spaiStvs,
                     SEXP      spadVals,
                     SEXP      snModifyRule)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);
    int       iParentScen = Rf_asInteger(siParentScen);
    int       iStage = Rf_asInteger(siStage);
    double    dProb = Rf_asReal(sdProb);
    int       nElems = Rf_asInteger(snElems);
    int       *paiRows = INTEGER(spaiRows);
    int       *paiCols = INTEGER(spaiCols);
    int       *paiStvs = INTEGER(spaiStvs);
    double    *padVals = REAL(spadVals);
    int       nModifyRule = Rf_asInteger(snModifyRule);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddScenario(pModel,
                                 jScenario,
                                 iParentScen,
                                 iStage,
                                 dProb,
                                 nElems,
                                 paiRows,
                                 paiCols,
                                 paiStvs,
                                 padVals,
                                 nModifyRule);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddDiscreteIndep(SEXP      sModel,
                          SEXP      siRow,
                          SEXP      sjCol,
                          SEXP      siStv,
                          SEXP      snRealizations,
                          SEXP      spadProbs,
                          SEXP      spadVals,
                          SEXP      snModifyRule)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iRow = Rf_asInteger(siRow);
    int       jCol = Rf_asInteger(sjCol);
    int       iStv = Rf_asInteger(siStv);
    int       nRealizations = Rf_asInteger(snRealizations);
    double    *padProbs = REAL(spadProbs);
    double    *padVals = REAL(spadVals);
    int       nModifyRule = Rf_asInteger(snModifyRule);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddDiscreteIndep(pModel,
                                      iRow,
                                      jCol,
                                      iStv,
                                      nRealizations,
                                      padProbs,
                                      padVals,
                                      nModifyRule);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddParamDistIndep(SEXP      sModel,
                           SEXP      siRow,
                           SEXP      sjCol,
                           SEXP      siStv,
                           SEXP      snDistType,
                           SEXP      snParams,
                           SEXP      spadParams,
                           SEXP      siModifyRule)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iRow = Rf_asInteger(siRow);
    int       jCol = Rf_asInteger(sjCol);
    int       iStv = Rf_asInteger(siStv);
    int       nDistType = Rf_asInteger(snDistType);
    int       nParams = Rf_asInteger(snParams);
    double    *padParams = REAL(spadParams);
    int       iModifyRule = Rf_asInteger(siModifyRule);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddParamDistIndep(pModel,
                                       iRow,
                                       jCol,
                                       iStv,
                                       nDistType,
                                       nParams,
                                       padParams,
                                       iModifyRule);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSaddChanceConstraint(SEXP      sModel,
                             SEXP      siSense,
                             SEXP      snCons,
                             SEXP      spaiCons,
                             SEXP      sdPrLevel,
                             SEXP      sdObjWeight)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iSense = Rf_asInteger(siSense);
    int       nCons = Rf_asInteger(snCons);
    int       *paiCons = INTEGER(spaiCons);
    double    dPrLevel = Rf_asReal(sdPrLevel);
    double    dObjWeight = Rf_asReal(sdObjWeight);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSaddChanceConstraint(pModel,
                                         iSense,
                                         nCons,
                                         paiCons,
                                         dPrLevel,
                                         dObjWeight);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetNumStages(SEXP      sModel,
                      SEXP      snumStages)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       numStages = Rf_asInteger(snumStages);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetNumStages(pModel,
                                  numStages);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocParOutcomes(SEXP      sModel,
                            SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    double    *pdProb;
    SEXP      spdProb = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","padVals", "pdProb"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;
    int       nSpars;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSgetInfo(pModel,LS_IINFO_NUM_SPARS,&nSpars);
    CHECK_ERRCODE;

    PROTECT(spadVals = NEW_NUMERIC(nSpars));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    PROTECT(spdProb = NEW_NUMERIC(1));
    nProtect += 1;
    pdProb = NUMERIC_POINTER(spdProb);

    *pnErrorCode = LSgetStocParOutcomes(pModel,jScenario,padVals,pdProb);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spadVals);
        SET_VECTOR_ELT(rList, 2, spdProb);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSloadCorrelationMatrix(SEXP      sModel,
                               SEXP      snDim,
                               SEXP      snCorrType,
                               SEXP      snQCnnz,
                               SEXP      spaiQCcols1,
                               SEXP      spaiQCcols2,
                               SEXP      spadQCcoef)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nDim = Rf_asInteger(snDim);
    int       nCorrType = Rf_asInteger(snCorrType);
    int       nQCnnz = Rf_asInteger(snQCnnz);
    int       *paiQCcols1 = INTEGER(spaiQCcols1);
    int       *paiQCcols2 = INTEGER(spaiQCcols2);
    double    *padQCcoef = REAL(spadQCcoef);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSloadCorrelationMatrix(pModel,
                                           nDim,
                                           nCorrType,
                                           nQCnnz,
                                           paiQCcols1,
                                           paiQCcols2,
                                           padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetCorrelationMatrix(SEXP      sModel,
                              SEXP      siFlag,
                              SEXP      snCorrType)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iFlag = Rf_asInteger(siFlag);
    int       nCorrType = Rf_asInteger(snCorrType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnQCnnz;
    SEXP      spnQCnnz = R_NilValue;
    int       *paiQCcols1;
    SEXP      spaiQCcols1 = R_NilValue;
    int       *paiQCcols2;
    SEXP      spaiQCcols2 = R_NilValue;
    double    *padQCcoef;
    SEXP      spadQCcoef = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode","pnQCnnz","paiQCcols1","paiQCcols2","padQCcoef"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnQCnnz = NEW_INTEGER(1));
    nProtect += 1;
    pnQCnnz = INTEGER_POINTER(spnQCnnz);

    *pnErrorCode = LSgetCorrelationMatrix(pModel,iFlag,nCorrType,pnQCnnz,NULL,NULL,NULL);
    CHECK_ERRCODE;

    PROTECT(spaiQCcols1 = NEW_INTEGER(*pnQCnnz));
    nProtect += 1;
    paiQCcols1 = INTEGER_POINTER(spaiQCcols1);

    PROTECT(spaiQCcols2 = NEW_INTEGER(*pnQCnnz));
    nProtect += 1;
    paiQCcols2 = INTEGER_POINTER(spaiQCcols2);

    PROTECT(spadQCcoef = NEW_NUMERIC(*pnQCnnz));
    nProtect += 1;
    padQCcoef = NUMERIC_POINTER(spadQCcoef);

    *pnErrorCode = LSgetCorrelationMatrix(pModel,iFlag,nCorrType,NULL,
                                          paiQCcols1,paiQCcols2,padQCcoef);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnQCnnz);
        SET_VECTOR_ELT(rList, 2, spaiQCcols1);
        SET_VECTOR_ELT(rList, 3, spaiQCcols2);
        SET_VECTOR_ELT(rList, 4, spadQCcoef);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocParSample(SEXP      sModel,
                          SEXP      siStv,
                          SEXP      siRow,
                          SEXP      sjCol)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iStv = Rf_asInteger(siStv);
    int       iRow = Rf_asInteger(siRow);
    int       jCol = Rf_asInteger(sjCol);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    prLSsample prSample = NULL;
    SEXP       sprSample = R_NilValue;

    //errorcode item
    PROTECT(spnErrorCode = NEW_INTEGER(1));
    pnErrorCode = INTEGER_POINTER(spnErrorCode);

    CHECK_MODEL_ERROR;

    prSample = (prLSsample)malloc(sizeof(rLSsample)*1);
    if(prSample == NULL)
    {
        return R_NilValue;
    }

    prSample->pSample = LSgetStocParSample(pModel,iStv,iRow,jCol,pnErrorCode);
    CHECK_ERRCODE;

    sprSample = R_MakeExternalPtr(prSample,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sprSample,tagLSsample);

ErrorReturn:

    UNPROTECT(1);

    if(*pnErrorCode)
    {
        Rprintf("\nFail to get Sample object (error %d)\n",*pnErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }
    else
    {
        return sprSample;
    }
}

SEXP rcLSgetDiscreteBlocks(SEXP      sModel,
                           SEXP      siEvent)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iEvent = Rf_asInteger(siEvent);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *nDistType;
    SEXP      snDistType = R_NilValue;
    int       *iStage;
    SEXP      siStage = R_NilValue;
    int       *nRealzBlock;
    SEXP      snRealzBlock = R_NilValue;
    double    *padProbs;
    SEXP      spadProbs = R_NilValue;
    int       *iModifyRule;
    SEXP      siModifyRule = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[6] = {"ErrorCode","nDistType","iStage",
                           "nRealzBlock","padProbs","iModifyRule"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 6;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(snDistType = NEW_INTEGER(1));
    nProtect += 1;
    nDistType = INTEGER_POINTER(snDistType);

    PROTECT(siStage = NEW_INTEGER(1));
    nProtect += 1;
    iStage = INTEGER_POINTER(siStage);

    PROTECT(snRealzBlock = NEW_INTEGER(1));
    nProtect += 1;
    nRealzBlock = INTEGER_POINTER(snRealzBlock);

    PROTECT(siModifyRule = NEW_INTEGER(1));
    nProtect += 1;
    iModifyRule = INTEGER_POINTER(siModifyRule);

    *pnErrorCode = LSgetDiscreteBlocks(pModel,iEvent,nDistType,iStage,
                                       nRealzBlock,NULL,iModifyRule);
    CHECK_ERRCODE;

    PROTECT(spadProbs = NEW_NUMERIC(*nRealzBlock));
    nProtect += 1;
    padProbs = NUMERIC_POINTER(spadProbs);

    *pnErrorCode = LSgetDiscreteBlocks(pModel,iEvent,NULL,NULL,NULL,padProbs,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, snDistType);
        SET_VECTOR_ELT(rList, 2, siStage);
        SET_VECTOR_ELT(rList, 3, snRealzBlock);
        SET_VECTOR_ELT(rList, 4, spadProbs);
        SET_VECTOR_ELT(rList, 5, siModifyRule);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDiscreteBlockOutcomes(SEXP      sModel,
                                  SEXP      siEvent,
                                  SEXP      siRealz)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iEvent = Rf_asInteger(siEvent);
    int       iRealz = Rf_asInteger(siRealz);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *nRealz;
    SEXP      snRealz = R_NilValue;
    int       *paiArows;
    SEXP      spaiArows = R_NilValue;
    int       *paiAcols;
    SEXP      spaiAcols = R_NilValue;
    int       *paiStvs;
    SEXP      spaiStvs = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[6] = {"ErrorCode","nRealz","paiArows",
                           "paiAcols","paiStvs","padVals"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 6;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(snRealz = NEW_INTEGER(1));
    nProtect += 1;
    nRealz = INTEGER_POINTER(snRealz);

    *pnErrorCode = LSgetDiscreteBlockOutcomes(pModel,iEvent,iRealz,
                                              nRealz,NULL,NULL,NULL,NULL);
    CHECK_ERRCODE;

    PROTECT(spaiArows = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiArows = INTEGER_POINTER(spaiArows);

    PROTECT(spaiAcols = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiAcols = INTEGER_POINTER(spaiAcols);

    PROTECT(spaiStvs = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiStvs = INTEGER_POINTER(spaiStvs);

    PROTECT(spadVals = NEW_NUMERIC(*nRealz));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    *pnErrorCode = LSgetDiscreteBlockOutcomes(pModel,iEvent,iRealz,NULL,
                                              paiArows,paiAcols,paiStvs,padVals);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, snRealz);
        SET_VECTOR_ELT(rList, 2, spaiArows);
        SET_VECTOR_ELT(rList, 3, spaiAcols);
        SET_VECTOR_ELT(rList, 4, spaiStvs);
        SET_VECTOR_ELT(rList, 5, spadVals);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDiscreteIndep(SEXP      sModel,
                          SEXP      siEvent)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iEvent = Rf_asInteger(siEvent);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *nDistType;
    SEXP      snDistType = R_NilValue;
    int       *iStage;
    SEXP      siStage = R_NilValue;
    int       *iRow;
    SEXP      siRow = R_NilValue;
    int       *jCol;
    SEXP      sjCol = R_NilValue;
    int       *iStv;
    SEXP      siStv = R_NilValue;
    int       *nRealizations;
    SEXP      snRealizations = R_NilValue;
    double    *padProbs;
    SEXP      spadProbs = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    int       *iModifyRule;
    SEXP      siModifyRule = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[10] = {"ErrorCode","nDistType","iStage",
                            "iRow","jCol","iStv","nRealizations",
                            "padProbs","padVals","iModifyRule"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 10;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(snDistType = NEW_INTEGER(1));
    nProtect += 1;
    nDistType = INTEGER_POINTER(snDistType);

    PROTECT(siStage = NEW_INTEGER(1));
    nProtect += 1;
    iStage = INTEGER_POINTER(siStage);

    PROTECT(siRow = NEW_INTEGER(1));
    nProtect += 1;
    iRow = INTEGER_POINTER(siRow);

    PROTECT(sjCol = NEW_INTEGER(1));
    nProtect += 1;
    jCol = INTEGER_POINTER(sjCol);

    PROTECT(siStv = NEW_INTEGER(1));
    nProtect += 1;
    iStv = INTEGER_POINTER(siStv);

    PROTECT(snRealizations = NEW_INTEGER(1));
    nProtect += 1;
    nRealizations = INTEGER_POINTER(snRealizations);

    PROTECT(siModifyRule = NEW_INTEGER(1));
    nProtect += 1;
    iModifyRule = INTEGER_POINTER(siModifyRule);

    *pnErrorCode = LSgetDiscreteIndep(pModel,iEvent,nDistType,iStage,
                                      iRow,jCol,iStv,nRealizations,NULL,NULL,iModifyRule);
    CHECK_ERRCODE;

    PROTECT(spadProbs = NEW_NUMERIC(*nRealizations));
    nProtect += 1;
    padProbs = NUMERIC_POINTER(spadProbs);

    PROTECT(spadVals = NEW_NUMERIC(*nRealizations));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    *pnErrorCode = LSgetDiscreteIndep(pModel,iEvent,NULL,NULL,NULL,NULL,NULL,NULL,
                                      padProbs,padVals,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, snDistType);
        SET_VECTOR_ELT(rList, 2, siStage);
        SET_VECTOR_ELT(rList, 3, siRow);
        SET_VECTOR_ELT(rList, 4, sjCol);
        SET_VECTOR_ELT(rList, 5, siStv);
        SET_VECTOR_ELT(rList, 6, snRealizations);
        SET_VECTOR_ELT(rList, 7, spadProbs);
        SET_VECTOR_ELT(rList, 8, spadVals);
        SET_VECTOR_ELT(rList, 9, siModifyRule);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetParamDistIndep(SEXP      sModel,
                           SEXP      siEvent)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iEvent = Rf_asInteger(siEvent);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *nDistType;
    SEXP      snDistType = R_NilValue;
    int       *iStage;
    SEXP      siStage = R_NilValue;
    int       *iRow;
    SEXP      siRow = R_NilValue;
    int       *jCol;
    SEXP      sjCol = R_NilValue;
    int       *iStv;
    SEXP      siStv = R_NilValue;
    int       *nParams;
    SEXP      snParams = R_NilValue;
    double    *padParams;
    SEXP      spadParams = R_NilValue;
    int       *iModifyRule;
    SEXP      siModifyRule = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[9] = {"ErrorCode","nDistType","iStage",
                            "iRow","jCol","iStv","nParams",
                            "padParams","iModifyRule"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 9;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(snDistType = NEW_INTEGER(1));
    nProtect += 1;
    nDistType = INTEGER_POINTER(snDistType);

    PROTECT(siStage = NEW_INTEGER(1));
    nProtect += 1;
    iStage = INTEGER_POINTER(siStage);

    PROTECT(siRow = NEW_INTEGER(1));
    nProtect += 1;
    iRow = INTEGER_POINTER(siRow);

    PROTECT(sjCol = NEW_INTEGER(1));
    nProtect += 1;
    jCol = INTEGER_POINTER(sjCol);

    PROTECT(siStv = NEW_INTEGER(1));
    nProtect += 1;
    iStv = INTEGER_POINTER(siStv);

    PROTECT(snParams = NEW_INTEGER(1));
    nProtect += 1;
    nParams = INTEGER_POINTER(snParams);

    PROTECT(siModifyRule = NEW_INTEGER(1));
    nProtect += 1;
    iModifyRule = INTEGER_POINTER(siModifyRule);

    *pnErrorCode = LSgetParamDistIndep(pModel,iEvent,nDistType,iStage,
                                       iRow,jCol,iStv,nParams,NULL,iModifyRule);
    CHECK_ERRCODE;

    PROTECT(spadParams = NEW_NUMERIC(*nParams));
    nProtect += 1;
    padParams = NUMERIC_POINTER(spadParams);

    *pnErrorCode = LSgetParamDistIndep(pModel,iEvent,NULL,NULL,
                                       NULL,NULL,NULL,NULL,padParams,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, snDistType);
        SET_VECTOR_ELT(rList, 2, siStage);
        SET_VECTOR_ELT(rList, 3, siRow);
        SET_VECTOR_ELT(rList, 4, sjCol);
        SET_VECTOR_ELT(rList, 5, siStv);
        SET_VECTOR_ELT(rList, 6, snParams);
        SET_VECTOR_ELT(rList, 7, spadParams);
        SET_VECTOR_ELT(rList, 8, siModifyRule);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenario(SEXP      sModel,
                     SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *iParentScen;
    SEXP      siParentScen = R_NilValue;
    int       *iBranchStage;
    SEXP      siBranchStage = R_NilValue;
    double    *pdProb;
    SEXP      spdProb = R_NilValue;
    int       *nRealz;
    SEXP      snRealz = R_NilValue;
    int       *paiArows;
    SEXP      spaiArows = R_NilValue;
    int       *paiAcols;
    SEXP      spaiAcols = R_NilValue;
    int       *paiStvs;
    SEXP      spaiStvs = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    int       *iModifyRule;
    SEXP      siModifyRule = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[10] = {"ErrorCode","iParentScen","iBranchStage",
                            "pdProb","nRealz","paiArows","paiAcols",
                            "paiStvs","padVals","iModifyRule"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 10;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(siParentScen = NEW_INTEGER(1));
    nProtect += 1;
    iParentScen = INTEGER_POINTER(siParentScen);

    PROTECT(siBranchStage = NEW_INTEGER(1));
    nProtect += 1;
    iBranchStage = INTEGER_POINTER(siBranchStage);

    PROTECT(spdProb = NEW_NUMERIC(1));
    nProtect += 1;
    pdProb = NUMERIC_POINTER(spdProb);

    PROTECT(snRealz = NEW_INTEGER(1));
    nProtect += 1;
    nRealz = INTEGER_POINTER(snRealz);

    PROTECT(siModifyRule = NEW_INTEGER(1));
    nProtect += 1;
    iModifyRule = INTEGER_POINTER(siModifyRule);

    *pnErrorCode = LSgetScenario(pModel,jScenario,iParentScen,iBranchStage,pdProb,nRealz,
                                 NULL,NULL,NULL,NULL,iModifyRule);
    CHECK_ERRCODE;

    PROTECT(spaiArows = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiArows = INTEGER_POINTER(spaiArows);

    PROTECT(spaiAcols = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiAcols = INTEGER_POINTER(spaiAcols);

    PROTECT(spaiStvs = NEW_INTEGER(*nRealz));
    nProtect += 1;
    paiStvs = INTEGER_POINTER(spaiStvs);

    PROTECT(spadVals = NEW_NUMERIC(*nRealz));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    *pnErrorCode = LSgetScenario(pModel,jScenario,NULL,NULL,NULL,NULL,
                                 paiArows,paiAcols,paiStvs,padVals,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, siParentScen);
        SET_VECTOR_ELT(rList, 2, siBranchStage);
        SET_VECTOR_ELT(rList, 3, spdProb);
        SET_VECTOR_ELT(rList, 4, snRealz);
        SET_VECTOR_ELT(rList, 5, spaiArows);
        SET_VECTOR_ELT(rList, 6, spaiAcols);
        SET_VECTOR_ELT(rList, 7, spaiStvs);
        SET_VECTOR_ELT(rList, 8, spadVals);
        SET_VECTOR_ELT(rList, 9, siModifyRule);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetChanceConstraint(SEXP      sModel,
                             SEXP      siChance)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       iChance = Rf_asInteger(siChance);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *piSense;
    SEXP      spiSense = R_NilValue;
    int       *pnCons;
    SEXP      spnCons = R_NilValue;
    int       *paiCons;
    SEXP      spaiCons = R_NilValue;
    double    *pdProb;
    SEXP      spdProb = R_NilValue;
    double    *pdObjWeight;
    SEXP      spdObjWeight = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[6] = {"ErrorCode","piSense","pnCons",
                            "paiCons","pdProb","pdObjWeight"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 6;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spiSense = NEW_INTEGER(1));
    nProtect += 1;
    piSense = INTEGER_POINTER(spiSense);

    PROTECT(spnCons = NEW_INTEGER(1));
    nProtect += 1;
    pnCons = INTEGER_POINTER(spnCons);

    PROTECT(spdProb = NEW_NUMERIC(1));
    nProtect += 1;
    pdProb = NUMERIC_POINTER(spdProb);

    PROTECT(spdObjWeight = NEW_NUMERIC(1));
    nProtect += 1;
    pdObjWeight = NUMERIC_POINTER(spdObjWeight);

    *pnErrorCode = LSgetChanceConstraint(pModel,iChance,
                                         piSense,pnCons,NULL,
                                         pdProb,pdObjWeight);
    CHECK_ERRCODE;

    PROTECT(spaiCons = NEW_INTEGER(*pnCons));
    nProtect += 1;
    paiCons = INTEGER_POINTER(spaiCons);

    *pnErrorCode = LSgetChanceConstraint(pModel,iChance,
                                         NULL,NULL,paiCons,
                                         NULL,NULL);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spiSense);
        SET_VECTOR_ELT(rList, 2, spnCons);
        SET_VECTOR_ELT(rList, 3, spaiCons);
        SET_VECTOR_ELT(rList, 4, spdProb);
        SET_VECTOR_ELT(rList, 5, spdObjWeight);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetSampleSizes(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panSampleSize;
    SEXP      spanSampleSize = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","panSampleSize"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spanSampleSize = NEW_INTEGER(1));
    nProtect += 1;
    panSampleSize = INTEGER_POINTER(spanSampleSize);

    *pnErrorCode = LSgetSampleSizes(pModel,panSampleSize);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanSampleSize);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetConstraintStages(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panStage;
    SEXP      spanStage = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","panStage"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spanStage = NEW_INTEGER(1));
    nProtect += 1;
    panStage = INTEGER_POINTER(spanStage);

    *pnErrorCode = LSgetConstraintStages(pModel,panStage);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanStage);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetVariableStages(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *panStage;
    SEXP      spanStage = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","panStage"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spanStage = NEW_INTEGER(1));
    nProtect += 1;
    panStage = INTEGER_POINTER(spanStage);

    *pnErrorCode = LSgetVariableStages(pModel,panStage);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spanStage);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetStocRowIndices(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *paiSrows;
    SEXP      spaiSrows = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","paiSrows"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spaiSrows = NEW_INTEGER(1));
    nProtect += 1;
    paiSrows = INTEGER_POINTER(spaiSrows);

    *pnErrorCode = LSgetStocRowIndices(pModel,paiSrows);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spaiSrows);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetStocParRG(SEXP      sModel,
                      SEXP      siStv,
                      SEXP      siRow,
                      SEXP      sjCol,
                      SEXP      sRG)
{
    prLSmodel   prModel;
    pLSmodel    pModel;
    prLSrandGen prRG;
    pLSrandGen  pRG;
    int         iStv = Rf_asInteger(siStv);
    int         iRow = Rf_asInteger(siRow);
    int         jCol = Rf_asInteger(sjCol);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    if(sRG != R_NilValue && R_ExternalPtrTag(sRG) == tagLSrandGen)
    {
        prRG = (prLSrandGen)R_ExternalPtrAddr(sRG);
        pRG = prRG->pRG;
    }
    else
    {
        *pnErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        goto ErrorReturn;
    }

    *pnErrorCode = LSsetStocParRG(pModel,iStv,iRow,jCol,pRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetScenarioModel(SEXP      sModel,
                          SEXP      sjScenario)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       jScenario = Rf_asInteger(sjScenario);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    prLSmodel prdModel = NULL;
    SEXP      sprdModel = R_NilValue;
    int       nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    prdModel = (prLSmodel)malloc(sizeof(rLSmodel)*1);
    if(prdModel == NULL)
    {
        return R_NilValue;
    }

    prdModel->pModel = LSgetScenarioModel(pModel,jScenario,pnErrorCode);
    CHECK_ERRCODE;

    SET_PRINT_LOG(prdModel->pModel,*pnErrorCode);
    CHECK_ERRCODE;

    SET_MODEL_CALLBACK(prdModel->pModel,*pnErrorCode);
    CHECK_ERRCODE;

    sprdModel = R_MakeExternalPtr(prdModel,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sprdModel,tagLSprob);

ErrorReturn:

    UNPROTECT(nProtect);

    if(*pnErrorCode)
    {
        Rprintf("\nFail to get ScenarioModel (error %d)\n",*pnErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }
    else
    {
        return sprdModel;
    }
}

SEXP rcLSfreeStocMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeStocMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfreeStocHashMemory(SEXP      sModel)
{
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    LSfreeStocHashMemory(pModel);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelStocParameterInt(SEXP      sModel,
                                  SEXP      snQuery)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSgetModelStocParameter(pModel,nQuery,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelStocParameterDou(SEXP      sModel,
                                  SEXP      snQuery)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetModelStocParameter(pModel,nQuery,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetModelStocParameterChar(SEXP      sModel,
                                   SEXP      snQuery)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      pachResult[256];
    SEXP      spachResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pachResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spachResult = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetModelStocParameter(pModel,nQuery,pachResult);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachResult,0,mkChar(pachResult));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelStocParameterInt(SEXP       sModel,
                                  SEXP       snQuery,
                                  SEXP       spnResult)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    int       *pnResult = INTEGER(spnResult);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelStocParameter(pModel,
                                           nQuery,
                                           pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelStocParameterDou(SEXP       sModel,
                                  SEXP       snQuery,
                                  SEXP       spdResult)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    double    *pdResult = REAL(spdResult);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelStocParameter(pModel,
                                           nQuery,
                                           pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetModelStocParameterChar(SEXP       sModel,
                                   SEXP       snQuery,
                                   SEXP       spacResult)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nQuery = Rf_asInteger(snQuery);
    char      *pacResult = (char *)CHAR(STRING_ELT(spacResult,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    *pnErrorCode = LSsetModelStocParameter(pModel,
                                           nQuery,
                                           pacResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetEnvStocParameterInt(SEXP      sEnv,
                                SEXP      snQuery)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSgetEnvStocParameter(pEnv,nQuery,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetEnvStocParameterDou(SEXP      sEnv,
                                SEXP      snQuery)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetEnvStocParameter(pEnv,nQuery,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetEnvStocParameterChar(SEXP      sEnv,
                                 SEXP      snQuery)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    char      pachResult[256];
    SEXP      spachResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pachResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    PROTECT(spachResult = NEW_CHARACTER(1));
    nProtect += 1;

    *pnErrorCode = LSgetEnvStocParameter(pEnv,nQuery,pachResult);

    CHECK_ERRCODE;

    SET_STRING_ELT(spachResult,0,mkChar(pachResult));

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spachResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetEnvStocParameterInt(SEXP      sEnv,
                                SEXP      snQuery,
                                SEXP      spnResult)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);
    int       *pnResult = INTEGER(spnResult);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSsetEnvStocParameter(pEnv,
                                         nQuery,
                                         pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetEnvStocParameterDou(SEXP      sEnv,
                                SEXP      snQuery,
                                SEXP      spdResult)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);
    double    *pdResult = REAL(spdResult);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSsetEnvStocParameter(pEnv,
                                         nQuery,
                                         pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetEnvStocParameterChar(SEXP      sEnv,
                                 SEXP      snQuery,
                                 SEXP      spacResult)
{
    prLSenv   prEnv;
    pLSenv    pEnv;
    int       nQuery = Rf_asInteger(snQuery);
    char      *pacResult = (char *)CHAR(STRING_ELT(spacResult,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_ENV_ERROR;

    *pnErrorCode = LSsetEnvStocParameter(pEnv,
                                         nQuery,
                                         pacResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Statistical Calculations Interface (15)               *
*********************************************************/
SEXP rcLSsampCreate(SEXP sEnv,
                    SEXP snDistType)
{
    int         nErrorCode = LSERR_NO_ERROR;
    prLSenv     prEnv = NULL;
    pLSenv      pEnv = NULL;
    prLSsample  prSample = NULL;
    pLSsample   pSample = NULL;
    SEXP        sSample = R_NilValue;
    int         nDistType = Rf_asInteger(snDistType);

    if(sEnv != R_NilValue && R_ExternalPtrTag(sEnv) == tagLSenv)
    {
        prEnv = (prLSenv)R_ExternalPtrAddr(sEnv);
        pEnv = prEnv->pEnv;
    }
    else
    {
        nErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        Rprintf("Failed to create Sample object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prSample = (prLSsample)malloc(sizeof(rLSsample)*1);
    if(prSample == NULL)
    {
        return R_NilValue;
    }

    pSample = LSsampCreate(pEnv,nDistType,&nErrorCode);
    if(nErrorCode)
    {
        Rprintf("Failed to create Sample object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prSample->pSample = pSample;

    sSample = R_MakeExternalPtr(prSample,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sSample,tagLSsample);

    return sSample;
}

SEXP rcLSsampDelete(SEXP  sSample)
{
    prLSsample   prSample;
    pLSsample    pSample;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    *pnErrorCode = LSsampDelete(&pSample);
    CHECK_ERRCODE;

    prSample->pSample = NULL;

    R_ClearExternalPtr(sSample);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampSetDistrParam(SEXP  sSample,
                           SEXP  snIndex,
                           SEXP  sdValue)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nIndex = Rf_asInteger(snIndex);
    double       dValue = Rf_asReal(sdValue);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    *pnErrorCode = LSsampSetDistrParam(pSample,nIndex,dValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetDistrParam(SEXP  sSample,
                           SEXP  snIndex)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nIndex = Rf_asInteger(snIndex);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdValue;
    SEXP      spdValue = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdValue"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spdValue = NEW_NUMERIC(1));
    nProtect += 1;
    pdValue = NUMERIC_POINTER(spdValue);

    *pnErrorCode = LSsampGetDistrParam(pSample,nIndex,pdValue);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdValue);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampEvalDistr(SEXP  sSample,
                       SEXP  snFuncType,
                       SEXP  sdXval)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nFuncType = Rf_asInteger(snFuncType);
    double       dXval = Rf_asReal(sdXval);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSsampEvalDistr(pSample,nFuncType,dXval,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampEvalUserDistr(SEXP  sSample,
                           SEXP  snFuncType,
                           SEXP  spadXval,
                           SEXP  snDim)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nFuncType = Rf_asInteger(snFuncType);
    double       *padXval = REAL(spadXval);
    int          nDim = Rf_asInteger(snDim);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSsampEvalUserDistr(pSample,nFuncType,padXval,nDim,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampSetRG(SEXP  sSample,
                   SEXP  sRG)
{
    prLSsample    prSample;
    pLSsample     pSample;
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;
    CHECK_RG_ERROR;

    *pnErrorCode = LSsampSetRG(pSample,pRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGenerate(SEXP  sSample,
                      SEXP  snMethod,
                      SEXP  snSize)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nMethod = Rf_asInteger(snMethod);
    int          nSize = Rf_asInteger(snSize);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    *pnErrorCode = LSsampGenerate(pSample,nMethod,nSize);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetPoints(SEXP  sSample)
{
    prLSsample   prSample;
    pLSsample    pSample;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnSampSize;
    SEXP      spnSampSize = R_NilValue;
    double    *padXval;
    SEXP      spadXval = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","pnSampSize","padXval"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spnSampSize = NEW_INTEGER(1));
    nProtect += 1;
    pnSampSize = INTEGER_POINTER(spnSampSize);

    *pnErrorCode = LSsampGetPoints(pSample,pnSampSize,NULL);
    CHECK_ERRCODE;

    PROTECT(spadXval = NEW_NUMERIC(*pnSampSize));
    nProtect += 1;
    padXval = NUMERIC_POINTER(spadXval);

    *pnErrorCode = LSsampGetPoints(pSample,NULL,padXval);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnSampSize);
        SET_VECTOR_ELT(rList, 2, spadXval);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampLoadPoints(SEXP  sSample,
                        SEXP  snSampSize,
                        SEXP  spadXval)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nSampSize = Rf_asInteger(snSampSize);
    double       *padXval = REAL(spadXval);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    *pnErrorCode = LSsampLoadPoints(pSample,nSampSize,padXval);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetCIPoints(SEXP  sSample)
{
    prLSsample   prSample;
    pLSsample    pSample;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnSampSize;
    SEXP      spnSampSize = R_NilValue;
    double    *padXval;
    SEXP      spadXval = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[3] = {"ErrorCode","pnSampSize","padXval"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 3;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spnSampSize = NEW_INTEGER(1));
    nProtect += 1;
    pnSampSize = INTEGER_POINTER(spnSampSize);

    *pnErrorCode = LSsampGetCIPoints(pSample,pnSampSize,NULL);
    CHECK_ERRCODE;

    PROTECT(spadXval = NEW_NUMERIC(*pnSampSize));
    nProtect += 1;
    padXval = NUMERIC_POINTER(spadXval);

    *pnErrorCode = LSsampGetCIPoints(pSample,NULL,padXval);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnSampSize);
        SET_VECTOR_ELT(rList, 2, spadXval);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampLoadDiscretePdfTable(SEXP  sSample,
                                  SEXP  snLen,
                                  SEXP  spadProb,
                                  SEXP  spadVals)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nLen = Rf_asInteger(snLen);
    double       *padProb = REAL(spadProb);
    double       *padVals = REAL(spadVals);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    *pnErrorCode = LSsampLoadDiscretePdfTable(pSample,nLen,padProb,padVals);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetDiscretePdfTable(SEXP  sSample)
{
    prLSsample   prSample;
    pLSsample    pSample;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnLen;
    SEXP      spnLen = R_NilValue;
    double    *padProb;
    SEXP      spadProb = R_NilValue;
    double    *padVals;
    SEXP      spadVals = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[4] = {"ErrorCode","pnLen", "padProb","padVals"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 4;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spnLen = NEW_INTEGER(1));
    nProtect += 1;
    pnLen = INTEGER_POINTER(spnLen);

    *pnErrorCode = LSsampGetDiscretePdfTable(pSample,pnLen,NULL,NULL);
    CHECK_ERRCODE;

    PROTECT(spadProb = NEW_NUMERIC(*pnLen));
    nProtect += 1;
    padProb = NUMERIC_POINTER(spadProb);


    PROTECT(spadVals = NEW_NUMERIC(*pnLen));
    nProtect += 1;
    padVals = NUMERIC_POINTER(spadVals);

    *pnErrorCode = LSsampGetDiscretePdfTable(pSample,NULL,padProb,padVals);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnLen);
        SET_VECTOR_ELT(rList, 2, spadProb);
        SET_VECTOR_ELT(rList, 3, spadVals);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetIInfo(SEXP      sSample,
                      SEXP      snQuery)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnErrorCode = LSsampGetInfo(pSample,nQuery,pnResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsampGetDInfo(SEXP      sSample,
                      SEXP      snQuery)
{
    prLSsample   prSample;
    pLSsample    pSample;
    int          nQuery = Rf_asInteger(snQuery);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_SAMPLE_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSsampGetInfo(pSample,nQuery,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Random Number Generation Interface (12)               *
*********************************************************/
SEXP rcLScreateRG(SEXP sEnv,
                  SEXP snMethod)
{
    int          nErrorCode = LSERR_NO_ERROR;
    prLSenv      prEnv = NULL;
    pLSenv       pEnv = NULL;
    prLSrandGen  prRG = NULL;
    pLSrandGen   pRG = NULL;
    SEXP         sRG = R_NilValue;
    int          nMethod = Rf_asInteger(snMethod);

    if(sEnv != R_NilValue && R_ExternalPtrTag(sEnv) == tagLSenv)
    {
        prEnv = (prLSenv)R_ExternalPtrAddr(sEnv);
        pEnv = prEnv->pEnv;
    }
    else
    {
        nErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        Rprintf("Failed to create Random Generator object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prRG = (prLSrandGen)malloc(sizeof(rLSrandGen)*1);
    if(prRG == NULL)
    {
        return R_NilValue;
    }

    pRG = LScreateRG(pEnv,nMethod);

    prRG->pRG = pRG;

    sRG = R_MakeExternalPtr(prRG,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sRG,tagLSrandGen);

    return sRG;
}

SEXP rcLScreateRGMT(SEXP sEnv,
                    SEXP snMethod)
{
    int          nErrorCode = LSERR_NO_ERROR;
    prLSenv      prEnv = NULL;
    pLSenv       pEnv = NULL;
    prLSrandGen  prRG = NULL;
    pLSrandGen   pRG = NULL;
    SEXP         sRG = R_NilValue;
    int          nMethod = Rf_asInteger(snMethod);

    if(sEnv != R_NilValue && R_ExternalPtrTag(sEnv) == tagLSenv)
    {
        prEnv = (prLSenv)R_ExternalPtrAddr(sEnv);
        pEnv = prEnv->pEnv;
    }
    else
    {
        nErrorCode = LSERR_ILLEGAL_NULL_POINTER;
        Rprintf("Failed to create Random Generator object (error %d)\n",nErrorCode);
        R_FlushConsole();
        return R_NilValue;
    }

    prRG = (prLSrandGen)malloc(sizeof(rLSrandGen)*1);
    if(prRG == NULL)
    {
        return R_NilValue;
    }

    pRG = LScreateRGMT(pEnv,nMethod);

    prRG->pRG = pRG;

    sRG = R_MakeExternalPtr(prRG,R_NilValue,R_NilValue);

    R_SetExternalPtrTag(sRG,tagLSrandGen);

    return sRG;
}

SEXP rcLSgetDoubleRV(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pdResult = LSgetDoubleRV(pRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetInt32RV(SEXP   sRG,
                    SEXP   siLow,
                    SEXP   siHigh)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;
    int           iLow = Rf_asInteger(siLow);
    int           iHigh = Rf_asInteger(siHigh);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnResult = LSgetInt32RV(pRG,iLow,iHigh);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetRGSeed(SEXP   sRG,
                   SEXP   snSeed)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;
    int           nSeed = Rf_asInteger(snSeed);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    LSsetRGSeed(pRG,nSeed);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSdisposeRG(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    LSdisposeRG(&pRG);

    prRG->pRG = NULL;

    R_ClearExternalPtr(sRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetDistrParamRG(SEXP   sRG,
                         SEXP   siParam,
                         SEXP   sdParam)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;
    int           iParam = Rf_asInteger(siParam);
    double        dParam = Rf_asReal(sdParam);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    *pnErrorCode = LSsetDistrParamRG(pRG,iParam,dParam);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSsetDistrRG(SEXP   sRG,
                    SEXP   snDistType)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;
    int           nDistType = Rf_asInteger(snDistType);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    *pnErrorCode = LSsetDistrRG(pRG,nDistType);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetDistrRV(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    double    *pdResult;
    SEXP      spdResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pdResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    PROTECT(spdResult = NEW_NUMERIC(1));
    nProtect += 1;
    pdResult = NUMERIC_POINTER(spdResult);

    *pnErrorCode = LSgetDistrRV(pRG,pdResult);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spdResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetInitSeed(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnResult;
    SEXP      spnResult = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnResult"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    PROTECT(spnResult = NEW_INTEGER(1));
    nProtect += 1;
    pnResult = INTEGER_POINTER(spnResult);

    *pnResult = LSgetInitSeed(pRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnResult);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSgetRGNumThreads(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnThreads;
    SEXP      spnThreads = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode","pnThreads"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    PROTECT(spnThreads = NEW_INTEGER(1));
    nProtect += 1;
    pnThreads = INTEGER_POINTER(spnThreads);

    *pnErrorCode = LSgetRGNumThreads(pRG,pnThreads);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnThreads);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP rcLSfillRGBuffer(SEXP   sRG)
{
    prLSrandGen   prRG;
    pLSrandGen    pRG;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_RG_ERROR;

    *pnErrorCode = LSfillRGBuffer(pRG);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Sprint Interface (1)                                  *
*********************************************************/
SEXP rcLSsolveFileLP(SEXP      sModel,
                     SEXP      sszFileNameMPS,
                     SEXP      sszFileNameSol,
                     SEXP      snNoOfColsEvaluatedPerSet,
                     SEXP      snNoOfColsSelectedPerSet,
                     SEXP      snTimeLimitSec)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    char      *szFileNameMPS = (char *)CHAR(STRING_ELT(sszFileNameMPS,0));
    char      *szFileNameSol = (char *)CHAR(STRING_ELT(sszFileNameSol,0));
    int       nNoOfColsEvaluatedPerSet = Rf_asInteger(snNoOfColsEvaluatedPerSet);
    int       nNoOfColsSelectedPerSet = Rf_asInteger(snNoOfColsSelectedPerSet);
    int       nTimeLimitSec = Rf_asInteger(snTimeLimitSec);

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnSolStatusParam;
    SEXP      spnSolStatusParam = R_NilValue;
    int       *pnNoOfConsMps;
    SEXP      spnNoOfConsMps = R_NilValue;
    long long *pnNoOfColsMps;
    SEXP      spnNoOfColsMps = R_NilValue;
    long long *pnErrorLine;
    SEXP      spnErrorLine = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[5] = {"ErrorCode", "pnSolStatusParam", "pnNoOfConsMps",
                           "pnNoOfColsMps","pnErrorLine"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 5;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnSolStatusParam = NEW_INTEGER(1));
    nProtect += 1;
    pnSolStatusParam = INTEGER_POINTER(spnSolStatusParam);

    PROTECT(spnNoOfConsMps = NEW_INTEGER(1));
    nProtect += 1;
    pnNoOfConsMps = INTEGER_POINTER(spnNoOfConsMps);

    PROTECT(spnNoOfColsMps = NEW_INTEGER(1));
    nProtect += 1;
    pnNoOfColsMps = (long long *)INTEGER_POINTER(spnNoOfColsMps);

    PROTECT(spnErrorLine = NEW_INTEGER(1));
    nProtect += 1;
    pnErrorLine = (long long *)INTEGER_POINTER(spnErrorLine);

    *pnErrorCode = LSsolveFileLP(pModel,
                                 szFileNameMPS,
                                 szFileNameSol,
                                 nNoOfColsEvaluatedPerSet,
                                 nNoOfColsSelectedPerSet,
                                 nTimeLimitSec,
                                 pnSolStatusParam,
                                 pnNoOfConsMps,
                                 pnNoOfColsMps,
                                 pnErrorLine);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnSolStatusParam);
        SET_VECTOR_ELT(rList, 2, spnNoOfConsMps);
        SET_VECTOR_ELT(rList, 3, spnNoOfColsMps);
        SET_VECTOR_ELT(rList, 4, spnErrorLine);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}

/********************************************************
* Branch and price (1)                                  *
*********************************************************/
SEXP rcLSsolveMipBnp(SEXP      sModel,
                     SEXP      snBlock,
                     SEXP      spszFname)
{
    prLSmodel prModel;
    pLSmodel  pModel;
    int       nBlock = Rf_asInteger(snBlock);
    char      *pszFname = (char *)CHAR(STRING_ELT(spszFname,0));

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    int       *pnStatus;
    SEXP      spnStatus = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[2] = {"ErrorCode", "pnStatus"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 2;
    int       nIdx, nProtect = 0;

    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    PROTECT(spnStatus = NEW_INTEGER(1));
    nProtect += 1;
    pnStatus = INTEGER_POINTER(spnStatus);

    *pnErrorCode = LSsolveMipBnp(pModel,nBlock,pszFname,pnStatus);

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    if(*pnErrorCode != LSERR_NO_ERROR)
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
    }
    else
    {
        SET_VECTOR_ELT(rList, 0, spnErrorCode);
        SET_VECTOR_ELT(rList, 1, spnStatus);
    }

    UNPROTECT(nProtect + 2);

    return rList;
}


/********************************************************
* For Internal Use (1)                                  *
*********************************************************/
SEXP rcLSgetVersionInfo()
{
    char ver_num[255];
    char build_date[255];

    LSgetVersionInfo(ver_num,build_date);

    Rprintf("\nrLindo package for LINDO API %s, built on %s\n\n",ver_num,build_date);

    return R_NilValue;
}

/*
 * @brief LSwriteVarPriorities
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @param[in,out] nMode
 * @return int An integer error code
 * @remark errorcode = rLSwriteVarPriorities(pModel,pszFname,nMode)
 */
SEXP rcLSwriteVarPriorities(SEXP spModel,SEXP spszFname,SEXP snMode) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname;
  int nMode;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
  ibuf[3] = nMode = Rf_asInteger(snMode);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSwriteVarPriorities(pModel
    ,sbuf[2] //*pszFname
    ,ibuf[3]); //nMode


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSaddObjPool
 * @param[in,out] pModel
 * @param[in,out] padC
 * @param[in,out] mObjSense
 * @param[in,out] mRank
 * @param[in,out] dRelOptTol
 * @return int An integer error code
 * @remark errorcode = rLSaddObjPool(pModel,padC,mObjSense,mRank,dRelOptTol)
 */
SEXP rcLSaddObjPool(SEXP spModel,SEXP spadC,SEXP smObjSense,SEXP smRank,SEXP sdRelOptTol) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *padC;
  int mObjSense;
  int mRank;
  double dRelOptTol;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  MAKE_REAL_ARRAY(padC,spadC);
  ibuf[3] = mObjSense = Rf_asInteger(smObjSense);
  ibuf[4] = mRank = Rf_asInteger(smRank);
  dbuf[5] = dRelOptTol = Rf_asReal(sdRelOptTol);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  dvecptr[2] = (double*)padC;

  *pnErrorCode = errorcode = LSaddObjPool(pModel
    ,dvecptr[2] //*padC
    ,ibuf[3] //mObjSense
    ,ibuf[4] //mRank
    ,dbuf[5]); //dRelOptTol


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSwriteTunerConfigString
 * @param[in,out] pEnv
 * @param[in,out] szJsonString
 * @param[in,out] szJsonFile
 * @return int An integer error code
 * @remark errorcode = rLSwriteTunerConfigString(pEnv,szJsonString,szJsonFile)
 */
SEXP rcLSwriteTunerConfigString(SEXP spEnv,SEXP sszJsonString,SEXP sszJsonFile) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szJsonString;
  char *szJsonFile;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szJsonString = (char *) CHAR(STRING_ELT(sszJsonString,0));
  sbuf[3] = szJsonFile = (char *) CHAR(STRING_ELT(sszJsonFile,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSwriteTunerConfigString(pEnv
    ,sbuf[2] //*szJsonString
    ,sbuf[3]); //*szJsonFile


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSwriteTunerParameters
 * @param[in,out] pEnv
 * @param[in,out] szFile
 * @param[in,out] jInstance
 * @param[in,out] mCriterion
 * @return int An integer error code
 * @remark errorcode = rLSwriteTunerParameters(pEnv,szFile,jInstance,mCriterion)
 */
SEXP rcLSwriteTunerParameters(SEXP spEnv,SEXP sszFile,SEXP sjInstance,SEXP smCriterion) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szFile;
  int jInstance;
  int mCriterion;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szFile = (char *) CHAR(STRING_ELT(sszFile,0));
  ibuf[3] = jInstance = Rf_asInteger(sjInstance);
  ibuf[4] = mCriterion = Rf_asInteger(smCriterion);

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSwriteTunerParameters(pEnv
    ,sbuf[2] //*szFile
    ,ibuf[3] //jInstance
    ,ibuf[4]); //mCriterion


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSwriteParameterAsciiDoc
 * @param[in,out] pEnv
 * @param[in,out] pszFileName
 * @return int An integer error code
 * @remark errorcode = rLSwriteParameterAsciiDoc(pEnv,pszFileName)
 */
SEXP rcLSwriteParameterAsciiDoc(SEXP spEnv,SEXP spszFileName) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *pszFileName;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFileName = (char *) CHAR(STRING_ELT(spszFileName,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSwriteParameterAsciiDoc(pEnv
    ,sbuf[2]); //*pszFileName


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSaddTunerInstance
 * @param[in,out] pEnv
 * @param[in,out] szFile
 * @return int An integer error code
 * @remark errorcode = rLSaddTunerInstance(pEnv,szFile)
 */
SEXP rcLSaddTunerInstance(SEXP spEnv,SEXP sszFile) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szFile;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szFile = (char *) CHAR(STRING_ELT(sszFile,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSaddTunerInstance(pEnv
    ,sbuf[2]); //*szFile


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSaddTunerOption
 * @param[in,out] pEnv
 * @param[in,out] szKey
 * @param[in,out] dValue
 * @return int An integer error code
 * @remark errorcode = rLSaddTunerOption(pEnv,szKey,dValue)
 */
SEXP rcLSaddTunerOption(SEXP spEnv,SEXP sszKey,SEXP sdValue) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szKey;
  double dValue;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szKey = (char *) CHAR(STRING_ELT(sszKey,0));
  dbuf[3] = dValue = Rf_asReal(sdValue);

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSaddTunerOption(pEnv
    ,sbuf[2] //*szKey
    ,dbuf[3]); //dValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSaddTunerStrOption
 * @param[in,out] pEnv
 * @param[in,out] szKey
 * @param[in,out] szValue
 * @return int An integer error code
 * @remark errorcode = rLSaddTunerStrOption(pEnv,szKey,szValue)
 */
SEXP rcLSaddTunerStrOption(SEXP spEnv,SEXP sszKey,SEXP sszValue) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szKey;
  char *szValue;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szKey = (char *) CHAR(STRING_ELT(sszKey,0));
  sbuf[3] = szValue = (char *) CHAR(STRING_ELT(sszValue,1));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSaddTunerStrOption(pEnv
    ,sbuf[2] //*szKey
    ,sbuf[3]); //*szValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSaddTunerZDynamic
 * @param[in,out] pEnv
 * @param[in,out] iParam
 * @return int An integer error code
 * @remark errorcode = rLSaddTunerZDynamic(pEnv,iParam)
 */
SEXP rcLSaddTunerZDynamic(SEXP spEnv,SEXP siParam) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  int iParam;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = iParam = Rf_asInteger(siParam);

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSaddTunerZDynamic(pEnv
    ,ibuf[2]); //iParam


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSaddTunerZStatic
 * @param[in,out] pEnv
 * @param[in,out] jGroupId
 * @param[in,out] iParam
 * @param[in,out] dValue
 * @return int An integer error code
 * @remark errorcode = rLSaddTunerZStatic(pEnv,jGroupId,iParam,dValue)
 */
SEXP rcLSaddTunerZStatic(SEXP spEnv,SEXP sjGroupId,SEXP siParam,SEXP sdValue) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  int jGroupId;
  int iParam;
  double dValue;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = jGroupId = Rf_asInteger(sjGroupId);
  ibuf[3] = iParam = Rf_asInteger(siParam);
  dbuf[4] = dValue = Rf_asReal(sdValue);

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSaddTunerZStatic(pEnv
    ,ibuf[2] //jGroupId
    ,ibuf[3] //iParam
    ,dbuf[4]); //dValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSclearTuner
 * @param[in,out] pEnv
 * @return int An integer error code
 * @remark errorcode = rLSclearTuner(pEnv)
 */
SEXP rcLSclearTuner(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;



  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSclearTuner(pEnv); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSdisplayTunerResults
 * @param[in,out] pEnv
 * @return int An integer error code
 * @remark errorcode = rLSdisplayTunerResults(pEnv)
 */
SEXP rcLSdisplayTunerResults(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;



  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSdisplayTunerResults(pEnv); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetTunerConfigString
 * @param[in,out] pEnv
 * @param[in,out] pszJsonString
 * @return int An integer error code
 * @remark errorcode,spszJsonString = rLSgetTunerConfigString(pEnv)
 */
SEXP rcLSgetTunerConfigString(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char      *pszJsonString;
  SEXP      spszJsonString = R_NilValue;
  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","spszJsonString"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSgetTunerConfigString(pEnv
    ,&sbuf[2]); //**pszJsonString
  CHECK_ERRCODE;

  PROTECT(spszJsonString = NEW_CHARACTER(1));
  nProtect += 1;
  pszJsonString = sbuf[2];

  SET_STRING_ELT(spszJsonString,0,mkChar(pszJsonString));
ErrorReturn:
  if (sbuf[2]) free(sbuf[2]);
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spszJsonString);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetTunerOption
 * @param[in,out] pEnv
 * @param[in,out] szkey
 * @param[in,out] pdval
 * @return int An integer error code
 * @remark errorcode = rLSgetTunerOption(pEnv,szkey)
 */
SEXP rcLSgetTunerOption(SEXP spEnv,SEXP sszkey) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szkey;
  double *pdval;

  SEXP      spdval = R_NilValue;
  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pdval"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szkey = (char *) CHAR(STRING_ELT(sszkey,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  PROTECT(spdval = NEW_NUMERIC(1));
  nProtect += 1;
  dvecptr[3] = pdval = NUMERIC_POINTER(spdval); //pdval

  *pnErrorCode = errorcode = LSgetTunerOption(pEnv
    ,sbuf[2] //*szkey
    ,dvecptr[3]); //*pdval


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spdval);

  UNPROTECT(nProtect + 2);
  return rList;

}


/*
 * @brief LSgetTunerResult
 * @param[in,out] pEnv
 * @param[in,out] szkey
 * @param[in,out] jInstance
 * @param[in,out] kConfig
 * @param[in,out] pdval
 * @return int An integer error code
 * @remark errorcode = rLSgetTunerResult(pEnv,szkey,jInstance,kConfig)
 */
SEXP rcLSgetTunerResult(SEXP spEnv,SEXP sszkey,SEXP sjInstance,SEXP skConfig) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szkey;
  int jInstance;
  int kConfig;
  double *pdval;

  SEXP      spdval = R_NilValue;
  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pdval"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szkey = (char *) CHAR(STRING_ELT(sszkey,0));
  ibuf[3] = jInstance = Rf_asInteger(sjInstance);
  ibuf[4] = kConfig = Rf_asInteger(skConfig);

  CHECK_ENV_ERROR;

  // Get C pointers
  PROTECT(spdval = NEW_NUMERIC(1));
  nProtect += 1;
  dvecptr[5] = pdval = NUMERIC_POINTER(spdval); //pdval

  *pnErrorCode = errorcode = LSgetTunerResult(pEnv
    ,sbuf[2] //*szkey
    ,ibuf[3] //jInstance
    ,ibuf[4] //kConfig
    ,dvecptr[5]); //*pdval


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spdval);

  UNPROTECT(nProtect + 2);
  return rList;

}


SEXP rcLSgetTunerSpace(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  int *panParamId;
  int *numParam;

  SEXP      spanParamId = R_NilValue, snumParam = R_NilValue;
  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[3] = {"ErrorCode","panParamId","numParam"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 3;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  CHECK_ENV_ERROR;

  PROTECT(snumParam = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = numParam = INTEGER_POINTER(snumParam); //numParam

  *pnErrorCode = errorcode = LSgetTunerSpace(pEnv
    ,NULL //*panParamId
    ,ivecptr[3]); //*numParam

  // Get C pointers
  PROTECT(spanParamId = NEW_INTEGER(*numParam));
  nProtect += 1;
  ivecptr[2] = panParamId = INTEGER_POINTER(spanParamId); //panParamId

  *pnErrorCode = errorcode = LSgetTunerSpace(pEnv
    ,ivecptr[2] //*panParamId
    ,ivecptr[3]); //*numParam


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spanParamId);
  SET_VECTOR_ELT(rList, 2, snumParam);

  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSgetTunerStrOption
 * @param[in,out] pEnv
 * @param[in,out] szkey
 * @param[in,out] szval
 * @return int An integer error code
 * @remark errorcode = rLSgetTunerStrOption(pEnv,szkey)
 */
SEXP rcLSgetTunerStrOption(SEXP spEnv,SEXP sszkey) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szkey;
  char szval[255];

  SEXP      sszval = R_NilValue;
  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","sszval"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szkey = (char *) CHAR(STRING_ELT(sszkey,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSgetTunerStrOption(pEnv
    ,sbuf[2] //*szkey
    ,szval); //*szval

  SET_STRING_ELT(sszval,0,mkChar(szval));

ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, sszval);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSloadTunerConfigFile
 * @param[in,out] pEnv
 * @param[in,out] szJsonFile
 * @return int An integer error code
 * @remark errorcode = rLSloadTunerConfigFile(pEnv,szJsonFile)
 */
SEXP rcLSloadTunerConfigFile(SEXP spEnv,SEXP sszJsonFile) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szJsonFile;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szJsonFile = (char *) CHAR(STRING_ELT(sszJsonFile,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSloadTunerConfigFile(pEnv
    ,sbuf[2]); //*szJsonFile


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSloadTunerConfigString
 * @param[in,out] pEnv
 * @param[in,out] szJsonString
 * @return int An integer error code
 * @remark errorcode = rLSloadTunerConfigString(pEnv,szJsonString)
 */
SEXP rcLSloadTunerConfigString(SEXP spEnv,SEXP sszJsonString) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szJsonString;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szJsonString = (char *) CHAR(STRING_ELT(sszJsonString,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSloadTunerConfigString(pEnv
    ,sbuf[2]); //*szJsonString


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSprintTuner
 * @param[in,out] pEnv
 * @return int An integer error code
 * @remark errorcode = rLSprintTuner(pEnv)
 */
SEXP rcLSprintTuner(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;



  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSprintTuner(pEnv); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSresetTuner
 * @param[in,out] pEnv
 * @return int An integer error code
 * @remark errorcode = rLSresetTuner(pEnv)
 */
SEXP rcLSresetTuner(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;



  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSresetTuner(pEnv); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSrunTuner
 * @param[in,out] pEnv
 * @return int An integer error code
 * @remark errorcode = rLSrunTuner(pEnv)
 */
SEXP rcLSrunTuner(SEXP spEnv) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;



  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSrunTuner(pEnv); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSrunTunerFile
 * @param[in,out] pEnv
 * @param[in,out] szJsonFile
 * @return int An integer error code
 * @remark errorcode = rLSrunTunerFile(pEnv,szJsonFile)
 */
SEXP rcLSrunTunerFile(SEXP spEnv,SEXP sszJsonFile) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szJsonFile;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szJsonFile = (char *) CHAR(STRING_ELT(sszJsonFile,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSrunTunerFile(pEnv
    ,sbuf[2]); //*szJsonFile


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSrunTunerString
 * @param[in,out] pEnv
 * @param[in,out] szJsonString
 * @return int An integer error code
 * @remark errorcode = rLSrunTunerString(pEnv,szJsonString)
 */
SEXP rcLSrunTunerString(SEXP spEnv,SEXP sszJsonString) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szJsonString;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szJsonString = (char *) CHAR(STRING_ELT(sszJsonString,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSrunTunerString(pEnv
    ,sbuf[2]); //*szJsonString


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSsetTunerOption
 * @param[in,out] pEnv
 * @param[in,out] szKey
 * @param[in,out] dval
 * @return int An integer error code
 * @remark errorcode = rLSsetTunerOption(pEnv,szKey,dval)
 */
SEXP rcLSsetTunerOption(SEXP spEnv,SEXP sszKey,SEXP sdval) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szKey;
  double dval;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szKey = (char *) CHAR(STRING_ELT(sszKey,0));
  dbuf[3] = dval = Rf_asReal(sdval);

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSsetTunerOption(pEnv
    ,sbuf[2] //*szKey
    ,dbuf[3]); //dval


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSsetTunerStrOption
 * @param[in,out] pEnv
 * @param[in,out] szKey
 * @param[in,out] szval
 * @return int An integer error code
 * @remark errorcode = rLSsetTunerStrOption(pEnv,szKey,szval)
 */
SEXP rcLSsetTunerStrOption(SEXP spEnv,SEXP sszKey,SEXP sszval) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *szKey;
  char *szval;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = szKey = (char *) CHAR(STRING_ELT(sszKey,0));
  sbuf[3] = szval = (char *) CHAR(STRING_ELT(sszval,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSsetTunerStrOption(pEnv
    ,sbuf[2] //*szKey
    ,sbuf[3]); //*szval


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSwriteMPXFile
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @param[in,out] mMask
 * @return int An integer error code
 * @remark errorcode = rLSwriteMPXFile(pModel,pszFname,mMask)
 */
SEXP rcLSwriteMPXFile(SEXP spModel,SEXP spszFname,SEXP smMask) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname;
  int mMask;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
  ibuf[3] = mMask = Rf_asInteger(smMask);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSwriteMPXFile(pModel
    ,sbuf[2] //*pszFname
    ,ibuf[3]); //mMask


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSapplyLtf
 * @param[in,out] pModel
 * @param[in,out] panNewColIdx
 * @param[in,out] panNewRowIdx
 * @param[in,out] panNewColPos
 * @param[in,out] panNewRowPos
 * @param[in,out] nMode
 * @return int An integer error code
 * @remark errorcode = rLSapplyLtf(pModel,panNewColIdx,panNewRowIdx,panNewColPos,panNewRowPos,nMode)
 */
SEXP rcLSapplyLtf(SEXP spModel,SEXP spanNewColIdx,SEXP spanNewRowIdx,SEXP spanNewColPos,SEXP spanNewRowPos,SEXP snMode) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *panNewColIdx= NULL;
  int *panNewRowIdx= NULL;
  int *panNewColPos= NULL;
  int *panNewRowPos= NULL;
  int nMode;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  MAKE_INT_ARRAY(panNewColIdx,spanNewColIdx);
  MAKE_INT_ARRAY(panNewRowIdx,spanNewRowIdx);
  MAKE_INT_ARRAY(panNewColPos,spanNewColPos);
  MAKE_INT_ARRAY(panNewRowPos,spanNewRowPos);
  ibuf[6] = nMode = Rf_asInteger(snMode);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  // Get C pointers
  ivecptr[2] = (int*)panNewColIdx;

  ivecptr[3] = (int*)panNewRowIdx;

  ivecptr[4] = (int*)panNewColPos;

  ivecptr[5] = (int*)panNewRowPos;

  *pnErrorCode = errorcode = LSapplyLtf(pModel
    ,ivecptr[2] //*panNewColIdx
    ,ivecptr[3] //*panNewRowIdx
    ,ivecptr[4] //*panNewColPos
    ,ivecptr[5] //*panNewRowPos
    ,ibuf[6]); //nMode


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}


/*
 * @brief LSbnbSolve
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark errorcode = rLSbnbSolve(pModel,pszFname)
 */
SEXP rcLSbnbSolve(SEXP spModel,SEXP spszFname) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSbnbSolve(pModel
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);

  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LScalcConFunc
 * @param[in,out] pModel
 * @param[in,out] iRow
 * @param[in,out] padPrimal
 * @param[in,out] padSlacks
 * @return int An integer error code
 * @remark errorcode,spadSlacks = rLScalcConFunc(pModel,iRow,padPrimal,padSlacks)
 */
SEXP rcLScalcConFunc(SEXP spModel,SEXP siRow,SEXP spadPrimal) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int iRow;
  double *padPrimal= NULL;
  double *padSlacks= NULL;

  SEXP      spadSlacks = R_NilValue;
  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","padSlacks"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = iRow = Rf_asInteger(siRow);
  MAKE_REAL_ARRAY(padPrimal,spadPrimal);
  dvecptr[3] = padPrimal; //padPrimal

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);


  PROTECT(spadSlacks = NEW_NUMERIC(m));
  nProtect += 1;
  dvecptr[4] = padSlacks = NUMERIC_POINTER(spadSlacks); //padSlacks

  *pnErrorCode = errorcode = LScalcConFunc(pModel
    ,ibuf[2] //iRow
    ,dvecptr[3] //*padPrimal
    ,dvecptr[4]); //*padSlacks


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spadSlacks);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


/*
 * @brief LScalcConGrad
 * @param[in,out] pModel
 * @param[in,out] irow
 * @param[in,out] padPrimal
 * @param[in,out] nParList
 * @param[in,out] paiParList
 * @param[in,out] padParGrad
 * @return int An integer error code
 * @remark errorcode,padParGrad = rLScalcConGrad(pModel,irow,padPrimal,nParList,paiParList)
 */
SEXP rcLScalcConGrad(SEXP spModel,SEXP sirow,SEXP spadPrimal,SEXP snParList,SEXP spaiParList) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int irow;
  double *padPrimal= NULL;
  int nParList;
  int *paiParList= NULL;
  double *padParGrad= NULL;

  SEXP spadParGrad = R_NilValue;
  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","padParGrad"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = irow = Rf_asInteger(sirow);
  ibuf[4] = nParList = Rf_asInteger(snParList);
  MAKE_REAL_ARRAY(padPrimal,spadPrimal);
  dvecptr[3] = padPrimal; //padPrimal
  MAKE_INT_ARRAY(paiParList,spaiParList);
  ivecptr[5] = paiParList; //paiParList

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);


  // Get C pointers
  PROTECT(spadParGrad = NEW_NUMERIC(nParList));
  nProtect += 1;
  dvecptr[6] = padParGrad = NUMERIC_POINTER(spadParGrad); //padParGrad

  *pnErrorCode = errorcode = LScalcConGrad(pModel
    ,ibuf[2] //irow
    ,dvecptr[3] //*padPrimal
    ,ibuf[4] //nParList
    ,ivecptr[5] //*paiParList
    ,dvecptr[6]); //*padParGrad


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spadParGrad);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSwriteNLSolution
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark errorcode = rLSwriteNLSolution(pModel,pszFname)
 */
SEXP rcLSwriteNLSolution(SEXP spModel,SEXP spszFname) {
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSwriteNLSolution(pModel
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSwriteEnvParameter
 * @param[in,out] pEnv
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark errorcode = rLSwriteEnvParameter(pEnv,pszFname)
 */
SEXP rcLSwriteEnvParameter(SEXP spEnv,SEXP spszFname) {
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  char *pszFname= NULL;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSwriteEnvParameter(pEnv
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSreadCBFFile
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark ErrorCode = rLSreadCBFFile(spModel,pszFname)
 */
SEXP rcLSreadCBFFile(SEXP spModel,SEXP spszFname)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSreadCBFFile(pModel
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSreadMPXFile
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark ErrorCode = rLSreadMPXFile(spModel	,pszFname)
 */
SEXP rcLSreadMPXFile(SEXP spModel	,SEXP spszFname)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSreadMPXFile(pModel
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSreadNLFile
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @return int An integer error code
 * @remark ErrorCode = rLSreadNLFile(spModel,pszFname)
 */
SEXP rcLSreadNLFile(SEXP spModel,SEXP spszFname)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSreadNLFile(pModel
    ,sbuf[2]); //*pszFname


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSwriteDualLINDOFile
 * @param[in,out] pModel
 * @param[in,out] pszFname
 * @param[in,out] nObjSense
 * @return int An integer error code
 * @remark ErrorCode = rLSwriteDualLINDOFile(spModel,pszFname,nObjSense)
 */
SEXP rcLSwriteDualLINDOFile(SEXP spModel,SEXP spszFname,SEXP snObjSense)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszFname= NULL;
  int nObjSense;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));
  ibuf[3] = nObjSense = Rf_asInteger(snObjSense);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSwriteDualLINDOFile(pModel
    ,sbuf[2] //*pszFname
    ,ibuf[3]); //nObjSense


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetGOPVariablePriority
 * @param[in,out] pModel
 * @param[in,out] ndxVar
 * @param[in,out] pnPriority
 * @return int An integer error code
 * @remark ErrorCode,pnPriority = rLSgetGOPVariablePriority(spModel,ndxVar)
 */
SEXP rcLSgetGOPVariablePriority(SEXP spModel,SEXP sndxVar)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int ndxVar;
  int *pnPriority= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pnPriority"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;
  SEXP      spnPriority = R_NilValue;;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = ndxVar = Rf_asInteger(sndxVar);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnPriority = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnPriority = INTEGER_POINTER(spnPriority); //pnPriority

  *pnErrorCode = errorcode = LSgetGOPVariablePriority(pModel
    ,ibuf[2] //ndxVar
    ,ivecptr[3]); //*pnPriority


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnPriority);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


/*
 * @brief LSsetXSolverLibrary
 * @param[in,out] pEnv
 * @param[in,out] mVendorId
 * @param[in,out] szLibrary
 * @return int An integer error code
 * @remark ErrorCode = rLSsetXSolverLibrary(spEnv,mVendorId,szLibrary)
 */
SEXP rcLSsetXSolverLibrary(SEXP spEnv,SEXP smVendorId,SEXP sszLibrary)
{
  DCL_BUF(20);
  pLSenv     pEnv=NULL;
  prLSenv prEnv=NULL;

  int mVendorId;
  char *szLibrary= NULL;


  SEXP      sEnv=spEnv;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = mVendorId = Rf_asInteger(smVendorId);
  sbuf[3] = szLibrary = (char *) CHAR(STRING_ELT(sszLibrary,0));

  CHECK_ENV_ERROR;

  // Get C pointers
  *pnErrorCode = errorcode = LSsetXSolverLibrary(pEnv
    ,ibuf[2] //mVendorId
    ,sbuf[3]); //*szLibrary


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSsetGOPVariablePriority
 * @param[in,out] pModel
 * @param[in,out] ndxVar
 * @param[in,out] nPriority
 * @return int An integer error code
 * @remark ErrorCode = rLSsetGOPVariablePriority(spModel,ndxVar,nPriority)
 */
SEXP rcLSsetGOPVariablePriority(SEXP spModel,SEXP sndxVar,SEXP snPriority)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int ndxVar;
  int nPriority;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = ndxVar = Rf_asInteger(sndxVar);
  ibuf[3] = nPriority = Rf_asInteger(snPriority);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSsetGOPVariablePriority(pModel
    ,ibuf[2] //ndxVar
    ,ibuf[3]); //nPriority


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSsetObjPoolParam
 * @param[in,out] pModel
 * @param[in,out] nObjIndex
 * @param[in,out] mInfo
 * @param[in,out] dValue
 * @return int An integer error code
 * @remark ErrorCode = rLSsetObjPoolParam(spModel,nObjIndex,mInfo,dValue)
 */
SEXP rcLSsetObjPoolParam(SEXP spModel,SEXP snObjIndex,SEXP smInfo,SEXP sdValue)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nObjIndex;
  int mInfo;
  double dValue;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nObjIndex = Rf_asInteger(snObjIndex);
  ibuf[3] = mInfo = Rf_asInteger(smInfo);
  dbuf[4] = dValue = Rf_asReal(sdValue);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSsetObjPoolParam(pModel
    ,ibuf[2] //nObjIndex
    ,ibuf[3] //mInfo
    ,dbuf[4]); //dValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSsetSETSStatei
 * @param[in,out] pModel
 * @param[in,out] iSet
 * @param[in,out] mState
 * @return int An integer error code
 * @remark ErrorCode = rLSsetSETSStatei(spModel,iSet,mState)
 */
SEXP rcLSsetSETSStatei(SEXP spModel,SEXP siSet,SEXP smState)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int iSet;
  int mState;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = iSet = Rf_asInteger(siSet);
  ibuf[3] = mState = Rf_asInteger(smState);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSsetSETSStatei(pModel
    ,ibuf[2] //iSet
    ,ibuf[3]); //mState


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSremObjPool
 * @param[in,out] pModel
 * @param[in,out] nObjIndex
 * @return int An integer error code
 * @remark ErrorCode = rLSremObjPool(spModel,nObjIndex)
 */
SEXP rcLSremObjPool(SEXP spModel,SEXP snObjIndex)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nObjIndex;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nObjIndex = Rf_asInteger(snObjIndex);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSremObjPool(pModel
    ,ibuf[2]); //nObjIndex


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSrepairQterms
 * @param[in,out] pModel
 * @param[in,out] nCons
 * @param[in,out] paiCons
 * @param[in,out] paiType
 * @return int An integer error code
 * @remark ErrorCode = rLSrepairQterms(spModel,nCons,paiCons,paiType)
 */
SEXP rcLSrepairQterms(SEXP spModel,SEXP snCons,SEXP spaiCons,SEXP spaiType)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nCons;
  int *paiCons= NULL;
  int *paiType= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nCons = Rf_asInteger(snCons);
  MAKE_INT_ARRAY(paiCons,spaiCons);
  MAKE_INT_ARRAY(paiType,spaiType);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  ivecptr[3] = (int*)paiCons;

  ivecptr[4] = (int*)paiType;

  *pnErrorCode = errorcode = LSrepairQterms(pModel
    ,ibuf[2] //nCons
    ,ivecptr[3] //*paiCons
    ,ivecptr[4]); //*paiType


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSloadLicenseString
 * @param[in,out] pEnv
 * @param[in,out] pszFname
 * @param[in,out] pachLicense
 * @return int An integer error code
 * @remark ErrorCode, pachLicense = rLSloadLicenseString(pszFname)
 */
SEXP rcLSloadLicenseString(SEXP spszFname)
{
  DCL_BUF(20);

  char *pszFname= NULL;
  char *pachLicense= NULL;

  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pachLicense"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;
  SEXP      spachLicense = R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszFname = (char *) CHAR(STRING_ELT(spszFname,0));

  PROTECT(spachLicense = NEW_CHARACTER(1));
  nProtect += 1;
  pachLicense = (char *)malloc(sizeof(char)*(1024));

  // Get C pointers
  svecptr[3] = (char*)pachLicense;

  *pnErrorCode = errorcode = LSloadLicenseString(sbuf[2] //*pszFname
    ,svecptr[3]); //*pachLicense

  SET_STRING_ELT(spachLicense,0,mkChar(pachLicense));

  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spachLicense);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSloadALLDIFFData
 * @param[in,out] pModel
 * @param[in,out] nALLDIFF
 * @param[in,out] paiAlldiffDim
 * @param[in,out] paiAlldiffL
 * @param[in,out] paiAlldiffU
 * @param[in,out] paiAlldiffBeg
 * @param[in,out] paiAlldiffVar
 * @return int An integer error code
 * @remark ErrorCode = rLSloadALLDIFFData(spModel,nALLDIFF,paiAlldiffDim,paiAlldiffL,paiAlldiffU,paiAlldiffBeg,paiAlldiffVar)
 */
SEXP rcLSloadALLDIFFData(SEXP spModel,SEXP snALLDIFF,SEXP spaiAlldiffDim,SEXP spaiAlldiffL,SEXP spaiAlldiffU,SEXP spaiAlldiffBeg,SEXP spaiAlldiffVar)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nALLDIFF;
  int *paiAlldiffDim= NULL;
  int *paiAlldiffL= NULL;
  int *paiAlldiffU= NULL;
  int *paiAlldiffBeg= NULL;
  int *paiAlldiffVar= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nALLDIFF = Rf_asInteger(snALLDIFF);
  MAKE_INT_ARRAY(paiAlldiffDim,spaiAlldiffDim);
  MAKE_INT_ARRAY(paiAlldiffL,spaiAlldiffL);
  MAKE_INT_ARRAY(paiAlldiffU,spaiAlldiffU);
  MAKE_INT_ARRAY(paiAlldiffBeg,spaiAlldiffBeg);
  MAKE_INT_ARRAY(paiAlldiffVar,spaiAlldiffVar);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  ivecptr[3] = (int*)paiAlldiffDim;

  ivecptr[4] = (int*)paiAlldiffL;

  ivecptr[5] = (int*)paiAlldiffU;

  ivecptr[6] = (int*)paiAlldiffBeg;

  ivecptr[7] = (int*)paiAlldiffVar;

  *pnErrorCode = errorcode = LSloadALLDIFFData(pModel
    ,ibuf[2] //nALLDIFF
    ,ivecptr[3] //*paiAlldiffDim
    ,ivecptr[4] //*paiAlldiffL
    ,ivecptr[5] //*paiAlldiffU
    ,ivecptr[6] //*paiAlldiffBeg
    ,ivecptr[7]); //*paiAlldiffVar


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSloadIISPriorities
 * @param[in,out] pModel
 * @param[in,out] panRprior
 * @param[in,out] panCprior
 * @return int An integer error code
 * @remark ErrorCode = rLSloadIISPriorities(spModel,panRprior,panCprior)
 */
SEXP rcLSloadIISPriorities(SEXP spModel,SEXP spanRprior,SEXP spanCprior)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *panRprior= NULL;
  int *panCprior= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  MAKE_INT_ARRAY(panRprior,spanRprior);
  MAKE_INT_ARRAY(panCprior,spanCprior);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  ivecptr[2] = (int*)panRprior;

  ivecptr[3] = (int*)panCprior;

  *pnErrorCode = errorcode = LSloadIISPriorities(pModel
    ,ivecptr[2] //*panRprior
    ,ivecptr[3]); //*panCprior


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSloadNLPDense
 * @param[in,out] pModel
 * @param[in,out] nCons
 * @param[in,out] nVars
 * @param[in,out] dObjSense
 * @param[in,out] pszConTypes
 * @param[in,out] pszVarTypes
 * @param[in,out] padX0
 * @param[in,out] padL
 * @param[in,out] padU
 * @return int An integer error code
 * @remark ErrorCode = rLSloadNLPDense(spModel,nCons,nVars,dObjSense,pszConTypes,pszVarTypes,padX0,padL,padU)
 */
SEXP rcLSloadNLPDense(SEXP spModel,SEXP snCons,SEXP snVars,SEXP sdObjSense,SEXP spszConTypes,SEXP spszVarTypes,SEXP spadX0,SEXP spadL,SEXP spadU)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nCons;
  int nVars;
  int dObjSense;
  char *pszConTypes= NULL;
  char *pszVarTypes= NULL;
  double *padX0= NULL;
  double *padL= NULL;
  double *padU= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nCons = Rf_asInteger(snCons);
  ibuf[3] = nVars = Rf_asInteger(snVars);
  ibuf[4] = dObjSense = Rf_asInteger(sdObjSense);
  sbuf[5] = pszConTypes = (char *) CHAR(STRING_ELT(spszConTypes,0));
  sbuf[6] = pszVarTypes = (char *) CHAR(STRING_ELT(spszVarTypes,0));
  MAKE_REAL_ARRAY(padX0,spadX0);
  MAKE_REAL_ARRAY(padL,spadL);
  MAKE_REAL_ARRAY(padU,spadU);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  dvecptr[7] = (double*)padX0;

  dvecptr[8] = (double*)padL;

  dvecptr[9] = (double*)padU;

  *pnErrorCode = errorcode = LSloadNLPDense(pModel
    ,ibuf[2] //nCons
    ,ibuf[3] //nVars
    ,ibuf[4] //dObjSense
    ,sbuf[5] //*pszConTypes
    ,sbuf[6] //*pszVarTypes
    ,dvecptr[7] //*padX0
    ,dvecptr[8] //*padL
    ,dvecptr[9]); //*padU


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSloadSolutionAt
 * @param[in,out] pModel
 * @param[in,out] nObjIndex
 * @param[in,out] nSolIndex
 * @return int An integer error code
 * @remark ErrorCode = rLSloadSolutionAt(spModel,nObjIndex,nSolIndex)
 */
SEXP rcLSloadSolutionAt(SEXP spModel,SEXP snObjIndex,SEXP snSolIndex)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nObjIndex;
  int nSolIndex;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nObjIndex = Rf_asInteger(snObjIndex);
  ibuf[3] = nSolIndex = Rf_asInteger(snSolIndex);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSloadSolutionAt(pModel
    ,ibuf[2] //nObjIndex
    ,ibuf[3]); //nSolIndex


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSmodifyObjConstant
 * @param[in,out] pModel
 * @param[in,out] dObjConst
 * @return int An integer error code
 * @remark ErrorCode = rLSmodifyObjConstant(spModel,dObjConst)
 */
SEXP rcLSmodifyObjConstant(SEXP spModel,SEXP sdObjConst)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double dObjConst;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  dbuf[2] = dObjConst = Rf_asReal(sdObjConst);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSmodifyObjConstant(pModel
    ,dbuf[2]); //dObjConst


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSreadMPXStream
 * @param[in,out] pModel
 * @param[in,out] pszStream
 * @param[in,out] nStreamLen
 * @return int An integer error code
 * @remark ErrorCode = rLSreadMPXStream(spModel,pszStream,nStreamLen)
 */
SEXP rcLSreadMPXStream(SEXP spModel,SEXP spszStream,SEXP snStreamLen)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  char *pszStream= NULL;
  int nStreamLen;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  sbuf[2] = pszStream = (char *) CHAR(STRING_ELT(spszStream,0));
  ibuf[3] = nStreamLen = Rf_asInteger(snStreamLen);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSreadMPXStream(pModel
    ,sbuf[2] //*pszStream
    ,ibuf[3]); //nStreamLen


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSsetMIPCCStrategy
 * @param[in,out] pModel
 * @param[in,out] nRunId
 * @param[in,out] szParamFile
 * @return int An integer error code
 * @remark ErrorCode = rLSsetMIPCCStrategy(spModel,nRunId,szParamFile)
 */
SEXP rcLSsetMIPCCStrategy(SEXP spModel,SEXP snRunId,SEXP sszParamFile)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nRunId;
  char *szParamFile= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nRunId = Rf_asInteger(snRunId);
  sbuf[3] = szParamFile = (char *) CHAR(STRING_ELT(sszParamFile,0));

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSsetMIPCCStrategy(pModel
    , NULL
    ,ibuf[2] //nRunId
    ,sbuf[3] //*szParamFile
    , NULL);


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetNextBestSol
 * @param[in,out] pModel
 * @param[in,out] pnModStatus
 * @return int An integer error code
 * @remark ErrorCode,pnModStatus = rLSgetNextBestSol(spModel)
 */
SEXP rcLSgetNextBestSol(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnModStatus= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pnModStatus"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spnModStatus= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnModStatus = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnModStatus = INTEGER_POINTER(spnModStatus); //pnModStatus

  *pnErrorCode = errorcode = LSgetNextBestSol(pModel
    ,ivecptr[2]); //*pnModStatus


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnModStatus);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetNnzData
 * @param[in,out] pModel
 * @param[in,out] mStat
 * @param[in,out] panOutput
 * @return int An integer error code
 * @remark ErrorCode,panOutput = rLSgetNnzData(spModel,mStat)
 */
SEXP rcLSgetNnzData(SEXP spModel,SEXP smStat)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int mStat, nItems;
  int *panOutput= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","panOutput"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spanOutput= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = mStat = Rf_asInteger(smStat);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

   if (mStat==LS_IINFO_NZCINDEX||mStat==LS_IINFO_NZCRANK||mStat==LS_IINFO_NZCCOUNT) {
     nItems = n;
   } else if (mStat==LS_IINFO_NZRINDEX||mStat==LS_IINFO_NZRRANK||mStat==LS_IINFO_NZRCOUNT) {
     nItems = m;
   } else {
     errorcode = LSERR_ERROR_IN_INPUT;
     goto ErrorReturn;
   }

  // Get C pointers
  PROTECT(spanOutput = NEW_INTEGER(nItems));
  nProtect += 1;
  ivecptr[3] = panOutput = INTEGER_POINTER(spanOutput); //panOutput

  *pnErrorCode = errorcode = LSgetNnzData(pModel
    ,ibuf[2] //mStat
    ,ivecptr[3]); //*panOutput


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spanOutput);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetObjectiveRanges
 * @param[in,out] pModel
 * @param[in,out] padDec
 * @param[in,out] padInc
 * @return int An integer error code
 * @remark ErrorCode,padDec,padInc = rLSgetObjectiveRanges(spModel)
 */
SEXP rcLSgetObjectiveRanges(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *padDec= NULL;
  double *padInc= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[3] = {"ErrorCode","padDec","padInc"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 3;
  int       nIdx, nProtect = 0;

  SEXP spadDec= R_NilValue;
  SEXP spadInc= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spadDec = NEW_NUMERIC(n));
  nProtect += 1;
  dvecptr[2] = padDec = NUMERIC_POINTER(spadDec); //padDec

  PROTECT(spadInc = NEW_NUMERIC(n));
  nProtect += 1;
  dvecptr[3] = padInc = NUMERIC_POINTER(spadInc); //padInc

  *pnErrorCode = errorcode = LSgetObjectiveRanges(pModel
    ,dvecptr[2] //*padDec
    ,dvecptr[3]); //*padInc


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spadDec);
    SET_VECTOR_ELT(rList, 2, spadInc);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetObjPoolNumSol
 * @param[in,out] pModel
 * @param[in,out] nObjIndex
 * @param[in,out] pNumSol
 * @return int An integer error code
 * @remark ErrorCode,pNumSol = rLSgetObjPoolNumSol(spModel,nObjIndex)
 */
SEXP rcLSgetObjPoolNumSol(SEXP spModel,SEXP snObjIndex)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nObjIndex;
  int *pNumSol= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pNumSol"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spNumSol= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nObjIndex = Rf_asInteger(snObjIndex);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spNumSol = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pNumSol = INTEGER_POINTER(spNumSol); //pNumSol

  *pnErrorCode = errorcode = LSgetObjPoolNumSol(pModel
    ,ibuf[2] //nObjIndex
    ,ivecptr[3]); //*pNumSol


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spNumSol);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetPOSDData
 * @param[in,out] pModel
 * @param[in,out] pinPOSD
 * @param[in,out] paiPOSDdim
 * @param[in,out] paiPOSDnnz
 * @param[in,out] paiPOSDbeg
 * @param[in,out] paiPOSDrowndx
 * @param[in,out] paiPOSDcolndx
 * @param[in,out] paiPOSDvarndx
 * @return int An integer error code
 * @remark ErrorCode,pinPOSD,paiPOSDdim,paiPOSDnnz,paiPOSDbeg,paiPOSDrowndx,paiPOSDcolndx,paiPOSDvarndx = rLSgetPOSDData(spModel)
 */
SEXP rcLSgetPOSDData(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pinPOSD= NULL, i=0;
  int *paiPOSDdim= NULL;
  int *paiPOSDnnz= NULL;
  int *paiPOSDbeg= NULL;
  int *paiPOSDrowndx= NULL;
  int *paiPOSDcolndx= NULL;
  int *paiPOSDvarndx= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[8] = {"ErrorCode","pinPOSD","paiPOSDdim","paiPOSDnnz","paiPOSDbeg","paiPOSDrowndx","paiPOSDcolndx","paiPOSDvarndx"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 8;
  int       nIdx, nProtect = 0;

  SEXP spinPOSD= R_NilValue;
  SEXP spaiPOSDdim= R_NilValue;
  SEXP spaiPOSDnnz= R_NilValue;
  SEXP spaiPOSDbeg= R_NilValue;
  SEXP spaiPOSDrowndx= R_NilValue;
  SEXP spaiPOSDcolndx= R_NilValue;
  SEXP spaiPOSDvarndx= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  errorcode = LSgetPOSDData(pModel,&ibuf[2],NULL,NULL,NULL,NULL,NULL,NULL);
  if (errorcode) goto ErrorReturn;

  // Get C pointers
  PROTECT(spinPOSD = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pinPOSD = INTEGER_POINTER(spinPOSD); //pinPOSD

  PROTECT(spaiPOSDdim = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[3] = paiPOSDdim = INTEGER_POINTER(spaiPOSDdim); //paiPOSDdim

  PROTECT(spaiPOSDnnz = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[4] = paiPOSDnnz = INTEGER_POINTER(spaiPOSDnnz); //paiPOSDnnz

  PROTECT(spaiPOSDbeg = NEW_INTEGER(ibuf[2]+1));
  nProtect += 1;
  ivecptr[5] = paiPOSDbeg = INTEGER_POINTER(spaiPOSDbeg); //paiPOSDbeg

  errorcode = LSgetPOSDData(pModel,&ibuf[2],ivecptr[3],ivecptr[4],ivecptr[5],NULL,NULL,NULL);
  if (errorcode) goto ErrorReturn;
  ibuf[4]=0;
  for (i=0; i<ibuf[4]; i++)
    ibuf[4]+=ivecptr[4][i];

  PROTECT(spaiPOSDrowndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[6] = paiPOSDrowndx = INTEGER_POINTER(spaiPOSDrowndx); //paiPOSDrowndx

  PROTECT(spaiPOSDcolndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[7] = paiPOSDcolndx = INTEGER_POINTER(spaiPOSDcolndx); //paiPOSDcolndx

  PROTECT(spaiPOSDvarndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[8] = paiPOSDvarndx = INTEGER_POINTER(spaiPOSDvarndx); //paiPOSDvarndx

  *pnErrorCode = errorcode = LSgetPOSDData(pModel
    ,ivecptr[2] //*pinPOSD
    ,ivecptr[3] //*paiPOSDdim
    ,ivecptr[4] //*paiPOSDnnz
    ,ivecptr[5] //*paiPOSDbeg
    ,ivecptr[6] //*paiPOSDrowndx
    ,ivecptr[7] //*paiPOSDcolndx
    ,ivecptr[8]); //*paiPOSDvarndx


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spinPOSD);
    SET_VECTOR_ELT(rList, 2, spaiPOSDdim);
    SET_VECTOR_ELT(rList, 3, spaiPOSDnnz);
    SET_VECTOR_ELT(rList, 4, spaiPOSDbeg);
    SET_VECTOR_ELT(rList, 5, spaiPOSDrowndx);
    SET_VECTOR_ELT(rList, 6, spaiPOSDcolndx);
    SET_VECTOR_ELT(rList, 7, spaiPOSDvarndx);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetPOSDDatai
 * @param[in,out] pModel
 * @param[in,out] iPOSD
 * @param[in,out] piPOSDdim
 * @param[in,out] piPOSDnnz
 * @param[in,out] paiPOSDrowndx
 * @param[in,out] paiPOSDcolndx
 * @param[in,out] paiPOSDvarndx
 * @return int An integer error code
 * @remark ErrorCode,piPOSDdim,piPOSDnnz,paiPOSDrowndx,paiPOSDcolndx,paiPOSDvarndx = rLSgetPOSDDatai(spModel,iPOSD)
 */
SEXP rcLSgetPOSDDatai(SEXP spModel,SEXP siPOSD)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int iPOSD;
  int *piPOSDdim= NULL;
  int *piPOSDnnz= NULL;
  int *paiPOSDrowndx= NULL;
  int *paiPOSDcolndx= NULL;
  int *paiPOSDvarndx= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[6] = {"ErrorCode","piPOSDdim","piPOSDnnz","paiPOSDrowndx","paiPOSDcolndx","paiPOSDvarndx"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 6;
  int       nIdx, nProtect = 0;

  SEXP spiPOSDdim= R_NilValue;
  SEXP spiPOSDnnz= R_NilValue;
  SEXP spaiPOSDrowndx= R_NilValue;
  SEXP spaiPOSDcolndx= R_NilValue;
  SEXP spaiPOSDvarndx= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = iPOSD = Rf_asInteger(siPOSD);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  errorcode = LSgetPOSDDatai(pModel,ibuf[2],&ibuf[3],&ibuf[4],NULL,NULL,NULL);
  if (errorcode) goto ErrorReturn;

  // Get C pointers
  PROTECT(spiPOSDdim = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = piPOSDdim = INTEGER_POINTER(spiPOSDdim); //piPOSDdim

  PROTECT(spiPOSDnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[4] = piPOSDnnz = INTEGER_POINTER(spiPOSDnnz); //piPOSDnnz

  PROTECT(spaiPOSDrowndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[5] = paiPOSDrowndx = INTEGER_POINTER(spaiPOSDrowndx); //paiPOSDrowndx

  PROTECT(spaiPOSDcolndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[6] = paiPOSDcolndx = INTEGER_POINTER(spaiPOSDcolndx); //paiPOSDcolndx

  PROTECT(spaiPOSDvarndx = NEW_INTEGER(ibuf[4]));
  nProtect += 1;
  ivecptr[7] = paiPOSDvarndx = INTEGER_POINTER(spaiPOSDvarndx); //paiPOSDvarndx

  *pnErrorCode = errorcode = LSgetPOSDDatai(pModel
    ,ibuf[2] //iPOSD
    ,ivecptr[3] //*piPOSDdim
    ,ivecptr[4] //*piPOSDnnz
    ,ivecptr[5] //*paiPOSDrowndx
    ,ivecptr[6] //*paiPOSDcolndx
    ,ivecptr[7]); //*paiPOSDvarndx


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spiPOSDdim);
    SET_VECTOR_ELT(rList, 2, spiPOSDnnz);
    SET_VECTOR_ELT(rList, 3, spaiPOSDrowndx);
    SET_VECTOR_ELT(rList, 4, spaiPOSDcolndx);
    SET_VECTOR_ELT(rList, 5, spaiPOSDvarndx);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetProgressInfo
 * @param[in,out] pModel
 * @param[in,out] nLocation
 * @param[in,out] nQuery
 * @param[in,out] pvValue
 * @return int An integer error code
 * @remark ErrorCode,pvValue = rLSgetProgressInfo(spModel,nLocation,nQuery)
 */
SEXP rcLSgetProgressIInfo(SEXP spModel,SEXP snLocation,SEXP snQuery)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nLocation;
  int nQuery;
  int *pvValue= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pvValue"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;


  SEXP spvValue= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nLocation = Rf_asInteger(snLocation);
  ibuf[3] = nQuery = Rf_asInteger(snQuery);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spvValue = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[4] = pvValue = INTEGER_POINTER(spvValue); //pvValue

  *pnErrorCode = errorcode = LSgetProgressInfo(pModel
    ,ibuf[2] //nLocation
    ,ibuf[3] //nQuery
    ,ivecptr[4]); //*pvValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spvValue);

  UNPROTECT(nProtect + 2);
  return rList;

}

/*
 * @brief LSgetProgressInfo
 * @param[in,out] pModel
 * @param[in,out] nLocation
 * @param[in,out] nQuery
 * @param[in,out] pvValue
 * @return int An integer error code
 * @remark ErrorCode,pvValue = rLSgetProgressInfo(spModel,nLocation,nQuery)
 */
SEXP rcLSgetProgressDInfo(SEXP spModel,SEXP snLocation,SEXP snQuery)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nLocation;
  int nQuery;
  double *pvValue= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pvValue"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;


  SEXP spvValue= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nLocation = Rf_asInteger(snLocation);
  ibuf[3] = nQuery = Rf_asInteger(snQuery);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spvValue = NEW_NUMERIC(1));
  nProtect += 1;
  dvecptr[4] = pvValue = NUMERIC_POINTER(spvValue); //pvValue

  *pnErrorCode = errorcode = LSgetProgressInfo(pModel
    ,ibuf[2] //nLocation
    ,ibuf[3] //nQuery
    ,dvecptr[4]); //*pvValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  SET_VECTOR_ELT(rList, 1, spvValue);

  UNPROTECT(nProtect + 2);
  return rList;

}
/*
 * @brief LSgetObjective
 * @param[in,out] pModel
 * @param[in,out] pdObj
 * @return int An integer error code
 * @remark ErrorCode,pdObj = rLSgetObjective(spModel)
 */
SEXP rcLSgetObjective(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *pdObj= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","pdObj"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spdObj= R_NilValue;

  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spdObj = NEW_NUMERIC(n));
  nProtect += 1;
  dvecptr[2] = pdObj = NUMERIC_POINTER(spdObj); //pdObj

  *pnErrorCode = errorcode = LSgetObjective(pModel
    ,dvecptr[2] //*pdObj
    );


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spdObj);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LScalcObjFunc
 * @param[in,out] pModel
 * @param[in,out] pdObjval
 * @return int An integer error code
 * @remark ErrorCode,pdObjval = rLScalcObjFunc(spModel,padPrimal)
 */
SEXP rcLScalcObjFunc(SEXP spModel,SEXP spadPrimal)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *padPrimal= NULL;
  double *pdObjval= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[3] = {"ErrorCode","pdObjval"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 3;
  int       nIdx, nProtect = 0;

  SEXP spdObjval= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  MAKE_REAL_ARRAY(padPrimal,spadPrimal);
  dvecptr[2] = padPrimal;

  PROTECT(spdObjval = NEW_NUMERIC(1));
  nProtect += 1;
  dvecptr[3] = pdObjval = NUMERIC_POINTER(spdObjval); //pdObjval

  *pnErrorCode = errorcode = LScalcObjFunc(pModel
    ,dvecptr[2] //*padPrimal
    ,dvecptr[3]); //*pdObjval


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spdObjval);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LScalcObjGrad
 * @param[in,out] pModel
 * @param[in,out] padPrimal
 * @param[in,out] nParList
 * @param[in,out] paiParList
 * @param[in,out] padParGrad
 * @return int An integer error code
 * @remark ErrorCode,padParGrad = rLScalcObjGrad(spModel,padPrimal,nParList,paiParList)
 */
SEXP rcLScalcObjGrad(SEXP spModel,SEXP spadPrimal,SEXP snParList, SEXP spaiParList)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *padPrimal= NULL;
  int nParList;
  int *paiParList= NULL;
  double *padParGrad= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","padParGrad"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spadParGrad= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[3] = nParList = Rf_asInteger(snParList);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);
  MAKE_REAL_ARRAY(padPrimal,spadPrimal);
  dvecptr[2] = padPrimal;
  MAKE_INT_ARRAY(paiParList,spaiParList);
  ivecptr[4] = paiParList;

  PROTECT(spadParGrad = NEW_NUMERIC(nParList));
  nProtect += 1;
  dvecptr[5] = padParGrad = NUMERIC_POINTER(spadParGrad); //padParGrad

  *pnErrorCode = errorcode = LScalcObjGrad(pModel
    ,dvecptr[2] //*padPrimal
    ,ibuf[3] //nParList
    ,ivecptr[4] //*paiParList
    ,dvecptr[5]); //*padParGrad


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spadParGrad);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LScheckQterms
 * @param[in,out] pModel
 * @param[in,out] nCons
 * @param[in,out] paiCons
 * @param[in,out] paiType
 * @return int An integer error code
 * @remark ErrorCode,paiType = rLScheckQterms(spModel,nCons,paiCons)
 */
SEXP rcLScheckQterms(SEXP spModel,SEXP snCons,SEXP spaiCons)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nCons;
  int *paiCons= NULL;
  int *paiType= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = {"ErrorCode","paiType"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spaiType= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nCons = Rf_asInteger(snCons);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);
  MAKE_INT_ARRAY(paiCons,spaiCons);



  // Get C pointers
  ivecptr[3] = paiCons; //paiCons

  PROTECT(spaiType = NEW_INTEGER(nCons));
  nProtect += 1;
  ivecptr[4] = paiType = INTEGER_POINTER(spaiType); //paiType

  *pnErrorCode = errorcode = LScheckQterms(pModel
    ,ibuf[2] //nCons
    ,ivecptr[3] //*paiCons
    ,ivecptr[4]); //*paiType


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spaiType);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSdisplayBlockStructure
 * @param[in,out] pModel
 * @param[in,out] pnBlock
 * @param[in,out] panNewColIdx
 * @param[in,out] panNewRowIdx
 * @param[in,out] panNewColPos
 * @param[in,out] panNewRowPos
 * @return int An integer error code
 * @remark ErrorCode,pnBlock,panNewColIdx,panNewRowIdx,panNewColPos,panNewRowPos = rLSdisplayBlockStructure(spModel)
 */
SEXP rcLSdisplayBlockStructure(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnBlock= NULL;
  int *panNewColIdx= NULL;
  int *panNewRowIdx= NULL;
  int *panNewColPos= NULL;
  int *panNewRowPos= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[6] = {"ErrorCode","pnBlock","panNewColIdx","panNewRowIdx","panNewColPos","panNewRowPos"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 6;
  int       nIdx, nProtect = 0;

  SEXP spnBlock= R_NilValue;
  SEXP spanNewColIdx= R_NilValue;
  SEXP spanNewRowIdx= R_NilValue;
  SEXP spanNewColPos= R_NilValue;
  SEXP spanNewRowPos= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnBlock = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnBlock = INTEGER_POINTER(spnBlock); //pnBlock

  *pnErrorCode = errorcode = LSdisplayBlockStructure(pModel,ivecptr[2],ivecptr[3],ivecptr[4],ivecptr[5],ivecptr[6]);
  if (errorcode) goto ErrorReturn;
  ibuf[2] = *ivecptr[2];

  PROTECT(spanNewColIdx = NEW_INTEGER(n));
  nProtect += 1;
  ivecptr[3] = panNewColIdx = INTEGER_POINTER(spanNewColIdx); //panNewColIdx

  PROTECT(spanNewRowIdx = NEW_INTEGER(m));
  nProtect += 1;
  ivecptr[4] = panNewRowIdx = INTEGER_POINTER(spanNewRowIdx); //panNewRowIdx

  PROTECT(spanNewColPos = NEW_INTEGER(n));
  nProtect += 1;
  ivecptr[5] = panNewColPos = INTEGER_POINTER(spanNewColPos); //panNewColPos

  PROTECT(spanNewRowPos = NEW_INTEGER(m));
  nProtect += 1;
  ivecptr[6] = panNewRowPos = INTEGER_POINTER(spanNewRowPos); //panNewRowPos

  *pnErrorCode = errorcode = LSdisplayBlockStructure(pModel
    ,ivecptr[2] //*pnBlock
    ,ivecptr[3] //*panNewColIdx
    ,ivecptr[4] //*panNewRowIdx
    ,ivecptr[5] //*panNewColPos
    ,ivecptr[6]); //*panNewRowPos


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnBlock);
    SET_VECTOR_ELT(rList, 2, spanNewColIdx);
    SET_VECTOR_ELT(rList, 3, spanNewRowIdx);
    SET_VECTOR_ELT(rList, 4, spanNewColPos);
    SET_VECTOR_ELT(rList, 5, spanNewRowPos);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSdoBTRAN
 * @param[in,out] pModel
 * @param[in,out] pcYnz
 * @param[in,out] paiY
 * @param[in,out] padY
 * @param[in,out] pcXnz
 * @param[in,out] paiX
 * @param[in,out] padX
 * @return int An integer error code
 * @remark ErrorCode,pcXnz,paiX,padX = rLSdoBTRAN(spModel,spcYnz,spaiY,spadY)
 */
SEXP rcLSdoBTRAN(SEXP spModel,SEXP spcYnz,SEXP spaiY,SEXP spadY)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pcYnz= NULL, i;
  int *paiY= NULL;
  double *padY= NULL;
  int *pcXnz= NULL;
  int *paiX= NULL;
  double *padX= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[4] = {"ErrorCode","pcXnz","paiX","padX"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 4;
  int       nIdx, nProtect = 0;

  SEXP spcXnz= R_NilValue;
  SEXP spaiX= R_NilValue;
  SEXP spadX= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);
  MAKE_INT_ARRAY(pcYnz,spcYnz);
  MAKE_INT_ARRAY(paiY,spaiY);
  MAKE_REAL_ARRAY(padY,spadY);


  // Get C pointers
  ivecptr[2] = pcYnz; //pcYnz

  ivecptr[3] = paiY; //paiY

  dvecptr[4] = padY; //padY

  PROTECT(spcXnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[5] = pcXnz = INTEGER_POINTER(spcXnz); //pcXnz
  ivecptr[8] = malloc(m*sizeof(int));
  dvecptr[9] = malloc(m*sizeof(double));
  *pnErrorCode = errorcode = LSdoBTRAN(pModel
    ,ivecptr[2] //*pcYnz
    ,ivecptr[3] //*paiY
    ,dvecptr[4] //*padY
    ,ivecptr[5] //*pcXnz
    ,ivecptr[8] //*paiX
    ,dvecptr[9]); //*padX
  if (errorcode) goto ErrorReturn;
  PROTECT(spaiX = NEW_INTEGER(*pcXnz));
  nProtect += 1;
  ivecptr[6] = paiX = INTEGER_POINTER(spaiX); //paiX

  PROTECT(spadX = NEW_NUMERIC(*pcXnz));
  nProtect += 1;
  dvecptr[7] = padX = NUMERIC_POINTER(spadX); //padX

  for (i=0; i<*pcXnz; i++) {
    paiX[i] = ivecptr[8][i];
    padX[i] = dvecptr[9][i];
  }

ErrorReturn:
  if (ivecptr[8]) free(ivecptr[8]);
  if (dvecptr[9]) free(dvecptr[9]);
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spcXnz);
    SET_VECTOR_ELT(rList, 2, spaiX);
    SET_VECTOR_ELT(rList, 3, spadX);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSdoBTRAN
 * @param[in,out] pModel
 * @param[in,out] pcYnz
 * @param[in,out] paiY
 * @param[in,out] padY
 * @param[in,out] pcXnz
 * @param[in,out] paiX
 * @param[in,out] padX
 * @return int An integer error code
 * @remark ErrorCode,pcXnz,paiX,padX = rLSdoFTRAN(spModel,spcYnz,spaiY,spadY)
 */
SEXP rcLSdoFTRAN(SEXP spModel,SEXP spcYnz,SEXP spaiY,SEXP spadY)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pcYnz= NULL, i;
  int *paiY= NULL;
  double *padY= NULL;
  int *pcXnz= NULL;
  int *paiX= NULL;
  double *padX= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[4] = {"ErrorCode","pcXnz","paiX","padX"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 4;
  int       nIdx, nProtect = 0;

  SEXP spcXnz= R_NilValue;
  SEXP spaiX= R_NilValue;
  SEXP spadX= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);
  MAKE_INT_ARRAY(pcYnz,spcYnz);
  MAKE_INT_ARRAY(paiY,spaiY);
  MAKE_REAL_ARRAY(padY,spadY);


  // Get C pointers
  ivecptr[2] = pcYnz; //pcYnz

  ivecptr[3] = paiY; //paiY

  dvecptr[4] = padY; //padY

  PROTECT(spcXnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[5] = pcXnz = INTEGER_POINTER(spcXnz); //pcXnz
  ivecptr[8] = malloc(m*sizeof(int));
  dvecptr[9] = malloc(m*sizeof(double));
  *pnErrorCode = errorcode = LSdoFTRAN(pModel
    ,ivecptr[2] //*pcYnz
    ,ivecptr[3] //*paiY
    ,dvecptr[4] //*padY
    ,ivecptr[5] //*pcXnz
    ,ivecptr[8] //*paiX
    ,dvecptr[9]); //*padX
  if (errorcode) goto ErrorReturn;
  PROTECT(spaiX = NEW_INTEGER(*pcXnz));
  nProtect += 1;
  ivecptr[6] = paiX = INTEGER_POINTER(spaiX); //paiX

  PROTECT(spadX = NEW_NUMERIC(*pcXnz));
  nProtect += 1;
  dvecptr[7] = padX = NUMERIC_POINTER(spadX); //padX

  for (i=0; i<*pcXnz; i++) {
    paiX[i] = ivecptr[8][i];
    padX[i] = dvecptr[9][i];
  }

ErrorReturn:
  if (ivecptr[8]) free(ivecptr[8]);
  if (dvecptr[9]) free(dvecptr[9]);
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spcXnz);
    SET_VECTOR_ELT(rList, 2, spaiX);
    SET_VECTOR_ELT(rList, 3, spadX);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSfindLtf
 * @param[in,out] pModel
 * @param[in,out] panNewColIdx
 * @param[in,out] panNewRowIdx
 * @param[in,out] panNewColPos
 * @param[in,out] panNewRowPos
 * @return int An integer error code
 * @remark ErrorCode,panNewColIdx,panNewRowIdx,panNewColPos,panNewRowPos = rLSfindLtf(spModel)
 */
SEXP rcLSfindLtf(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *panNewColIdx= NULL;
  int *panNewRowIdx= NULL;
  int *panNewColPos= NULL;
  int *panNewRowPos= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[5] = {"ErrorCode","panNewColIdx","panNewRowIdx","panNewColPos","panNewRowPos"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 5;
  int       nIdx, nProtect = 0;

  SEXP spanNewColIdx= R_NilValue;
  SEXP spanNewRowIdx= R_NilValue;
  SEXP spanNewColPos= R_NilValue;
  SEXP spanNewRowPos= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spanNewColIdx = NEW_INTEGER(n));
  nProtect += 1;
  ivecptr[2] = panNewColIdx = INTEGER_POINTER(spanNewColIdx); //panNewColIdx

  PROTECT(spanNewRowIdx = NEW_INTEGER(m));
  nProtect += 1;
  ivecptr[3] = panNewRowIdx = INTEGER_POINTER(spanNewRowIdx); //panNewRowIdx

  PROTECT(spanNewColPos = NEW_INTEGER(n));
  nProtect += 1;
  ivecptr[4] = panNewColPos = INTEGER_POINTER(spanNewColPos); //panNewColPos

  PROTECT(spanNewRowPos = NEW_INTEGER(m));
  nProtect += 1;
  ivecptr[5] = panNewRowPos = INTEGER_POINTER(spanNewRowPos); //panNewRowPos

  *pnErrorCode = errorcode = LSfindLtf(pModel
    ,ivecptr[2] //*panNewColIdx
    ,ivecptr[3] //*panNewRowIdx
    ,ivecptr[4] //*panNewColPos
    ,ivecptr[5]); //*panNewRowPos


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spanNewColIdx);
    SET_VECTOR_ELT(rList, 2, spanNewRowIdx);
    SET_VECTOR_ELT(rList, 3, spanNewColPos);
    SET_VECTOR_ELT(rList, 4, spanNewRowPos);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSfreeObjPool
 * @param[in,out] pModel
 * @return int An integer error code
 * @remark ErrorCode = rLSfreeObjPool(spModel)
 */
SEXP rcLSfreeObjPool(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;



  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = {"ErrorCode"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSfreeObjPool(pModel
); //nil


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetALLDIFFData
 * @param[in,out] pModel
 * @param[in,out] pinALLDIFF
 * @param[in,out] paiAlldiffDim
 * @param[in,out] paiAlldiffL
 * @param[in,out] paiAlldiffU
 * @param[in,out] paiAlldiffBeg
 * @param[in,out] paiAlldiffVar
 * @return int An integer error code
 * @remark ErrorCode,pinALLDIFF,paiAlldiffDim,paiAlldiffL,paiAlldiffU,paiAlldiffBeg,paiAlldiffVar = rLSgetALLDIFFData(spModel)
 */
SEXP rcLSgetALLDIFFData(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pinALLDIFF= NULL;
  int *paiAlldiffDim= NULL;
  int *paiAlldiffL= NULL;
  int *paiAlldiffU= NULL;
  int *paiAlldiffBeg= NULL;
  int *paiAlldiffVar= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[7] = {"ErrorCode","pinALLDIFF","paiAlldiffDim","paiAlldiffL","paiAlldiffU","paiAlldiffBeg","paiAlldiffVar"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 7;
  int       nIdx, nProtect = 0;

  SEXP spinALLDIFF= R_NilValue;
  SEXP spaiAlldiffDim= R_NilValue;
  SEXP spaiAlldiffL= R_NilValue;
  SEXP spaiAlldiffU= R_NilValue;
  SEXP spaiAlldiffBeg= R_NilValue;
  SEXP spaiAlldiffVar= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);


  errorcode = LSgetALLDIFFData(pModel,&ibuf[2],NULL,NULL,NULL,NULL,NULL);

  // Get C pointers
  PROTECT(spinALLDIFF = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pinALLDIFF = INTEGER_POINTER(spinALLDIFF); //pinALLDIFF
  *ivecptr[2] = ibuf[2];

  PROTECT(spaiAlldiffDim = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[3] = paiAlldiffDim = INTEGER_POINTER(spaiAlldiffDim); //paiAlldiffDim

  PROTECT(spaiAlldiffL = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[4] = paiAlldiffL = INTEGER_POINTER(spaiAlldiffL); //paiAlldiffL

  PROTECT(spaiAlldiffU = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[5] = paiAlldiffU = INTEGER_POINTER(spaiAlldiffU); //paiAlldiffU

  PROTECT(spaiAlldiffBeg = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[6] = paiAlldiffBeg = INTEGER_POINTER(spaiAlldiffBeg); //paiAlldiffBeg

  *pnErrorCode = errorcode = LSgetALLDIFFData(pModel,&ibuf[2],ivecptr[3],ivecptr[4],ivecptr[5],ivecptr[6],NULL);
  if (errorcode) goto ErrorReturn;
  ibuf[7]=ivecptr[6][ibuf[2]-1]+ivecptr[3][ibuf[2]-1]; //n+dim

  PROTECT(spaiAlldiffVar = NEW_INTEGER(ibuf[7]));
  nProtect += 1;
  ivecptr[7] = paiAlldiffVar = INTEGER_POINTER(spaiAlldiffVar); //paiAlldiffVar


  *pnErrorCode = errorcode = LSgetALLDIFFData(pModel
    ,ivecptr[2] //*pinALLDIFF
    ,NULL //*paiAlldiffDim
    ,NULL //*paiAlldiffL
    ,NULL //*paiAlldiffU
    ,NULL //*paiAlldiffBeg
    ,ivecptr[7]); //*paiAlldiffVar


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spinALLDIFF);
    SET_VECTOR_ELT(rList, 2, spaiAlldiffDim);
    SET_VECTOR_ELT(rList, 3, spaiAlldiffL);
    SET_VECTOR_ELT(rList, 4, spaiAlldiffU);
    SET_VECTOR_ELT(rList, 5, spaiAlldiffBeg);
    SET_VECTOR_ELT(rList, 6, spaiAlldiffVar);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetALLDIFFDatai
 * @param[in,out] pModel
 * @param[in,out] iALLDIFF
 * @param[in,out] piAlldiffDim
 * @param[in,out] piAlldiffL
 * @param[in,out] piAlldiffU
 * @param[in,out] paiAlldiffVar
 * @return int An integer error code
 * @remark ErrorCode,piAlldiffDim,piAlldiffL,piAlldiffU,paiAlldiffVar = rLSgetALLDIFFDatai(spModel,iALLDIFF)
 */
SEXP rcLSgetALLDIFFDatai(SEXP spModel,SEXP siALLDIFF)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int iALLDIFF;
  int *piAlldiffDim= NULL;
  int *piAlldiffL= NULL;
  int *piAlldiffU= NULL;
  int *paiAlldiffVar= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[5] = {"ErrorCode","piAlldiffDim","piAlldiffL","piAlldiffU","paiAlldiffVar"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 5;
  int       nIdx, nProtect = 0;

  SEXP spiAlldiffDim= R_NilValue;
  SEXP spiAlldiffL= R_NilValue;
  SEXP spiAlldiffU= R_NilValue;
  SEXP spaiAlldiffVar= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = iALLDIFF = Rf_asInteger(siALLDIFF);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);


  // Get C pointers
  PROTECT(spiAlldiffDim = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = piAlldiffDim = INTEGER_POINTER(spiAlldiffDim); //piAlldiffDim

  PROTECT(spiAlldiffL = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[4] = piAlldiffL = INTEGER_POINTER(spiAlldiffL); //piAlldiffL

  PROTECT(spiAlldiffU = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[5] = piAlldiffU = INTEGER_POINTER(spiAlldiffU); //piAlldiffU

  *pnErrorCode = errorcode = LSgetALLDIFFDatai(pModel,ibuf[2],ivecptr[3],ivecptr[4],ivecptr[5],NULL);
  if (errorcode) goto ErrorReturn;

  PROTECT(spaiAlldiffVar = NEW_INTEGER(*ivecptr[3]));
  nProtect += 1;
  ivecptr[6] = paiAlldiffVar = INTEGER_POINTER(spaiAlldiffVar); //paiAlldiffVar

  *pnErrorCode = errorcode = LSgetALLDIFFDatai(pModel
    ,ibuf[2] //iALLDIFF
    ,ivecptr[3] //*piAlldiffDim
    ,ivecptr[4] //*piAlldiffL
    ,ivecptr[5] //*piAlldiffU
    ,ivecptr[6]); //*paiAlldiffVar


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spiAlldiffDim);
    SET_VECTOR_ELT(rList, 2, spiAlldiffL);
    SET_VECTOR_ELT(rList, 3, spiAlldiffU);
    SET_VECTOR_ELT(rList, 4, spaiAlldiffVar);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetDimensions
 * @param[in,out] pModel
 * @param[in,out] pnVars
 * @param[in,out] pnCons
 * @param[in,out] pnCones
 * @param[in,out] pnAnnz
 * @param[in,out] pnQCnnz
 * @param[in,out] pnConennz
 * @param[in,out] pnNLPnnz
 * @param[in,out] pnNLPobjnnz
 * @param[in,out] pnVarNamelen
 * @param[in,out] pnConNamelen
 * @param[in,out] pnConeNamelen
 * @return int An integer error code
 * @remark ErrorCode,pnVars,pnCons,pnCones,pnAnnz,pnQCnnz,pnConennz,pnNLPnnz,pnNLPobjnnz,pnVarNamelen,pnConNamelen,pnConeNamelen = rLSgetDimensions(spModel)
 */
SEXP rcLSgetDimensions(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnVars= NULL;
  int *pnCons= NULL;
  int *pnCones= NULL;
  int *pnAnnz= NULL;
  int *pnQCnnz= NULL;
  int *pnConennz= NULL;
  int *pnNLPnnz= NULL;
  int *pnNLPobjnnz= NULL;
  int *pnVarNamelen= NULL;
  int *pnConNamelen= NULL;
  int *pnConeNamelen= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[12] = {"ErrorCode","pnVars","pnCons","pnCones","pnAnnz","pnQCnnz","pnConennz","pnNLPnnz","pnNLPobjnnz","pnVarNamelen","pnConNamelen","pnConeNamelen"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 12;
  int       nIdx, nProtect = 0;

  SEXP spnVars= R_NilValue;
  SEXP spnCons= R_NilValue;
  SEXP spnCones= R_NilValue;
  SEXP spnAnnz= R_NilValue;
  SEXP spnQCnnz= R_NilValue;
  SEXP spnConennz= R_NilValue;
  SEXP spnNLPnnz= R_NilValue;
  SEXP spnNLPobjnnz= R_NilValue;
  SEXP spnVarNamelen= R_NilValue;
  SEXP spnConNamelen= R_NilValue;
  SEXP spnConeNamelen= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnVars = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnVars = INTEGER_POINTER(spnVars); //pnVars

  PROTECT(spnCons = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnCons = INTEGER_POINTER(spnCons); //pnCons

  PROTECT(spnCones = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[4] = pnCones = INTEGER_POINTER(spnCones); //pnCones

  PROTECT(spnAnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[5] = pnAnnz = INTEGER_POINTER(spnAnnz); //pnAnnz

  PROTECT(spnQCnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[6] = pnQCnnz = INTEGER_POINTER(spnQCnnz); //pnQCnnz

  PROTECT(spnConennz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[7] = pnConennz = INTEGER_POINTER(spnConennz); //pnConennz

  PROTECT(spnNLPnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[8] = pnNLPnnz = INTEGER_POINTER(spnNLPnnz); //pnNLPnnz

  PROTECT(spnNLPobjnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[9] = pnNLPobjnnz = INTEGER_POINTER(spnNLPobjnnz); //pnNLPobjnnz

  PROTECT(spnVarNamelen = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[10] = pnVarNamelen = INTEGER_POINTER(spnVarNamelen); //pnVarNamelen

  PROTECT(spnConNamelen = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[11] = pnConNamelen = INTEGER_POINTER(spnConNamelen); //pnConNamelen

  PROTECT(spnConeNamelen = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[12] = pnConeNamelen = INTEGER_POINTER(spnConeNamelen); //pnConeNamelen

  *pnErrorCode = errorcode = LSgetDimensions(pModel
    ,ivecptr[2] //*pnVars
    ,ivecptr[3] //*pnCons
    ,ivecptr[4] //*pnCones
    ,ivecptr[5] //*pnAnnz
    ,ivecptr[6] //*pnQCnnz
    ,ivecptr[7] //*pnConennz
    ,ivecptr[8] //*pnNLPnnz
    ,ivecptr[9] //*pnNLPobjnnz
    ,ivecptr[10] //*pnVarNamelen
    ,ivecptr[11] //*pnConNamelen
    ,ivecptr[12]); //*pnConeNamelen


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnVars);
    SET_VECTOR_ELT(rList, 2, spnCons);
    SET_VECTOR_ELT(rList, 3, spnCones);
    SET_VECTOR_ELT(rList, 4, spnAnnz);
    SET_VECTOR_ELT(rList, 5, spnQCnnz);
    SET_VECTOR_ELT(rList, 6, spnConennz);
    SET_VECTOR_ELT(rList, 7, spnNLPnnz);
    SET_VECTOR_ELT(rList, 8, spnNLPobjnnz);
    SET_VECTOR_ELT(rList, 9, spnVarNamelen);
    SET_VECTOR_ELT(rList, 10, spnConNamelen);
    SET_VECTOR_ELT(rList, 11, spnConeNamelen);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetDualMIPsolution
 * @param[in,out] pModel
 * @param[in,out] padPrimal
 * @param[in,out] padDual
 * @param[in,out] padRedcosts
 * @param[in,out] panCstatus
 * @param[in,out] panRstatus
 * @return int An integer error code
 * @remark ErrorCode,padPrimal,padDual,padRedcosts,panCstatus,panRstatus = rLSgetDualMIPsolution(spModel)
 */
SEXP rcLSgetDualMIPsolution(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  double *padPrimal= NULL;
  double *padDual= NULL;
  double *padRedcosts= NULL;
  int *panCstatus= NULL;
  int *panRstatus= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[6] = {"ErrorCode","padPrimal","padDual","padRedcosts","panCstatus","panRstatus"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 6;
  int       nIdx, nProtect = 0;

  SEXP spadPrimal= R_NilValue;
  SEXP spadDual= R_NilValue;
  SEXP spadRedcosts= R_NilValue;
  SEXP spanCstatus= R_NilValue;
  SEXP spanRstatus= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spadPrimal = NEW_NUMERIC(n));
  nProtect += 1;
  dvecptr[2] = padPrimal = NUMERIC_POINTER(spadPrimal); //padPrimal

  PROTECT(spadDual = NEW_NUMERIC(m));
  nProtect += 1;
  dvecptr[3] = padDual = NUMERIC_POINTER(spadDual); //padDual

  PROTECT(spadRedcosts = NEW_NUMERIC(n));
  nProtect += 1;
  dvecptr[4] = padRedcosts = NUMERIC_POINTER(spadRedcosts); //padRedcosts

  PROTECT(spanCstatus = NEW_INTEGER(n));
  nProtect += 1;
  ivecptr[5] = panCstatus = INTEGER_POINTER(spanCstatus); //panCstatus

  PROTECT(spanRstatus = NEW_INTEGER(m));
  nProtect += 1;
  ivecptr[6] = panRstatus = INTEGER_POINTER(spanRstatus); //panRstatus

  *pnErrorCode = errorcode = LSgetDualMIPsolution(pModel
    ,dvecptr[2] //*padPrimal
    ,dvecptr[3] //*padDual
    ,dvecptr[4] //*padRedcosts
    ,ivecptr[5] //*panCstatus
    ,ivecptr[6]); //*panRstatus


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spadPrimal);
    SET_VECTOR_ELT(rList, 2, spadDual);
    SET_VECTOR_ELT(rList, 3, spadRedcosts);
    SET_VECTOR_ELT(rList, 4, spanCstatus);
    SET_VECTOR_ELT(rList, 5, spanRstatus);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetDuplicateColumns
 * @param[in,out] pModel
 * @param[in,out] nCheckVals
 * @param[in,out] pnSets
 * @param[in,out] paiSetsBeg
 * @param[in,out] paiCols
 * @return int An integer error code
 * @remark ErrorCode,pnSets,paiSetsBeg,paiCols = rLSgetDuplicateColumns(spModel,nCheckVals)
 */
SEXP rcLSgetDuplicateColumns(SEXP spModel,SEXP snCheckVals)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int nCheckVals;
  int *pnSets= NULL;
  int *paiSetsBeg= NULL;
  int *paiCols= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[4] = {"ErrorCode","pnSets","paiSetsBeg","paiCols"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 4;
  int       nIdx, nProtect = 0;

  SEXP spnSets= R_NilValue;
  SEXP spaiSetsBeg= R_NilValue;
  SEXP spaiCols= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nCheckVals = Rf_asInteger(snCheckVals);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  errorcode = LSgetDuplicateColumns(pModel,ibuf[2],&ibuf[3],NULL,NULL);

  // Get C pointers
  PROTECT(spnSets = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnSets = INTEGER_POINTER(spnSets); //pnSets
  *ivecptr[3] = ibuf[3];

  PROTECT(spaiSetsBeg = NEW_INTEGER(ibuf[3]+1));
  nProtect += 1;
  ivecptr[4] = paiSetsBeg = INTEGER_POINTER(spaiSetsBeg); //paiSetsBeg

  PROTECT(spaiCols = NEW_INTEGER(ibuf[3]));
  nProtect += 1;
  ivecptr[5] = paiCols = INTEGER_POINTER(spaiCols); //paiCols

  *pnErrorCode = errorcode = LSgetDuplicateColumns(pModel
    ,ibuf[2] //nCheckVals
    ,ivecptr[3] //*pnSets
    ,ivecptr[4] //*paiSetsBeg
    ,ivecptr[5]); //*paiCols


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnSets);
    SET_VECTOR_ELT(rList, 2, spaiSetsBeg);
    SET_VECTOR_ELT(rList, 3, spaiCols);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSgetJac
 * @param[in,out] pModel
 * @param[in,out] pnJnonzeros
 * @param[in,out] pnJobjnnz
 * @param[in,out] paiJrows
 * @param[in,out] paiJcols
 * @param[in,out] padJcoef
 * @param[in,out] padX
 * @return int An integer error code
 * @remark ErrorCode,pnJnonzeros,pnJobjnnz,paiJrows,paiJcols,padJcoef = rLSgetJac(spModel,padX)
 */
SEXP rcLSgetJac(SEXP spModel,SEXP spadX)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnJnonzeros= NULL;
  int *pnJobjnnz= NULL;
  int *paiJrows= NULL;
  int *paiJcols= NULL;
  double *padJcoef= NULL;
  double *padX= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[6] = {"ErrorCode","pnJnonzeros","pnJobjnnz","paiJrows","paiJcols","padJcoef"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 6;
  int       nIdx, nProtect = 0;

  SEXP spnJnonzeros= R_NilValue;
  SEXP spnJobjnnz= R_NilValue;
  SEXP spaiJrows= R_NilValue;
  SEXP spaiJcols= R_NilValue;
  SEXP spadJcoef= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);
  MAKE_REAL_ARRAY(padX,spadX);


  // Get C pointers
  dvecptr[7] = padX;

  PROTECT(spnJnonzeros = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnJnonzeros = INTEGER_POINTER(spnJnonzeros); //pnJnonzeros

  PROTECT(spnJobjnnz = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnJobjnnz = INTEGER_POINTER(spnJobjnnz); //pnJobjnnz

  PROTECT(spaiJrows = NEW_INTEGER(m+2));
  nProtect += 1;
  ivecptr[4] = paiJrows = INTEGER_POINTER(spaiJrows); //paiJrows

  errorcode = LSgetJac(pModel,&ibuf[2],&ibuf[3],NULL,NULL,NULL,dvecptr[7]);
  if (errorcode) goto ErrorReturn;

  PROTECT(spaiJcols = NEW_INTEGER(ibuf[3]));
  nProtect += 1;
  ivecptr[5] = paiJcols = INTEGER_POINTER(spaiJcols); //paiJcols

  PROTECT(spadJcoef = NEW_NUMERIC(ibuf[3]));
  nProtect += 1;
  dvecptr[6] = padJcoef = NUMERIC_POINTER(spadJcoef); //padJcoef

  *pnErrorCode = errorcode = LSgetJac(pModel
    ,ivecptr[2] //*pnJnonzeros
    ,ivecptr[3] //*pnJobjnnz
    ,ivecptr[4] //*paiJrows
    ,ivecptr[5] //*paiJcols
    ,dvecptr[6] //*padJcoef
    ,dvecptr[7]); //*padX


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnJnonzeros);
    SET_VECTOR_ELT(rList, 2, spnJobjnnz);
    SET_VECTOR_ELT(rList, 3, spaiJrows);
    SET_VECTOR_ELT(rList, 4, spaiJcols);
    SET_VECTOR_ELT(rList, 5, spadJcoef);
    SET_VECTOR_ELT(rList, 6, spadX);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSgetIISSETs
 * @param[in,out] pModel
 * @param[in,out] pnSuf_set
 * @param[in,out] pnIIS_set
 * @param[in,out] paiSets
 * @return int An integer error code
 * @remark ErrorCode,pnSuf_set,pnIIS_set,paiSets = rLSgetIISSETs(spModel)
 */
SEXP rcLSgetIISSETs(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnSuf_set= NULL;
  int *pnIIS_set= NULL;
  int *paiSets= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[4] = {"ErrorCode","pnSuf_set","pnIIS_set","paiSets"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 4;
  int       nIdx, nProtect = 0;

  SEXP spnSuf_set= R_NilValue;
  SEXP spnIIS_set= R_NilValue;
  SEXP spaiSets= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnSuf_set = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnSuf_set = INTEGER_POINTER(spnSuf_set); //pnSuf_set

  PROTECT(spnIIS_set = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnIIS_set = INTEGER_POINTER(spnIIS_set); //pnIIS_set

  *pnErrorCode = errorcode = LSgetIISSETs(pModel
    ,ivecptr[2] //*pnSuf_set
    ,ivecptr[3] //*pnIIS_set
    ,ivecptr[4]); //*paiSets
  if (errorcode) goto ErrorReturn;

  PROTECT(spaiSets = NEW_INTEGER(*ivecptr[3]));
  nProtect += 1;
  ivecptr[4] = paiSets = INTEGER_POINTER(spaiSets); //paiSets

  *pnErrorCode = errorcode = LSgetIISSETs(pModel
    ,ivecptr[2] //*pnSuf_set
    ,ivecptr[3] //*pnIIS_set
    ,ivecptr[4]); //*paiSets


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnSuf_set);
    SET_VECTOR_ELT(rList, 2, spnIIS_set);
    SET_VECTOR_ELT(rList, 3, spaiSets);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

  /*
 * @brief LSgetIISInts
 * @param[in,out] pModel
 * @param[in,out] pnSuf_xnt
 * @param[in,out] pnIIS_xnt
 * @param[in,out] paiVars
 * @return int An integer error code
 * @remark ErrorCode,pnSuf_xnt,pnIIS_xnt,paiVars = rLSgetIISInts(spModel)
 */
SEXP rcLSgetIISInts(SEXP spModel)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnSuf_xnt= NULL;
  int *pnIIS_xnt= NULL;
  int *paiVars= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[4] = {"ErrorCode","pnSuf_xnt","pnIIS_xnt","paiVars"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 4;
  int       nIdx, nProtect = 0;

  SEXP spnSuf_xnt= R_NilValue;
  SEXP spnIIS_xnt= R_NilValue;
  SEXP spaiVars= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spnSuf_xnt = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnSuf_xnt = INTEGER_POINTER(spnSuf_xnt); //pnSuf_xnt

  PROTECT(spnIIS_xnt = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[3] = pnIIS_xnt = INTEGER_POINTER(spnIIS_xnt); //pnIIS_xnt

  *pnErrorCode = errorcode = LSgetIISInts(pModel
    ,ivecptr[2] //*pnSuf_xnt
    ,ivecptr[3] //*pnIIS_xnt
    ,ivecptr[4]); //*paiVars
  if (errorcode) goto ErrorReturn;

  PROTECT(spaiVars = NEW_INTEGER(*pnIIS_xnt));
  nProtect += 1;
  ivecptr[4] = paiVars = INTEGER_POINTER(spaiVars); //paiVars

  *pnErrorCode = errorcode = LSgetIISInts(pModel
    ,ivecptr[2] //*pnSuf_xnt
    ,ivecptr[3] //*pnIIS_xnt
    ,ivecptr[4]); //*paiVars


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnSuf_xnt);
    SET_VECTOR_ELT(rList, 2, spnIIS_xnt);
    SET_VECTOR_ELT(rList, 3, spaiVars);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


  /*
 * @brief LSgetHess
 * @param[in,out] pModel
 * @param[in,out] pnHnonzeros
 * @param[in,out] paiHrows
 * @param[in,out] paiHcol1
 * @param[in,out] paiHcol2
 * @param[in,out] padHcoef
 * @param[in,out] padX
 * @return int An integer error code
 * @remark ErrorCode,pnHnonzeros,paiHrows,paiHcol1,paiHcol2,padHcoef = rLSgetHess(spModel,padX)
 */
SEXP rcLSgetHess(SEXP spModel,SEXP spadX)
{
  DCL_BUF(20);
  pLSmodel     pModel=NULL;
  prLSmodel prModel=NULL;

  int *pnHnonzeros= NULL;
  int *paiHrows= NULL;
  int *paiHcol1= NULL;
  int *paiHcol2= NULL;
  double *padHcoef= NULL;
  double *padX= NULL;


  SEXP      sModel=spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[6] = {"ErrorCode","pnHnonzeros","paiHrows","paiHcol1","paiHcol2","padHcoef"};
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 6;
  int       nIdx, nProtect = 0;

  SEXP spnHnonzeros= R_NilValue;
  SEXP spaiHrows= R_NilValue;
  SEXP spaiHcol1= R_NilValue;
  SEXP spaiHcol2= R_NilValue;
  SEXP spadHcoef= R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;


  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);

  MAKE_REAL_ARRAY(padX,spadX);
  dvecptr[7] = padX; //padX

  errorcode = LSgetHess(pModel,&ibuf[2],NULL,NULL,NULL,NULL,dvecptr[7]);

  // Get C pointers
  PROTECT(spnHnonzeros = NEW_INTEGER(1));
  nProtect += 1;
  ivecptr[2] = pnHnonzeros = INTEGER_POINTER(spnHnonzeros); //pnHnonzeros
  *ivecptr[2] = ibuf[2];

  PROTECT(spaiHrows = NEW_INTEGER(m+2));
  nProtect += 1;
  ivecptr[3] = paiHrows = INTEGER_POINTER(spaiHrows); //paiHrows

  PROTECT(spaiHcol1 = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[4] = paiHcol1 = INTEGER_POINTER(spaiHcol1); //paiHcol1

  PROTECT(spaiHcol2 = NEW_INTEGER(ibuf[2]));
  nProtect += 1;
  ivecptr[5] = paiHcol2 = INTEGER_POINTER(spaiHcol2); //paiHcol2

  PROTECT(spadHcoef = NEW_NUMERIC(ibuf[2]));
  nProtect += 1;
  dvecptr[6] = padHcoef = NUMERIC_POINTER(spadHcoef); //padHcoef


  *pnErrorCode = errorcode = LSgetHess(pModel
    ,ivecptr[2] //*pnHnonzeros
    ,ivecptr[3] //*paiHrows
    ,ivecptr[4] //*paiHcol1
    ,ivecptr[5] //*paiHcol2
    ,dvecptr[6] //*padHcoef
    ,dvecptr[7]); //*padX


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spnHnonzeros);
    SET_VECTOR_ELT(rList, 2, spaiHrows);
    SET_VECTOR_ELT(rList, 3, spaiHcol1);
    SET_VECTOR_ELT(rList, 4, spaiHcol2);
    SET_VECTOR_ELT(rList, 5, spadHcoef);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

typedef struct rLindoData_t {
    void *pModel;
    void *sModel;
    void *pEnv;
    void *sEnv;
    void *locFunc;
    void *locData;
    void *cbFunc;
    void *cbData;
    void *mipFunc;
    void *mipData;
    void *Funcalc_func;
    void *FData;
    void *Grad_func;
    void *Grad_func2; //partial gradients
    void *GData;
    void *Hessdc_func;
    void *HDCdata;
} rLindoData_t;

/*
* @brief relayLogfunc
* @param[in,out] pModel
* @param[in,out] line
* @param[in,out] userdata
* @return None
* @remark
*/
void LS_CALLTYPE relayLogfunc(pLSmodel pModel, char *line, void *userdata)
{
    int nProtect = 0;
    SEXP R_fcall;
    rLindoData_t *rudata = (rLindoData_t*)userdata;
    SEXP sFunc = rudata->locFunc;
    SEXP sData = rudata->locData;
    SEXP sLine = R_NilValue;

    PROTECT(sLine = NEW_CHARACTER(1));
    nProtect += 1;
    SET_STRING_ELT(sLine,0,mkChar(line));

    nProtect += 1;
    PROTECT(R_fcall = lang4(sFunc, rudata->sModel, sLine, sData));

    eval(R_fcall, sData);

    UNPROTECT(nProtect);

    return;
} /*relayModelLogfunc*/

  /*
  * @brief relayLogfunc
  * @param[in,out] pModel
  * @param[in,out] line
  * @param[in,out] userdata
  * @return None
  * @remark
  */
void LS_CALLTYPE relayLogfuncEnv(pLSenv pEnv, char *line, void *userdata)
{
  int nProtect = 0;
  SEXP R_fcall;
  rLindoData_t *rudata = (rLindoData_t*)userdata;
  SEXP sFunc = rudata->locFunc;
  SEXP sData = rudata->locData;
  SEXP sLine = R_NilValue;

  PROTECT(sLine = NEW_CHARACTER(1));
  nProtect += 1;
  SET_STRING_ELT(sLine, 0, mkChar(line));

  nProtect += 1;
  PROTECT(R_fcall = lang4(sFunc, rudata->sEnv, sLine, sData));

  eval(R_fcall, sData);

  UNPROTECT(nProtect);

  return;
} /*relayModelLogfuncEnv*/

/*
* @brief LSsetModelLogfunc
* @param[in,out] sModel
* @param[in,out] sFunc
* @param[in,out] sData
* @return int
* @remark errorcode = rLSsetModelLogfunc(pModel,sFunc,sData)
*/
SEXP rcLSsetModelLogfunc(SEXP sModel, SEXP sFunc, SEXP sData)
{
    DCL_BUF(20);
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;
    rLindoData_t *rudata = NULL;

    ZERO_BUF(20);
    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    rudata = (rLindoData_t*)LSgetObjHandle(pModel, LS_PTR_MODEL, LS_REF_LOCDATA);
    if (rudata) {
        free(rudata);
    }

    if (!isFunction(sFunc)) {
        if (isNull(sFunc)) {
            *pnErrorCode = errorcode = LSsetModelLogfunc(pModel, NULL, NULL);
        }
        else {
            error("rLindo Error: arg#2 is required to be a function");
        }
    }
    else {
        rudata = malloc(sizeof(rLindoData_t));
        memset(rudata, 0, sizeof(rLindoData_t));
        if (sFunc) {
            rudata->pModel = pModel;
            rudata->sModel = sModel;
            rudata->locFunc = sFunc;
            rudata->locData = sData;
        }
        *pnErrorCode = errorcode = LSsetModelLogfunc(pModel, relayLogfunc, rudata);
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/*
* @brief LSsetModelLogfunc
* @param[in,out] sModel
* @param[in,out] sFunc
* @param[in,out] sData
* @return int
* @remark errorcode = rLSsetModelLogfunc(pModel,sFunc,sData)
*/
SEXP rcLSsetEnvLogfunc(SEXP sEnv, SEXP sFunc, SEXP sData)
{
  DCL_BUF(20);
  prLSenv prEnv;
  pLSenv  pEnv;

  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = { "ErrorCode" };
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;
  rLindoData_t *rudata = NULL;

  ZERO_BUF(20);
  //errorcode item
  INI_ERR_CODE;

  CHECK_ENV_ERROR;

  rudata = (rLindoData_t*)LSgetObjHandle(pEnv, LS_PTR_ENV, LS_REF_LOCDATA);
  if (rudata) {
    free(rudata);
  }

  if (!isFunction(sFunc)) {
    if (isNull(sFunc)) {
      *pnErrorCode = errorcode = LSsetEnvLogfunc(pEnv, NULL, NULL);
    }
    else {
      error("rLindo Error: arg#2 is required to be a function");
    }
  }
  else {
    rudata = malloc(sizeof(rLindoData_t));
    memset(rudata, 0, sizeof(rLindoData_t));
    if (sFunc) {
      rudata->pEnv = pEnv;
      rudata->sEnv = sEnv;
      rudata->locFunc = sFunc;
      rudata->locData = sData;
    }
    *pnErrorCode = errorcode = LSsetEnvLogfunc(pEnv, relayLogfuncEnv, rudata);
  }

ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  UNPROTECT(nProtect + 2);

  return rList;
}
/*
* @brief relayCallback
* @param[in,out] pModel
* @param[in,out] iLoc
* @param[in,out] userdata
* @return int retval
*/
int LS_CALLTYPE relayCallback(pLSmodel pModel, int iLoc, void *userdata) {
    DCL_BUF(20);
    int       nProtect = 0;

    SEXP R_fcall;
    rLindoData_t *rudata = (rLindoData_t*)userdata;
    SEXP sFunc = rudata->cbFunc;
    SEXP sData = rudata->cbData;
    SEXP siLoc = R_NilValue;

    ZERO_BUF(20);

    PROTECT(siLoc = NEW_INTEGER(1));
    nProtect += 1;
    ivecptr[1] = INTEGER_POINTER(siLoc);  //iLoc
    *ivecptr[1] = iLoc;

    nProtect += 1;
    PROTECT(R_fcall = lang4(sFunc, rudata->sModel, siLoc, sData));

    errorcode = asInteger(eval(R_fcall, sData));

    UNPROTECT(nProtect);
    return errorcode;
}

/*
* @brief LSsetCallback
* @param[in,out] sModel
* @param[in,out] sFunc
* @param[in,out] sData
* @return int
* @remark errorcode = rLSsetCallback(pModel,sFunc,sData)
*/
SEXP rcLSsetCallback(SEXP sModel, SEXP sFunc, SEXP sData)
{
    DCL_BUF(20);
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = {"ErrorCode"};
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;
    rLindoData_t *rudata = NULL;

    ZERO_BUF(20);
    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    rudata = (rLindoData_t*)LSgetObjHandle(pModel, LS_PTR_MODEL, LS_REF_CBDATA);
    if (rudata) {
        free(rudata);
    }
    if (!isFunction(sFunc)) {
        if (isNull(sFunc)) {
            *pnErrorCode = errorcode = LSsetCallback(pModel, NULL, NULL);
        }
        else {
            error("rLindo Error: arg#2 is required to be a function");
        }
    }
    else {
        rudata = malloc(sizeof(rLindoData_t));
        memset(rudata, 0, sizeof(rLindoData_t));
        if (sFunc) {
            rudata->pModel = pModel;
            rudata->sModel = sModel;
            rudata->cbFunc = sFunc;
            rudata->cbData = sData;
        }
        *pnErrorCode = errorcode = LSsetCallback(pModel, relayCallback, rudata);
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

SEXP lang7(SEXP s, SEXP p1, SEXP p2, SEXP p3, SEXP p4, SEXP p5, SEXP p6)
{
    PROTECT(s);
    s = LCONS(s, list6(p1, p2, p3, p4, p5, p6));
    UNPROTECT(1);
    return s;
}

int    LS_CALLTYPE relayFuncalc(pLSmodel pModel, void    *userdata,
    int      nRow, double  *padPrimal,
    int      nJDiff, double  dXJBase,
    double   *pdFuncVal, void  *pReserved)
{
    DCL_BUF(20);
    int       nProtect = 0;

    SEXP R_fcall;
    rLindoData_t *rudata = (rLindoData_t*)userdata;
    SEXP sFunc = rudata->Funcalc_func;
    SEXP sData = rudata->FData;
    SEXP snRow = R_NilValue;
    SEXP snJDiff = R_NilValue;
    SEXP sdXJBase = R_NilValue;
    SEXP spadPrimal = R_NilValue;

    ZERO_BUF(20);

    PROTECT(snRow = NEW_INTEGER(1));
    nProtect += 1;
    ivecptr[2] = INTEGER_POINTER(snRow);  //nRow
    *ivecptr[2] = nRow;

    PROTECT(snJDiff = NEW_INTEGER(1));
    nProtect += 1;
    ivecptr[3] = INTEGER_POINTER(snJDiff);  //nJDiff
    *ivecptr[3] = nJDiff;

    PROTECT(sdXJBase = NEW_NUMERIC(1));
    nProtect += 1;
    dvecptr[4] = NUMERIC_POINTER(sdXJBase);  //dXJBase
    *dvecptr[4] = dXJBase;

    errorcode = LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
    if (errorcode) goto ErrorReturn;

#if 1
    PROTECT(spadPrimal = NEW_NUMERIC(n));
    nProtect += 1;
    dvecptr[3] = NUMERIC_POINTER(spadPrimal);  //padPrimal
    memcpy(dvecptr[3], padPrimal, n * sizeof(double));
#else
    PROTECT(spadPrimal = R_MakeExternalPtr(padPrimal,  R_NilValue,  R_NilValue));
    nProtect += 1;
#endif
    PROTECT(R_fcall = lang7(sFunc, rudata->sModel, sData, snRow, spadPrimal, snJDiff, sdXJBase));
    nProtect += 1;

    *pdFuncVal = asReal(eval(R_fcall, R_GlobalEnv));

    R_ClearExternalPtr(spadPrimal);

ErrorReturn:
    UNPROTECT(nProtect);
    return errorcode;
} /*Funcalc8*/

/*
* @brief LSsetFuncalc
* @param[in,out] sModel
* @param[in,out] sFunc
* @param[in,out] sData
* @return int
* @remark errorcode = rLSsetFuncalc(pModel,sFunc,sData)
*/
SEXP rcLSsetFuncalc(SEXP sModel, SEXP sFunc, SEXP sData)
{
    DCL_BUF(20);
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = { "ErrorCode" };
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;
    rLindoData_t *rudata = NULL;

    ZERO_BUF(20);
    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    rudata = (rLindoData_t*)LSgetObjHandle(pModel, LS_PTR_MODEL, LS_REF_FDATA);
    if (rudata) {
        free(rudata);
    }
    if (!isFunction(sFunc)) {
        if (isNull(sFunc)) {
            *pnErrorCode = errorcode = LSsetFuncalc(pModel, NULL, NULL);
        }
        else {
            error("rLindo Error: arg#2 is required to be a function");
        }
    }
    else {
        rudata = malloc(sizeof(rLindoData_t));
        memset(rudata, 0, sizeof(rLindoData_t));
        if (sFunc) {
            rudata->pModel = pModel;
            rudata->sModel = sModel;
            rudata->Funcalc_func = sFunc;
            rudata->FData = sData;
        }
        *pnErrorCode = errorcode = LSsetFuncalc(pModel, relayFuncalc, rudata);
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/*
* @brief relayMIPCallback
* @param[in,out] pModel
* @param[in,out] userdata
* @param[in,out] dObjval
* @param[in,out] padPrimal
* @return int
*/
int LS_CALLTYPE relayMIPCallback(pLSmodel pModel, void *userdata, double dObjval, double *padPrimal)
{
    DCL_BUF(20);
    int       nProtect = 0;

    SEXP R_fcall;
    rLindoData_t *rudata = (rLindoData_t*)userdata;
    SEXP sFunc = rudata->mipFunc;
    SEXP sData = rudata->mipData;
    SEXP sdObjval = R_NilValue;
    SEXP spadPrimal = R_NilValue;

    ZERO_BUF(20);

    PROTECT(sdObjval = NEW_NUMERIC(1));
    nProtect += 1;
    dvecptr[2] = NUMERIC_POINTER(sdObjval);  //nRow
    *dvecptr[2] = dObjval;

    errorcode = LSgetInfo(rudata->pModel, LS_IINFO_NUM_VARS, &n);
    if (errorcode) goto ErrorReturn;

    PROTECT(spadPrimal = NEW_NUMERIC(n));
    nProtect += 1;
    dvecptr[3] = NUMERIC_POINTER(spadPrimal);  //padPrimal
    memcpy(dvecptr[3], padPrimal, n * sizeof(double));

    nProtect += 1;
    PROTECT(R_fcall = lang5(sFunc, rudata->sModel, sData, sdObjval, spadPrimal));

    errorcode = asInteger(eval(R_fcall, sData));
ErrorReturn:
    UNPROTECT(nProtect);
    return errorcode;
} /*relayMIPCallback*/

/*
* @brief LSsetMIPCallback
* @param[in,out] sModel
* @param[in,out] sFunc
* @param[in,out] sData
* @return int
* @remark errorcode = rLSsetMIPCallback(pModel,sFunc,sData)
*/
SEXP rcLSsetMIPCallback(SEXP sModel, SEXP sFunc, SEXP sData)
{
    DCL_BUF(20);
    prLSmodel prModel;
    pLSmodel  pModel;

    int       *pnErrorCode;
    SEXP      spnErrorCode = R_NilValue;
    SEXP      rList = R_NilValue;
    char      *Names[1] = { "ErrorCode" };
    SEXP      ListNames = R_NilValue;
    int       nNumItems = 1;
    int       nIdx, nProtect = 0;
    rLindoData_t *rudata = NULL;

    ZERO_BUF(20);
    //errorcode item
    INI_ERR_CODE;

    CHECK_MODEL_ERROR;

    rudata = (rLindoData_t*)LSgetObjHandle(pModel, LS_PTR_MODEL, LS_REF_MIPDATA);
    if (rudata) {
        free(rudata);
    }
    if (!isFunction(sFunc)) {
        if (isNull(sFunc)) {
            *pnErrorCode = errorcode = LSsetMIPCallback(pModel, NULL, NULL);
        }
        else {
            error("rLindo Error: arg#2 is required to be a function");
        }
    }
    else {
        rudata = malloc(sizeof(rLindoData_t));
        memset(rudata, 0, sizeof(rLindoData_t));
        if (sFunc) {
            rudata->pModel = pModel;
            rudata->sModel = sModel;
            rudata->mipFunc = sFunc;
            rudata->mipData = sData;
        }
        *pnErrorCode = errorcode = LSsetMIPCallback(pModel, relayMIPCallback, rudata);
    }

ErrorReturn:
    //allocate list
    SET_UP_LIST;

    SET_VECTOR_ELT(rList, 0, spnErrorCode);
    UNPROTECT(nProtect + 2);

    return rList;
}

/*
* @brief LSgetObjPoolParam
* @param[in,out] pModel
* @param[in,out] nObjIndex
* @param[in,out] mParam
* @param[in,out] pdValue
* @return int An integer error code
* @remark ErrorCode,pdValue = rLSgetObjPoolParam(spModel,nObjIndex,mParam)
*/
SEXP rcLSgetObjPoolParam(SEXP spModel, SEXP snObjIndex, SEXP smParam)
{
  DCL_BUF(20);
  pLSmodel     pModel = NULL;
  prLSmodel prModel = NULL;

  int nObjIndex;
  int mParam;
  double *pdValue = NULL;


  SEXP      sModel = spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[2] = { "ErrorCode","pdValue" };
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 2;
  int       nIdx, nProtect = 0;

  SEXP spdValue = R_NilValue;


  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nObjIndex = Rf_asInteger(snObjIndex);
  ibuf[3] = mParam = Rf_asInteger(smParam);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  PROTECT(spdValue = NEW_NUMERIC(1));
  nProtect += 1;
  dvecptr[4] = pdValue = NUMERIC_POINTER(spdValue); //pdValue

  *pnErrorCode = errorcode = LSgetObjPoolParam(pModel
    , ibuf[2] //nObjIndex
    , ibuf[3] //mParam
    , dvecptr[4]); //*pdValue


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
    SET_VECTOR_ELT(rList, 1, spdValue);
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


/*
* @brief LSgetProgressInfo
* @param[in,out] pModel
* @param[in,out] nLocation
* @param[in,out] nQuery
* @return int An integer error code
* @remark ErrorCode = rLSgetProgressInfo(spModel,nLocation,nQuery)
*/
SEXP rcLSgetProgressInfo(SEXP spModel, SEXP snLocation, SEXP snQuery)
{
  DCL_BUF(20);
  pLSmodel     pModel = NULL;
  prLSmodel prModel = NULL;

  int nLocation;
  int nQuery;


  SEXP      sModel = spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = { "ErrorCode" };
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nLocation = Rf_asInteger(snLocation);
  ibuf[3] = nQuery = Rf_asInteger(snQuery);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers
  *pnErrorCode = errorcode = LSgetProgressInfo(pModel
    , ibuf[2] //nLocation
    , ibuf[3]
    ,NULL); //nQuery


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}


/*
* @brief LSdeleteIndConstraints
* @param[in,out] pModel
* @param[in,out] nCons
* @param[in,out] paiCons
* @return int An integer error code
* @remark ErrorCode = rLSdeleteIndConstraints(spModel,nCons,paiCons)
*/
SEXP rcLSdeleteIndConstraints(SEXP spModel, SEXP snCons, SEXP spaiCons)
{
  DCL_BUF(20);
  pLSmodel     pModel = NULL;
  prLSmodel prModel = NULL;

  int nCons;
  int *paiCons = NULL;


  SEXP      sModel = spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = { "ErrorCode" };
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nCons = Rf_asInteger(snCons);
  MAKE_INT_ARRAY(paiCons, spaiCons);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers    
  ivecptr[3] = (int*)paiCons;

  *pnErrorCode = errorcode = LSdeleteIndConstraints(pModel
    , ibuf[2] //nCons
    , ivecptr[3]); //*paiCons


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}

/*
* @brief LSloadIndData
* @param[in,out] pModel
* @param[in,out] nIndicRows
* @param[in,out] paiIndicRows
* @param[in,out] paiIndicCols
* @param[in,out] paiIndicVals
* @return int An integer error code
* @remark ErrorCode = rLSloadIndData(spModel,nIndicRows,paiIndicRows,paiIndicCols,paiIndicVals)
*/
SEXP rcLSloadIndData(SEXP spModel, SEXP snIndicRows, SEXP spaiIndicRows, SEXP spaiIndicCols, SEXP spaiIndicVals)
{
  DCL_BUF(20);
  pLSmodel     pModel = NULL;
  prLSmodel prModel = NULL;

  int nIndicRows;
  int *paiIndicRows = NULL;
  int *paiIndicCols = NULL;
  int *paiIndicVals = NULL;


  SEXP      sModel = spModel;
  int       *pnErrorCode;
  SEXP      spnErrorCode = R_NilValue;
  SEXP      rList = R_NilValue;
  char      *Names[1] = { "ErrorCode" };
  SEXP      ListNames = R_NilValue;
  int       nNumItems = 1;
  int       nIdx, nProtect = 0;



  // zero-out temp vectors
  ZERO_BUF(20);
  INI_ERR_CODE;

  ibuf[2] = nIndicRows = Rf_asInteger(snIndicRows);
  MAKE_INT_ARRAY(paiIndicRows, spaiIndicRows);
  MAKE_INT_ARRAY(paiIndicCols, spaiIndicCols);
  MAKE_INT_ARRAY(paiIndicVals, spaiIndicVals);

  CHECK_MODEL_ERROR;
  LSgetInfo(pModel, LS_IINFO_NUM_VARS, &n);
  LSgetInfo(pModel, LS_IINFO_NUM_CONS, &m);
  LSgetInfo(pModel, LS_IINFO_NUM_NONZ, &nz);



  // Get C pointers    
  ivecptr[3] = (int*)paiIndicRows;

  ivecptr[4] = (int*)paiIndicCols;

  ivecptr[5] = (int*)paiIndicVals;

  *pnErrorCode = errorcode = LSloadIndData(pModel
    , ibuf[2] //nIndicRows
    , ivecptr[3] //*paiIndicRows
    , ivecptr[4] //*paiIndicCols
    , ivecptr[5]); //*paiIndicVals


ErrorReturn:
  //allocate list
  SET_UP_LIST;

  SET_VECTOR_ELT(rList, 0, spnErrorCode);
  if (!errorcode) {
  }
  UNPROTECT(nProtect + 2);
  return rList;

}
