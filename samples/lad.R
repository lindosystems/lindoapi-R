#load the package
library(rLindo)
if (!require('stringr')) install.packages('stringr',repos="http://cran.r-project.org"); library('stringr')

#Create LINDO enviroment object
rEnv <- rLScreateEnv()

#Create LINDO model object
rModel <- rLScreateModel(rEnv)

#Disable printing log
#rLSsetPrintLogNull(rModel)

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

#Solve the model
rLSoptimize(rModel,LS_METHOD_FREE)

#Get solution information
rLSgetDInfo(rModel,LS_DINFO_POBJ)
rLSgetPrimalSolution(rModel)
rLSgetDualSolution(rModel)
rLSwriteVarPriorities(rModel,"samples/var.pri",0)
#Delete the model and environment
rLSdeleteModel(rModel)
rLSdeleteEnv(rEnv)


