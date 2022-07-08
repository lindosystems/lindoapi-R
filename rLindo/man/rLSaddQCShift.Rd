\name{rLSaddQCShift}
\alias{rLSaddQCShift}

\title{Shift diag(Q_{i}) by lambda.}

\description{
 R interface function for LINDO API function \code{LSaddQCShift}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSaddQCShift(model,iRow,dShift)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{iRow}{An integer specifying the index of the QC row.}
 \item{dShift}{A double specifying the shift parameter.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
