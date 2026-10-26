#ifndef OUTSHINE_SOURCES_SOURCEFAILURE_H
#define OUTSHINE_SOURCES_SOURCEFAILURE_H

#include <cstddef>
#include <optional>
#include <string>

#include "SourceFailureReason.h"
#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

struct SourceFailure {
  DataKind Kind = DataKind::Elevation;
  Address Requested = Address::Whole(0);
  std::optional<Address> Served;
  std::string SourceId;
  std::string SourceRevision;
  std::string SourceKey;
  SourceFailureReason Reason = SourceFailureReason::ProviderRefused;
  std::optional<int> HttpStatus = std::nullopt;
  int Retries = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return SourceId.capacity() + SourceRevision.capacity() + SourceKey.capacity();
  }
};

}
#endif
