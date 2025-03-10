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

### Read a model from a file into a LINDO API model object
## @param rModel LINDO API model object just initialized
## @param file character file name
lindo_read_file <- function(rModel, file) {
     #Read the model from a file
    r <- rLSreadMPSFile(rModel, file, 0)
    if (r$ErrorCode != LSERR_NO_ERROR) {
        R.utils::printf("Error %d while reading as MPS, trying LINDO format\n", r$ErrorCode)
        r <- rLSreadLINDOFile(rModel, file)
        if (r$ErrorCode != LSERR_NO_ERROR) {
            R.utils::printf("Error %d while reading as LINDO, trying LP format\n", r$ErrorCode)
            r <- rLSreadLPFile(rModel, file)
            if (r$ErrorCode != LSERR_NO_ERROR) {
                R.utils::printf("Error %d while reading as LP, trying MPX format\n", r$ErrorCode)
                r <- rLSreadMPXFile(rModel, file)
                if (r$ErrorCode != LSERR_NO_ERROR) {
                    R.utils::printf("Error %d while reading as MPX, trying MPI format\n", r$ErrorCode)
                    r <- rLSreadMPIFile(rModel, file)
                    if (r$ErrorCode != LSERR_NO_ERROR) {
                        R.utils::printf("Error %d while reading as MPI, trying NL format\n", r$ErrorCode)
                        r <- rLSreadNLFile(rModel, file)
                        if (r$ErrorCode != LSERR_NO_ERROR) {
                            rLSdeleteEnv(rEnv)
                            stop(sprintf("Error %d while reading as NL. Terminating..\n", r$ErrorCode))
                        } #
                    } # end if
                } # end if
            } # end if
        } # end if
    }
    return(r)
}

find_iis <- function(rModel, iis_level=1+2) {
    res <- rLSfindIIS(rModel,iis_level)
    if (res$ErrorCode == 0) {
        cat("IIS found\n")
        res <- rLSwriteIIS(rModel,"iis.ilp")
        if (res$ErrorCode == 0) {
            cat("IIS written to iis.ilp\n")
        } else {
            cat("Error writing IIS\n")
        }
        res <- rLSgetIIS(rModel) 
        if (res$ErrorCode == 0) {
            cat("IIS retrieved\n")
            print(res)
            cat(sprintf("\n\t ***  LSfindIIS Summary ***\n\n"))
            cat(sprintf("\t Number of Sufficient Rows = %d\n",res$pnSuf_r))
            cat(sprintf("\t Number of Sufficient Cols = %d\n",res$pnSuf_c))
            cat(sprintf("\t Number of Necessary  Rows = %d\n",res$pnIIS_r - res$pnSuf_r))
            cat(sprintf("\t Number of Necessary  Cols = %d\n",res$pnIIS_c - res$pnSuf_c))            
            # Print the IIS rows
            for (i in 1:res$pnIIS_r) {
                cat(sprintf("\t IIS Row %d: %s\n",i,res$paiCons[i]))
            }
            cat("\n")
            # Print the IIS columns
            for (i in 1:res$pnIIS_c) {
                cat(sprintf("\t IIS Col %d: %s\n",i,res$paiVars[i]))
            }            
        } else {
            cat("Error retrieving IIS\n")
        }
    }
    return(res)
}

lindoapi_solve_model <- function(rModel,time_limit = Inf,use_gop=FALSE) {
    numVars <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS)[2]$pnResult
    numCont <- rLSgetIInfo(rModel,LS_IINFO_NUM_CONT)[2]$pnResult
    has_int <- numVars > numCont
    if (has_int==FALSE) {
        ## check if implicit integers exist            
        nCards <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS_CARD)[2]$pnResult
        nSos1 <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS_SOS1)[2]$pnResult        
        nSos2 <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS_SOS2)[2]$pnResult
        nSos3 <- rLSgetIInfo(rModel,LS_IINFO_NUM_VARS_SOS3)[2]$pnResult        
        has_int <- nCards+nSos1+nSos2+nSos3 > 0
    }    
    modelType <- rLSgetIInfo(rModel,LS_IINFO_MODEL_TYPE)[2]$pnResult

    nfo = list()
    if (use_gop) {
        if ( is.finite(time_limit) ) {
            rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_GOP_TIMLIM,dValue=time_limit) # time limit
        }
        r <- rLSsolveGOP(rModel)
    }
    
    if (has_int==FALSE) {
        if (use_gop==FALSE) {
            if ( is.finite(time_limit) ) {
                rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_SOLVER_TIMLMT,dValue=time_limit) # time limit
            }            
            r <- rLSoptimize(rModel,LS_METHOD_FREE)
            nfo$method <- rLSgetIInfo(rModel,LS_IINFO_METHOD)[2]$pnResult
            nfo$type <- rLSgetIInfo(rModel,LS_IINFO_BASIC_STATUS)[2]$pnResult
        }
        r$x <- rLSgetPrimalSolution(rModel)$padPrimal
        r$pi = rLSgetDualSolution(rModel)$padDual
        r$status <- rLSgetIInfo(rModel,LS_IINFO_MODEL_STATUS)[2]$pnResult 
        r$objval <- rLSgetDInfo(rModel,LS_DINFO_POBJ)[2]$pdResult
        r$lpstat <- r$status
        r$dj = rLSgetReducedCosts(rModel)$padReducedCost
        r$slack = rLSgetSlacks(rModel)$padSlack
    } 
    else {
        if (use_gop==FALSE)  {
            if ( is.finite(time_limit) ) {
                rLSsetModelDouParameter(model=rModel,nParameter=LS_DPARAM_MIP_TIMLIM,dValue=time_limit) # time limit
            }            
            r <- rLSsolveMIP(rModel)
            r$status <- rLSgetIInfo(rModel,LS_IINFO_MIP_STATUS)[2]$pnResult 
            r$objval <- rLSgetDInfo(rModel,LS_DINFO_MIP_OBJ)[2]$pdResult
            nfo$method <- rLSgetIInfo(rModel,LS_IINFO_METHOD)[2]$pnResult
            nfo$type <- rLSgetIInfo(rModel,LS_IINFO_BASIC_STATUS)[2]$pnResult            
        } else {
            r$status <- rLSgetIInfo(rModel,LS_IINFO_GOP_STATUS)[2]$pnResult 
            r$objval <- rLSgetDInfo(rModel,LS_DINFO_GOP_OBJ)[2]$pdResult
            nfo$method <- LS_METHOD_GOP
            nfo$type <- rLSgetIInfo(rModel,LS_IINFO_GOP_STATUS)[2]$pnResult            
        }
        r$x <- rLSgetMIPPrimalSolution(rModel)$padPrimal        
        r$slack = rLSgetMIPSlacks(rModel)$padSlack        
    }

    c(r, info = nfo)
}

# Retrieve command-line arguments
args <- commandArgs(trailingOnly = TRUE)

# Print the arguments to debug
cat("Command-line arguments:", args, "\n")

# Check if the file path argument is provided
if (length(args) == 0) {
  stop("\n\tUsage: Rscript ex_mps.R <file>")
}

# Use the first argument as the file path
file <- args[1]

# Print the file path to verify the argument is passed correctly
cat("File path provided:", file, "\n")


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
#rLSsetMIPCallback(rModel,cbFuncMIP,new.env())


r <- lindo_read_file(rModel, file)

sol <- lindoapi_solve_model(rModel)

print(sol)

if (sol$pnStatus == LS_STATUS_INFEASIBLE) {
    res <- find_iis(rModel)
}
#Delete the model and environment
rLSdeleteModel(rModel)

#Terminate
rLSdeleteEnv(rEnv)

