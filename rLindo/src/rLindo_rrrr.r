
rLSgetProgressInfo <- function(spModel,nLocation,nQuery) {

  ans <- .Call("rcLSgetProgressInfo", PACKAGE = "rLindo"
        ,spModel
        ,as.integer(nLocation)
        ,as.integer(nQuery)
  )
  return (ans)
}
