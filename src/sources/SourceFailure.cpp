#include "SourceFailure.h"
#include "SourceRequest.h"
#include <format>
#include <string>

namespace outshine::Data {
std::string SourceFailure::Describe() const {
  return std::format("{} at {}; served={}; source={}; revision={}; key={}; http={}; retries={}",
                     Name(Reason),
                     SourceRequest(Kind, Requested).Key(),
                     Served ? Served->Text() : "unknown",
                     SourceId,
                     SourceRevision,
                     SourceKey,
                     HttpStatus ? std::to_string(*HttpStatus) : "none",
                     Retries);
}
}
