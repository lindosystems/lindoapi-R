#load the package
library(rLindo)
if (!require('stringr')) install.packages('stringr',repos="http://cran.r-project.org"); library('stringr')


#Create LINDO enviroment object
rEnv <- rLScreateEnv()

# error codes range
ib <- 2000
ie <- 2400
# Initialize empty lists
ls_errmsg <- list()  # Using a list to store error msgs

# Loop through the error index range, empty slots are ignored (ErrorCode > 0)
for (i in ib:ie) {
    # retrieve error names from LINDO API environment
    r <- rLSgetErrorMessage(rEnv, i)
    # Check for successful retrieval
    if (nchar(r$pachMessage)>0) {
        # Map error name to its integer identifier in ls_errmsg
        ls_errmsg[[i]] <- r$pachMessage
        cat((paste("Error ", i, ":", r$pachMessage, "\n")))
    }
}

#Delete the  environment
rLSdeleteEnv(rEnv)
