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

#Create LINDO model_str object
rModel <- rLScreateModel(rEnv)

#Disable internal printing of logs
#rLSsetModelLogfunc(rModel,NULL,new.env())

#Enable R-callback log
#rLSsetModelLogfunc(rModel,logFunc,new.env())

#Enable R-callback 
#rLSsetCallback(rModel,cbFunc,new.env())

model_str <-                " minimize    x0*x3*(x0 + x1 + x2) + x2;"
model_str = paste(model_str," x0^2 + x1^2 + x2^2 + x3^2 = 40;", sep="\n")
model_str = paste(model_str," x0*x1*x2*x3 >= 25;"             , sep="\n")
model_str = paste(model_str," BOUNDS"             , sep="\n") 
model_str = paste(model_str,"  1<=x0<=5;"         , sep="\n") 
model_str = paste(model_str,"  1<=x1<=5;"         , sep="\n") 
model_str = paste(model_str,"  1<=x2<=5;"         , sep="\n") 
model_str = paste(model_str,"  1<=x3<=5;"         , sep="\n") 
model_str = paste(model_str,"  END"               , sep="\n") 


rLSreadMPXStream(rModel, model_str, nchar(model_str));

#Initial point to start NLP solver
x0 = c(1.5,1.5,1.5,1.5)
rLSloadVarStartPoint(rModel,x0)        

#Some parameters
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_FEASTOL,1e-4) 
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_REDGTOL,1e-4) 
rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_SOLVER,9)  #enable multistart
#rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_MAXLOCALSEARCH,5) #do 5 multistarts with 5 different initial solutions
#Solve the model_str
rLSoptimize(rModel,LS_METHOD_FREE)

#Get solution information
rLSgetDInfo(rModel,LS_DINFO_POBJ)
rLSgetPrimalSolution(rModel)
rLSgetDualSolution(rModel)
#Delete the model_str and environment
rLSdeleteModel(rModel)
rLSdeleteEnv(rEnv)


