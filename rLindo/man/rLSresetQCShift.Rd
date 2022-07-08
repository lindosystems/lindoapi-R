\name{rLSresetQCShift }
\alias{rLSresetQCShift}

\title{Reset to zero any shift previously made to diag(Q_{i}).}

\description{
 R interface function for LINDO API function \code{LSresetQCShift}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSresetQCShift(model,iRow)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{iRow}{An integer specifying the index of the QC row.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
