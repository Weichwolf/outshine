#ifndef OUTSHINE_WORLD_DATA_FETCHED_H
#define OUTSHINE_WORLD_DATA_FETCHED_H

#include <optional>
#include <cstdint>
#include <utility>
#include <vector>

#include "FetchFailure.h"
#include "Transport.h"

namespace outshine::Data {

enum class Meaning : uint8_t { Bytes, Absent, Refused, Retry };

enum class AbsenceEvidence : uint8_t { Unknown, HttpNotFound };

class Fetched {
public:
  enum class State { Working, Settled, Consumed };

  Fetched(const Fetched &) = delete;

  Fetched &operator=(const Fetched &) = delete;

  Fetched(Fetched &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        What_(std::exchange(other.What_, Meaning::Refused)),
        Bytes_(std::move(other.Bytes_)),
        Range_(std::move(other.Range_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Reason_(other.Reason_),
        Evidence_(other.Evidence_),
        HttpStatus_(other.HttpStatus_) {}

  Fetched &operator=(Fetched &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    What_ = std::exchange(other.What_, Meaning::Refused);
    Bytes_ = std::move(other.Bytes_);
    Range_ = std::move(other.Range_);
    RetryAfterS_ = std::exchange(other.RetryAfterS_, 0.0);
    Reason_ = other.Reason_;
    Evidence_ = other.Evidence_;
    HttpStatus_ = other.HttpStatus_;
    return *this;
  }

  [[nodiscard]] static Fetched Working() { return {State::Working, Meaning::Retry, {}}; }

  [[nodiscard]] static Fetched
  Meant(Meaning what,
        FetchFailureReason reason = FetchFailureReason::ProviderRefused,
        std::optional<int> httpStatus = std::nullopt) {
    Fetched made(State::Settled, what, {});
    made.Reason_ = reason;
    made.HttpStatus_ = httpStatus;
    return made;
  }

  [[nodiscard]] static Fetched
  MeantAfter(Meaning what,
             double retryAfterS,
             FetchFailureReason reason = FetchFailureReason::ProviderRefused,
             std::optional<int> httpStatus = std::nullopt) {
    Fetched made(State::Settled, what, {});
    made.RetryAfterS_ = retryAfterS;
    made.Reason_ = reason;
    made.HttpStatus_ = httpStatus;
    return made;
  }

  [[nodiscard]] static Fetched NotFound() {
    auto made = Meant(Meaning::Absent, FetchFailureReason::ConfirmedAbsent, kHttpNotFound);
    made.Evidence_ = AbsenceEvidence::HttpNotFound;
    return made;
  }

  [[nodiscard]] static Fetched Delivered(std::vector<uint8_t> bytes,
                                         std::optional<RangeResponse> range = std::nullopt) {
    Fetched made(State::Settled, Meaning::Bytes, std::move(bytes));
    made.Range_ = std::move(range);
    return made;
  }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Settled {
    Meaning What = Meaning::Refused;
    FetchFailureReason Reason = FetchFailureReason::ProviderRefused;
    AbsenceEvidence Evidence = AbsenceEvidence::Unknown;
    std::vector<uint8_t> Bytes;
    std::optional<RangeResponse> Range = std::nullopt;
    std::optional<int> HttpStatus = std::nullopt;
  };

  [[nodiscard]] std::optional<Settled> Take() {
    if (Where_ != State::Settled) { return std::nullopt; }
    Where_ = State::Consumed;
    return Settled{.What = What_,
                   .Reason = Reason_,
                   .Evidence = Evidence_,
                   .Bytes = std::move(Bytes_),
                   .Range = std::move(Range_),
                   .HttpStatus = HttpStatus_};
  }

private:
  static constexpr int kHttpNotFound = 404;

  Fetched(State where, Meaning what, std::vector<uint8_t> bytes)
      : Where_(where), What_(what), Bytes_(std::move(bytes)) {}

  State Where_;
  Meaning What_;
  std::vector<uint8_t> Bytes_;
  std::optional<RangeResponse> Range_ = std::nullopt;
  double RetryAfterS_ = 0.0;
  FetchFailureReason Reason_ = FetchFailureReason::ProviderRefused;
  AbsenceEvidence Evidence_ = AbsenceEvidence::Unknown;
  std::optional<int> HttpStatus_;
};

}
#endif
