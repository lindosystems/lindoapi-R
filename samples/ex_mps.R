#load the package
library(rLindo)
if (!require('stringr')) install.packages('stringr',repos="http://cran.r-project.org"); library('stringr')

## Example of using the R-callback log
logFunc <- function(sModel,sLine,sData=NA) {
  print(sLine)
}

## Example of using the R-callback function
cbFunc <- function(sModel,iLoc,sData=NA) {
  retval <- 0
  iter <- rLSgetProgressIInfo(sModel,iLoc,LS_IINFO_CUR_ITER)
  dobj <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_OBJ)
  dbest <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_BEST_BOUND)
  if (iter$ErrorCode==0) {
    sout <- sprintf("iter:%d, obj:%g  bestbnd:%g (loc:%d)",as.integer(iter$pvValue),as.double(dobj$pvValue),as.double(dbest$pvValue),as.integer(iLoc))
    print(sout)
  }
  as.integer(retval)
}

## Example of using the R-callback function
cbFuncMIP <- function(sModel,sData=NA,dObj,padPrimal) {  
  retval <- 0
  iLoc <- 0
  iter <- rLSgetProgressIInfo(sModel,iLoc,LS_IINFO_CUR_ITER)
  dobj <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_OBJ)
  dbest <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_BEST_BOUND)
  if (iter$ErrorCode==0) {
    sout <- sprintf("iter:%d, obj:%g  bestbnd:%g (loc:%d)",as.integer(iter$pvValue),as.double(dobj$pvValue),as.double(dbest$pvValue),as.integer(iLoc))
    print(sout)
  }
  print(padPrimal)
  as.integer(retval)
}

#Create LINDO enviroment object
rEnv <- rLScreateEnv()

#Create LINDO model object
rModel <- rLScreateModel(rEnv)

#Disable internal printing of logs
rLSsetModelLogfunc(rModel,NULL,new.env())

#Enable R-callback log
#rLSsetModelLogfunc(rModel,logFunc,new.env())

#Enable R-callback 
#rLSsetCallback(rModel,cbFunc,new.env())
rLSsetMIPCallback(rModel,cbFuncMIP,new.env())

#Read the model from an MPS file
rLSreadMPSFile(rModel,"c:\\prob\\milp\\mps\\miplib3\\bm23.mps.gz",0)

numVars <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS)[2]$pnResult
numCont <- rLSgetIInfo(rModel,LS_IINFO_NUM_CONT)[2]$pnResult

#Solve the model
if (numCont == numVars) {
    rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_SOLVER_TIMLMT,dValue=100.0) # time limit
    rLSoptimize(rModel,LS_METHOD_FREE)
    
    #Get solution information
    status <- rLSgetIInfo(rModel,LS_IINFO_MODEL_STATUS) # integer type'd info
    obj <- rLSgetDInfo(rModel,LS_DINFO_POBJ) # double type'd info
    x <- rLSgetPrimalSolution(rModel)    
    y <- rLSgetDualSolution(rModel)    
} else {
    rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_MIP_TIMLIM,dValue=100.0) # time limit
    rLSsolveMIP(rModel)
    
    #Get solution information
    status <- rLSgetIInfo(rModel,LS_IINFO_MIP_STATUS) # integer type'd info
    obj <- rLSgetDInfo(rModel,LS_DINFO_MIP_OBJ) # double type'd info
    x <- rLSgetMIPPrimalSolution(rModel)
}

#Export model in LINDO file format
rLSwriteLINDOFile(rModel,"ex_mps.ltx")

#Write solution to a file 
rLSwriteSolution(rModel,"ex_mps.sol")

#Read back 
rLSreadVarStartPoint(rModel,"ex_mps.sol")

#Delete the model and environment
rLSdeleteModel(rModel)

#Terminate
rLSdeleteEnv(rEnv)
