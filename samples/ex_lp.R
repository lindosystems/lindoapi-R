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
  sout <- sprintf("iter:%d, obj:%g (loc:%d)",as.integer(iter$pvValue),as.double(dobj$pvValue),as.integer(iLoc))
  print(sout)
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

#Some parameters
rLSsetModelDouParameter(rModel,LS_DPARAM_SOLVER_FEASTOL,1e-6) 
rLSsetModelDouParameter(rModel,LS_DPARAM_SOLVER_OPTTOL,1e-6) 
#Solve the model
rLSoptimize(rModel,LS_METHOD_FREE)

#Get solution information
rLSgetDInfo(rModel,LS_DINFO_POBJ)
rLSgetPrimalSolution(rModel)
rLSgetDualSolution(rModel)
rLSwriteVarPriorities(rModel,"samples/ex_lp.pri",0)
#Delete the model and environment
rLSdeleteModel(rModel)
rLSdeleteEnv(rEnv)


