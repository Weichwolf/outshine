#ifndef OUTSHINE_SOURCES_SOURCERESPONSE_H
#define OUTSHINE_SOURCES_SOURCERESPONSE_H

#include <optional>
#include <cstdint>
#include <utility>
#include <vector>

#include "SourceFailureReason.h"
#include "Transport.h"

namespace outshine::Data {

enum class Meaning : uint8_t { Bytes, Absent, Refused, Retry };

enum class AbsenceEvidence : uint8_t { Unknown, HttpNotFound };

class SourceResponse {
public:
  enum class State { Working, Settled, Consumed };

  SourceResponse(const SourceResponse &) = delete;

  SourceResponse &operator=(const SourceResponse &) = delete;

  SourceResponse(SourceResponse &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        What_(std::exchange(other.What_, Meaning::Refused)),
        Bytes_(std::move(other.Bytes_)),
        Range_(std::move(other.Range_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Reason_(other.Reason_),
        Evidence_(other.Evidence_),
        HttpStatus_(other.HttpStatus_) {}

  SourceResponse &operator=(SourceResponse &&other) noexcept {
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

  [[nodiscard]] static SourceResponse Working() { return {State::Working, Meaning::Retry, {}}; }

  [[nodiscard]] static SourceResponse
  Meant(Meaning what,
        SourceFailureReason reason = SourceFailureReason::ProviderRefused,
        std::optional<int> httpStatus = std::nullopt) {
    SourceResponse made(State::Settled, what, {});
    made.Reason_ = reason;
    made.HttpStatus_ = httpStatus;
    return made;
  }

  [[nodiscard]] static SourceResponse
  MeantAfter(Meaning what,
             double retryAfterS,
             SourceFailureReason reason = SourceFailureReason::ProviderRefused,
             std::optional<int> httpStatus = std::nullopt) {
    SourceResponse made(State::Settled, what, {});
    made.RetryAfterS_ = retryAfterS;
    made.Reason_ = reason;
    made.HttpStatus_ = httpStatus;
    return made;
  }

  [[nodiscard]] static SourceResponse NotFound() {
    auto made = Meant(Meaning::Absent, SourceFailureReason::ConfirmedAbsent, kHttpNotFound);
    made.Evidence_ = AbsenceEvidence::HttpNotFound;
    return made;
  }

  [[nodiscard]] static SourceResponse Delivered(std::vector<uint8_t> bytes,
                                                std::optional<RangeResponse> range = std::nullopt) {
    SourceResponse made(State::Settled, Meaning::Bytes, std::move(bytes));
    made.Range_ = std::move(range);
    return made;
  }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Settled {
    Meaning What = Meaning::Refused;
    SourceFailureReason Reason = SourceFailureReason::ProviderRefused;
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

  SourceResponse(State where, Meaning what, std::vector<uint8_t> bytes)
      : Where_(where), What_(what), Bytes_(std::move(bytes)) {}

  State Where_;
  Meaning What_;
  std::vector<uint8_t> Bytes_;
  std::optional<RangeResponse> Range_ = std::nullopt;
  double RetryAfterS_ = 0.0;
  SourceFailureReason Reason_ = SourceFailureReason::ProviderRefused;
  AbsenceEvidence Evidence_ = AbsenceEvidence::Unknown;
  std::optional<int> HttpStatus_;
};

}
#endif
