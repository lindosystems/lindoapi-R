\name{rLSgetProfilerInfo}
\alias{rLSgetProfilerInfo}

\title{Get profiler info for the specified context.}

\description{
 R interface function for LINDO API function \code{LSgetProfilerInfo}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSgetProfilerInfo(model,mContext)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{mContext}{An integer macro specifying the profiler context.}
}

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
 \item{pnCalls}{The number of calls/hits to the context.}
 \item{pdElapsedTime}{The elapsed time in the context.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}


