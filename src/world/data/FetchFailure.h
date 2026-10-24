#ifndef OUTSHINE_WORLD_DATA_FETCHFAILURE_H
#define OUTSHINE_WORLD_DATA_FETCHFAILURE_H

#include <cstddef>
#include <optional>
#include <string>

#include "FetchFailureReason.h"
#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

struct FetchFailure {
  DataKind Kind = DataKind::Elevation;
  Address Requested = Address::Whole(0);
  std::optional<Address> Served;
  std::string SourceId;
  std::string SourceRevision;
  std::string SourceKey;
  FetchFailureReason Reason = FetchFailureReason::ProviderRefused;
  std::optional<int> HttpStatus = std::nullopt;
  int Retries = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return SourceId.capacity() + SourceRevision.capacity() + SourceKey.capacity();
  }
};

}
#endif
