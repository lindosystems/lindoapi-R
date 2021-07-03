\name{rLSgetQCEigs}
\alias{rLSgetQCEigs}

\title{Finds a few eigenvalues and eigenvectors of a Q matrix for the specified model.}

\description{
 R interface function for LINDO API function \code{LSgetQCEigs}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSgetQCEigs(model,iRow,pachWhich,nEigval,nCV,dTol,nMaxIter)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{iRow}{The row index of the quadratic constraint for which the eigenvalues of the associated Q matrix will be computed.}
 \item{pachWhich}{A character array specifying the type of eigenvalues to return.}
 \item{nEigval}{The Number of eigenvalues to be computed.}
 \item{nCV}{The number of columns of the matrix padEigvec.}
 \item{dTol}{Stopping tolerance which is the relative accuracy of the Ritz value.}
 \item{nMaxIter}{Maximum number of iterations.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
 \item{padEigVal}{Eigenvalues.}
 \item{paEigVec}{Eigenvectors.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
