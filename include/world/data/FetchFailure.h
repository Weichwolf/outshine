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

/// Stable acquisition failure classification; missing data is distinct from refusal.
enum class FetchFailureReason : uint8_t {
  ConfirmedAbsent, ///< Authoritative absence, not a transport failure.
  OfflineMiss,     ///< Required bytes are absent from an offline cache.
  ProviderRefused, ///< Provider refused access or content.
  Unavailable,     ///< Transport unavailable.
  TimedOut,        ///< Acquisition deadline expired.
  Cancelled,       ///< Owner cancelled acquisition.
  CorruptPayload,  ///< Bytes fail encoding or digest validation.
  CapacityRefused, ///< A declared work/byte capacity was exceeded.
  InvalidRequest   ///< Invalid address or request.
};

/// Describe a failure without allocation.
/// @param reason Failure category.
/// @return Process-lifetime text, including unknown for unsupported values.
[[nodiscard]] constexpr std::string_view Name(FetchFailureReason reason) noexcept {
  switch (reason) {
    case FetchFailureReason::ConfirmedAbsent: return "confirmed absence";
    case FetchFailureReason::OfflineMiss: return "offline cache miss";
    case FetchFailureReason::ProviderRefused: return "provider refusal";
    case FetchFailureReason::Unavailable: return "transport unavailable";
    case FetchFailureReason::TimedOut: return "timeout";
    case FetchFailureReason::Cancelled: return "cancelled";
    case FetchFailureReason::CorruptPayload: return "corrupt payload";
    case FetchFailureReason::CapacityRefused: return "capacity refusal";
    case FetchFailureReason::InvalidRequest: return "invalid request";
  }
  return "unknown failure";
}

/// Owned diagnostic retaining requested/served addresses and actual source identity.
/// Copies may allocate strings; no source lifetime or resource ownership is retained.
struct FetchFailure {
  DataKind Kind = DataKind::Elevation;   ///< Requested native category.
  Address Requested = Address::Whole(0); ///< Original demand address.
  std::optional<Address> Served;         ///< Actual source address when selected.
  std::string SourceId;                  ///< Actual selected source ID.
  std::string SourceRevision;            ///< Actual selected revision.
  std::string SourceKey;                 ///< Endpoint-sensitive source key.
  FetchFailureReason Reason = FetchFailureReason::ProviderRefused; ///< Failure classification.

  /// Sum retained string capacities without allocation.
  /// @return Auxiliary string bytes, excluding the fixed value object and allocator overhead.
  [[nodiscard]] size_t HeapBytes() const noexcept {
    return SourceId.capacity() + SourceRevision.capacity() + SourceKey.capacity();
  }
};

}
#endif
