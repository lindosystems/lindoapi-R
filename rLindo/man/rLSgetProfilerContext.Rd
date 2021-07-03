\name{rLSgetProfilerContext}
\alias{rLSgetProfilerContext}

\title{Get profiler info for the specified context.}

\description{
 R interface function for LINDO API function \code{LSgetProfilerContext}. For more information,
 please refer to LINDO API User Manual.
}

\usage{
  rLSgetProfilerContext(model,mContext)
}

\arguments{
 \item{model}{A LINDO API model object, returned by \code{\link{rLScreateModel}}.}
 \item{mContext}{An integer macro specifying the profiler context.}
}

\value{
 An R list object with components:
 \item{ErrorCode}{Zero if successful, nonzero otherwise.}
 \item{szDescription}{Profiler context description.}
}

\references{
 LINDO SYSTEMS home page at www.lindo.com
}


