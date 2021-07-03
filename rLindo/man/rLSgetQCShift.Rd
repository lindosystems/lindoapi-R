\name{rLSgetQCShift }
\alias{rLSgetQCShift}

\title{Shift diag(Q_{i}) by lambda.}

\description{
 R interface function for LINDO API function \code{LSgetQCShift}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSgetQCShift(model,iRow)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{iRow}{An integer specifying the index of the QC row.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
 \item{pdShift}{The shift parameter.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
