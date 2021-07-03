\name{rLSloadPOSDData}
\alias{rLSloadPOSDData}

\title{Load the given POSD data into the LSmodel data structure.}

\description{
 R interface function for LINDO API function \code{LSloadPOSDData}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSloadPOSDData(model,nPOSD,paiPOSDdim,paiPOSDbeg,
                  paiPOSDrowndx,paiPOSDcolndx,paiPOSDvarndx)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{nPOSD}{The number of PSD diagonal blocks to load.}
 \item{paiPOSDdim}{A vector containing the dimensions of PSD diagonal blocks.}
 \item{paiPOSDbeg}{A vector containing beginning position of each PSD matrix in paiPOSDrowndx, 
                   paiPOSDcolndx and paiPOSDvarndx vectors.}
 \item{paiPOSDrowndx}{A vector specifying the row indices of variables within PSD matrix blocks.}
 \item{paiPOSDcolndx}{A vector specifying the column indices of variables within PSD matrix blocks.}
 \item{paiPOSDvarndx}{A vector specifying the original indices of variables within PSD matrix blocks.}
 }

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}
