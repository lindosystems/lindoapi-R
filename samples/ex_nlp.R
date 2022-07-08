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

# Black-box functions (helpers)
g1 <- function(X,Y){ return (exp( -`^`(X  ,2)  - `^`(Y+1,2) ))  }   
g2 <- function(X,Y){ return (exp( -`^`(X  ,2)  - `^`(Y  ,2) ))  }
g3 <- function(X,Y){ return (exp( -`^`(X+1,2)  - `^`(Y  ,2) ))  }
f1 <- function(X,Y){ return (`^`(1-X,2)                      )  }  
f2 <- function(X,Y){ return (X/5 - `^`(X  ,3)  - `^`(Y  ,5)  )  }

# Main Black-box function
funcalc8 <- function(sModel,sData,nRow,padPrimal,nJDiff, dXJBase) {

  dFuncVal <- 0.    

  # local references for current point
  X = padPrimal[1]
  Y = padPrimal[2]
 
  # compute objective's functional value*/
  if (nRow==-1) {        
    dFuncVal = (3*f1(X,Y)*g1(X,Y)) - (10*f2(X,Y)*g2(X,Y)) - (g3(X,Y))/3  

  # compute constaint 0's functional value */
  } else if (nRow==0) {
    dFuncVal = (X*X) + Y - 6.0

  # compute constaint 1's functional value */
  } else if (nRow==1) {
    dFuncVal = X + (Y*Y) - 6.0
  } else {
    # cannot happen    
  }  

  # display user data
  #dput(sData)
  
  #return it
  dFuncVal
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

#Define the model data
nVars <- 2
nCons <- 2
nDir <- LS_MIN
dObjConst <- 0.
adC <- c(0., 0.)
adB <- c(0., 0.)
acConTypes <- "LL"
nNZ <- 4
anBegCol <- c( 0, 2, 4)

adA <- c(0.0,1.0,1.0,0.0)
         
anRowX <- c(0,1,0,1)

pdLower <- c(-3.0, -3.0)
pdUpper <- c(3.0, 3.0)

Nobjndx <- c(0,1)
Nnlobj <- 2

#Load LP data to the model
rLSloadLPData(rModel, nCons, nVars, nDir, dObjConst, adC, adB, acConTypes, 
              nNZ, anBegCol, NULL, adA, anRowX, pdLower, pdUpper)

acVarTypes <- "CC"
rLSloadVarType(rModel, acVarTypes)

#Load NLP data
# Length of nonlinear columns 
Alencol <- c(2,2)

# The number of nonlinear variables in each column 
Alencol[0]=1
Alencol[1]=1

# The indices of the first nonlinear variable in each column 
anBegCol[0]=0
anBegCol[1]=1
anBegCol[2]=2

# The indices of nonlinear constraints 
anRowX[0]=0
anRowX[1]=1

rLSloadNLPData(rModel,anBegCol,Alencol,
        NULL,anRowX,Nnlobj,Nobjndx,NULL);


#Arbitrary data as 'mydata' to pass to funcalc8
d <- c(1,2,3,4)
e <- c("red", "white", "red", NA)
f <- c(TRUE,TRUE,TRUE,FALSE)
mydata <- data.frame(d,e,f)

#R-callbacks by 'funcalc8' for feval, 'mydata' as userdata
rLSsetFuncalc(rModel,funcalc8,mydata)


#Initial point to start NLP solver
x0 = c(0.5,0.5)
rLSloadVarStartPoint(rModel,x0)        

#Some parameters
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_FEASTOL,1e-4) 
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_REDGTOL,1e-4) 
#rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_SOLVER,9)  #enable multistart
#rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_MAXLOCALSEARCH,5) #do 5 multistarts with 5 different initial solutions
#Solve the model
rLSoptimize(rModel,LS_METHOD_FREE)

#Get solution information
rLSgetDInfo(rModel,LS_DINFO_POBJ)
rLSgetPrimalSolution(rModel)
rLSgetDualSolution(rModel)
#Delete the model and environment
rLSdeleteModel(rModel)
rLSdeleteEnv(rEnv)


