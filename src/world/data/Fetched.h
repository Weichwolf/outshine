#ifndef OUTSHINE_WORLD_DATA_FETCHED_H
#define OUTSHINE_WORLD_DATA_FETCHED_H

#include <optional>
#include <cstdint>
#include <utility>
#include <vector>

#include "FetchFailure.h"

namespace outshine::Data {

enum class Meaning : uint8_t { Bytes, Absent, Refused, Retry };

class Fetched {
public:
  enum class State { Working, Settled, Consumed };

  Fetched(const Fetched &) = delete;
  Fetched &operator=(const Fetched &) = delete;

  Fetched(Fetched &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        What_(std::exchange(other.What_, Meaning::Refused)),
        Bytes_(std::move(other.Bytes_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Reason_(other.Reason_) {}

  Fetched &operator=(Fetched &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    What_ = std::exchange(other.What_, Meaning::Refused);
    Bytes_ = std::move(other.Bytes_);
    RetryAfterS_ = std::exchange(other.RetryAfterS_, 0.0);
    Reason_ = other.Reason_;
    return *this;
  }

  [[nodiscard]] static Fetched Working() { return {State::Working, Meaning::Retry, {}}; }

  [[nodiscard]] static Fetched
  Meant(Meaning what, FetchFailureReason reason = FetchFailureReason::ProviderRefused) {
    Fetched made(State::Settled, what, {});
    made.Reason_ = reason;
    return made;
  }

  [[nodiscard]] static Fetched
  MeantAfter(Meaning what,
             double retryAfterS,
             FetchFailureReason reason = FetchFailureReason::ProviderRefused) {
    Fetched made(State::Settled, what, {});
    made.RetryAfterS_ = retryAfterS;
    made.Reason_ = reason;
    return made;
  }

  [[nodiscard]] static Fetched Delivered(std::vector<uint8_t> bytes) {
    return {State::Settled, Meaning::Bytes, std::move(bytes)};
  }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Settled {
    Meaning What = Meaning::Refused;
    FetchFailureReason Reason = FetchFailureReason::ProviderRefused;
    std::vector<uint8_t> Bytes;
  };

  [[nodiscard]] std::optional<Settled> Take() {
    if (Where_ != State::Settled) { return std::nullopt; }
    Where_ = State::Consumed;
    return Settled{.What = What_, .Reason = Reason_, .Bytes = std::move(Bytes_)};
  }

private:
  Fetched(State where, Meaning what, std::vector<uint8_t> bytes)
      : Where_(where), What_(what), Bytes_(std::move(bytes)) {}

  State Where_;
  Meaning What_;
  std::vector<uint8_t> Bytes_;
  double RetryAfterS_ = 0.0;
  FetchFailureReason Reason_ = FetchFailureReason::ProviderRefused;
};

}
#endif
