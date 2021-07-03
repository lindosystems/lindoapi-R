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
funcalc8 <- function(sModel,sData,nRow,x,nJDiff, dXJBase) {

  dFuncVal <- 0.    

 
  # compute objective's functional value*/
  if (nRow==-1) {        
    dFuncVal = x[1]*x[4]*(x[1] + x[2] + x[3]) + x[3]

  # compute constaint 0's functional value */
  } else if (nRow==0) {
    dFuncVal =  x[1]*x[2]*x[3]*x[4] - 25

  # compute constaint 1's functional value */
  } else if (nRow==1) {
    dFuncVal = x[1]^2 + x[2]^2 + x[3]^2 + x[4]^2 - 40
    
  } else if (nRow==2) {
    dFuncVal = x[1]
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

if (2>1) {
	#Define the model data
	#REMARK: x[1]>=0 has to be added to make sure there is at least one linear variable 
	#        This is a requirement of black-box interface
	nVars <- 4
	nCons <- 3
	nDir <- LS_MIN
	dObjConst <- 0.
	adC <- c(0., 0.,0., 0.)
	adB <- c(0., 0., 0.)
	acConTypes <- "GEG"
	nNZ <- 1
	anBegCol <- c( 0, 1, 1, 1, 1, 1)

	adA <- c(1.0)

	anRowX <- c(2)

	pdLower <- c(1.0, 1.0,1.0, 1.0)
	pdUpper <- c(5.0, 5.0,5.0, 5.0)	

	Nobjndx <- c(0,1,2,3)
	Nnlobj <- 4

	#Load LP data to the model
	rLSloadLPData(rModel, nCons, nVars, nDir, dObjConst, adC, adB, acConTypes, 
				  nNZ, anBegCol, NULL, adA, anRowX, pdLower, pdUpper)

	acVarTypes <- "CCCC"
	rLSloadVarType(rModel, acVarTypes)

	#Load NLP data
	# Length of nonlinear columns 
	Alencol <- c(2,2,2,2)

	# The indices of the first nonlinear variable in each column 
	anBegCol[1]=0
	anBegCol[2]=2
	anBegCol[3]=4
	anBegCol[4]=6
	anBegCol[5]=8	

	# The indices of nonlinear constraints 
	paiRows <- c(0,1,0,1,0,1,0,1)

	rLSloadNLPData(rModel,anBegCol,Alencol,NULL,paiRows,Nnlobj,Nobjndx,NULL);


	#Arbitrary data as 'mydata' to pass to funcalc8
	d <- c(1,2,3,4)
	e <- c("red", "white", "red", NA)
	f <- c(TRUE,TRUE,TRUE,FALSE)
	mydata <- data.frame(d,e,f)

	#R-callbacks by 'funcalc8' for feval, 'mydata' as userdata
	rLSsetFuncalc(rModel,funcalc8,mydata)
} else {
    s <- Sys.getenv("LINDOAPI_HOME")
	s <- paste(s,"/samples/data/hs71.mpi")
	rLSreadMPIFile(rModel,s);
}	

#Initial point to start NLP solver
x0 = c(1.5,1.5,1.5,1.5)
rLSloadVarStartPoint(rModel,x0)        

#Some parameters
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_FEASTOL,1e-4) 
rLSsetModelDouParameter(rModel,LS_DPARAM_NLP_REDGTOL,1e-4) 
rLSsetModelIntParameter(rModel,LS_IPARAM_NLP_SOLVER,9)  #enable multistart
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


