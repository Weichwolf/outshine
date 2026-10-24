#ifndef OUTSHINE_SOURCES_FETCHFAILUREREASON_H
#define OUTSHINE_SOURCES_FETCHFAILUREREASON_H

#include <cstdint>
#include <string_view>

namespace outshine::Data {
enum class FetchFailureReason : uint8_t {
  ConfirmedAbsent,
  OfflineMiss,
  ProviderRefused,
  Unavailable,
  TimedOut,
  Cancelled,
  CorruptPayload,
  CapacityRefused,
  InvalidRequest,
  SourceChanged
};

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
    case FetchFailureReason::SourceChanged: return "source changed";
  }
  return "unknown failure";
}

}
#endif
