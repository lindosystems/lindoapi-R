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
#rLSsetModelLogfunc(rModel,NULL,new.env())

#Enable R-callback log
#rLSsetModelLogfunc(rModel,logFunc,new.env())

#Enable R-callback 
#rLSsetCallback(rModel,cbFunc,new.env())

# Solve the model:
# min = x0*x3*(x0 + x1 + x2) + x2;
# x0^2 + x1^2 + x2^2 + x3^2 = 40;
# x0*x1*x2*x3 >= 25;
# @bnd( 1, x0, 5);
# @bnd( 1, x1, 5);
# @bnd( 1, x2, 5);
# @bnd( 1, x3, 5);

#Define the dimensions of the model
nVars <- 4
nCons <- 2
nObjs <- 1
# Number numerical constants (2, 25, and  40)
nNums <- 3
# Numerical constants in the model;
pdNumVal <- c(2.0, 25.0, 40.0)
pnObjSense <- c(LS_MIN)
# Two constraints, one =, the second >=;
pcConType <- "EG"
# Four variables, all Continuous;
pcVarType <- "CCCC"

# Initial values of variable are arbitrary;
pdVarVal <- c(1.2345678, 1.2345678, 1.2345678, 1.2345678)
# Size of the instruction list;
nlsize <- 57

# Instruction codes, from lindo.h or LINDO API documentation:
#   EP_PLUS             = 1001,
#   EP_MINUS            = 1002,
#   EP_MULTIPLY         = 1003,
#   EP_DIVIDE           = 1004,
#   EP_POWER            = 1005,
#   EP_PUSH_NUM         = 1062,
#   EP_PUSH_VAR         = 1063,
# Instruction list for a RPN/stack oriented expression processor.
# Var indexing starts at 0 rather than 1;
pnCode <- c( 
    1063,       0,
    1063,       3, 
    1003,
    1063,       0, 
    1063,       1,  
    1001, 
    1063,       2,
    1001,    
    1003,   
    1063,       2,  
    1001,
 
    1063,       0, 
    1062,       0,
    1005,   
    1063,       1,
    1062,       0, 
    1005,    
    1001, 
    1063,       2,
    1062,       0,
    1005,    
    1001,    
    1063,       3,
    1062,       0,
    1005, 
    1001,    
    1062,       2,
    1002,   
 
    1063,       0,
    1063,       1,
    1003,   
    1063,       2, 
    1003,
    1063,       3,
    1003,    
    1062,       1,
    1002)	

# Start point in instruction list and length for objective;
pnObjBeg <- c(0)
pnObjLength <- c(17)
# Begin point and length of each constraint
pnConBeg <- c(17, 43)
pnConLength <- c(26, 14)

# Lower and upper bounds on variables;
pdLowerBnd <- c(1.0, 1.0, 1.0, 1.0)
pdUpperBnd <- c(5.0, 5.0, 5.0, 5.0)

#Load NLP instruction to the model
rLSloadInstruct(rModel, nCons, nObjs, nVars, nNums, pnObjSense, 
                pcConType, pcVarType, pnCode, nlsize, NULL,
                pdNumVal, pdVarVal, pnObjBeg, pnObjLength,
                pnConBeg, pnConLength, pdLowerBnd, pdUpperBnd)

#rLSwriteMPIFile(rModel,"a.mpi");
#Some parameters
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_FEASTOL,1e-4) 
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_REDGTOL,1e-4) 
rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_SOLVER,9)  #enable multistart
#rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_MAXLOCALSEARCH,5) #do 5 multistarts with 5 different initial solutions
#Solve the model using Global solver
rLSsolveGOP(rModel)
#Solve model using local/NLP solver
#rLSoptimize(rModel,LS_METHOD_FREE)

#Get solution information
rLSgetDInfo(rModel,LS_DINFO_POBJ)
rLSgetPrimalSolution(rModel)
rLSgetDualSolution(rModel)
#Delete the model and environment
rLSdeleteModel(rModel)
rLSdeleteEnv(rEnv)


