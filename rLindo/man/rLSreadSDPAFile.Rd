\name{rLSreadSDPAFile}
\alias{rLSreadSDPAFile}

\title{Read SDP model from an SDPA formatted file.}

\description{
 R interface function for LINDO API function \code{LSreadSDPAFile}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSreadSDPAFile(model, pszFname)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{pszFname}{The name of the SDPA file.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
