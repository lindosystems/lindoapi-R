#load the package
library(rLindo)
if (!require('stringr')) install.packages('stringr',repos="http://cran.r-project.org"); library('stringr')


#Create LINDO enviroment object
rEnv <- rLScreateEnv()

# control parameters range
ib <- 1000
ie <- 10000
# Initialize empty lists
ls_parkey <- c()  # Using a vector to store parameter names
ls_parnum <- list()  # Using a list to map names to integer identifiers
ls_pardesc <- list()  # Using a list to map names to descriptions

# Loop through the parameter index range, empty slots are ignored (ErrorCode > 0)
for (i in ib:ie) {
    # retrieve parameter names from LINDO API environment
    r <- rLSgetParamMacroName(rEnv, i)

    # Check for successful retrieval
    if (r$ErrorCode == 0) {
        # Append parameter name to ls_parkey (use a vector for names) only if a valid parameter name is returned
        ls_parkey <- c(ls_parkey, r$szParam)

        # Map parameter name to its integer identifier in ls_parnum
        ls_parnum[[r$szParam]] <- i
        dr <- rLSgetParamShortDesc(rEnv, i)
        #
        ls_pardesc[[r$szParam]] <- dr$szDescription
    }
}

solver <- "lindoapi"
cat((paste("Registered ", length(ls_parkey), " controls for ", solver, "\n")))  
debug = TRUE
if (debug) {
    for (i in seq_along(ls_parkey)) {
        par_key <- ls_parkey[i]
        par_id <- ls_parnum[par_key]
        par_desc <- ls_pardesc[par_key]
        if (grepl("LS_IPARAM", par_key)) {
            par <- rLSgetEnvIntParameter(rEnv,par_id)
            cat((paste(ls_parkey[i], " ( ", par_id, "); default.value = ", par$pnValue, "\n"))) 
            cat((paste("Description: ", par_desc, "\n")))
        } else if (grepl("LS_DPARAM", par_key)) {
            par <- rLSgetEnvDouParameter(rEnv,par_id)
            cat((paste(ls_parkey[i], " ( ", par_id, "); default.value = ", par$pdValue, "\n")))
            cat((paste("Description: ", par_desc, "\n")))
        } 
    }
}
#Delete the  environment
rLSdeleteEnv(rEnv)
