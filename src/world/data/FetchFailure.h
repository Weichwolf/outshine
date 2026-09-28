#ifndef OUTSHINE_WORLD_DATA_FETCHFAILURE_H
#define OUTSHINE_WORLD_DATA_FETCHFAILURE_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "Address.h"
#include "DataKind.h"

namespace outshine::Data {

enum class FetchFailureReason : uint8_t {
  OfflineMiss,
  ProviderRefused,
  Unavailable,
  TimedOut,
  Cancelled,
  CorruptPayload,
  CapacityRefused
};

[[nodiscard]] constexpr std::string_view Name(FetchFailureReason reason) noexcept {
  switch (reason) {
    case FetchFailureReason::OfflineMiss: return "offline cache miss";
    case FetchFailureReason::ProviderRefused: return "provider refusal";
    case FetchFailureReason::Unavailable: return "transport unavailable";
    case FetchFailureReason::TimedOut: return "timeout";
    case FetchFailureReason::Cancelled: return "cancelled";
    case FetchFailureReason::CorruptPayload: return "corrupt payload";
    case FetchFailureReason::CapacityRefused: return "capacity refusal";
  }
  return "unknown failure";
}

struct FetchFailure {
  DataKind Kind = DataKind::Elevation;
  Address Requested = Address::Whole(0);
  std::optional<Address> Served;
  std::string SourceId;
  std::string SourceRevision;
  std::string SourceKey;
  FetchFailureReason Reason = FetchFailureReason::ProviderRefused;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return SourceId.capacity() + SourceRevision.capacity() + SourceKey.capacity();
  }
};

}
#endif
