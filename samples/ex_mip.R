#load the package
library(rLindo)
if (!require('stringr')) install.packages('stringr',repos="http://cran.r-project.org"); library('stringr')

## Example of using the function
logFunc <- function(sModel,sLine,sData=NA) {
  dput(sLine)
}

## Example of using the function
cbFunc <- function(sModel,iLoc,sData=NA) {
  retval <- 0
  iter <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_ITER)  
  dobj <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_OBJ)
  dbest <- rLSgetProgressDInfo(sModel,iLoc,LS_DINFO_CUR_BEST_BOUND)
  #sout <- sprintf("iter:%g, obj:%g  bnd:%g, (loc:%d)",as.double(iter[1]),as.double(dobj[1]),as.double(dbest[1]),as.integer(iLoc))
  #print(sout)
  dput(iter)
  dput(dobj)
  dput(dbest)
  as.integer(retval)
}

#Create LINDO enviroment object
rEnv <- rLScreateEnv()

#Create LINDO model object
rModel <- rLScreateModel(rEnv)

#Disable internal printing of logs
rLSsetModelLogfunc(rModel,NULL,new.env())

#Enable R-callback logs
#rLSsetModelLogfunc(rModel,logFunc,new.env())

#Enable R-callback 
rLSsetCallback(rModel,cbFunc,new.env())

#Define the model data
nVars <- 12
nCons <- 5
nDir <- LS_MIN
dObjConst <- 0.
adC <- c(1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 0., 0.)
adB <- c( 2., 3., 4., 5., 8.)
acConTypes <- "EEEEE"
nNZ <- 20
anBegCol <- c( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20)

adA <- c(1.0,-1.0,1.0,-1.0,1.0,-1.0,1.0,-1.0,1.0,-1.0,
         1.0,1.0,1.0,1.0,1.0,1.0,2.0,4.0,6.0,7.0)
         
anRowX <- c(0,0,1,1,2,2,3,3,4,4,0,1,2,3,4,0,1,2,3,4)

pdLower <- c(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -LS_INFINITY, -LS_INFINITY)

#Load data to the model
rLSloadLPData(rModel, nCons, nVars, nDir, dObjConst, adC, adB, acConTypes, 
              nNZ, anBegCol, NULL, adA, anRowX, pdLower, NULL)

#Specify integrality of variables,  
# 3rd and 8th variables are general integer, 'I'
# 12'th is binary,  'B'
# rest are continous 'C'
acVarTypes <- "CCICCCCICCCB"
rLSloadVarType(rModel, acVarTypes)


#Set params
#rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_MIP_TIMLIM,dValue=3.0)

#Solve the model
rLSsolveMIP(rModel)

#Get solution information
rLSgetIInfo(rModel,LS_IINFO_MIP_STATUS) # integer type'd info
rLSgetDInfo(rModel,LS_DINFO_MIP_OBJ) # double type'd info

rLSgetMIPPrimalSolution(rModel)
#rLSgetDualSolution(rModel)

#Export model in LINDO file format
rLSwriteLINDOFile(rModel,"ex_mip.ltx")

#Write solution to a file 
rLSwriteSolution(rModel,"ex_mip.sol")

#Read back 
rLSreadVarStartPoint(rModel,"ex_mip.sol")

#Save it for later
sav = rLSgetVarStartPoint(rModel)

#Delete the model and environment
rLSdeleteModel(rModel)

#Re-Create LINDO model object
rModel <- rLScreateModel(rEnv)

#Import the same model 
rLSreadLINDOFile(rModel,"ex_mip.ltx")

#Now is later
rLSloadMIPVarStartPoint(rModel,sav$padPrimal)

rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_MIP_TIMLIM,dValue=3.0)
#Solve it
rLSsolveMIP(rModel)

#Etc..
rLSgetIInfo(rModel,LS_IINFO_MIP_STATUS) # integer type'd info
rLSgetDInfo(rModel,LS_DINFO_MIP_OBJ) # double type'd info
rLSgetMIPPrimalSolution(rModel)

#Terminate
rLSdeleteEnv(rEnv)


