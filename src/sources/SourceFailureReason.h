#ifndef OUTSHINE_SOURCES_SOURCEFAILUREREASON_H
#define OUTSHINE_SOURCES_SOURCEFAILUREREASON_H

#include <cstdint>
#include <string_view>

namespace outshine::Data {
enum class SourceFailureReason : uint8_t {
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

[[nodiscard]] constexpr std::string_view Name(SourceFailureReason reason) noexcept {
  switch (reason) {
    case SourceFailureReason::ConfirmedAbsent: return "confirmed absence";
    case SourceFailureReason::OfflineMiss: return "offline cache miss";
    case SourceFailureReason::ProviderRefused: return "provider refusal";
    case SourceFailureReason::Unavailable: return "transport unavailable";
    case SourceFailureReason::TimedOut: return "timeout";
    case SourceFailureReason::Cancelled: return "cancelled";
    case SourceFailureReason::CorruptPayload: return "corrupt payload";
    case SourceFailureReason::CapacityRefused: return "capacity refusal";
    case SourceFailureReason::InvalidRequest: return "invalid request";
    case SourceFailureReason::SourceChanged: return "source changed";
  }
  return "unknown failure";
}

}
#endif
