#ifndef OUTSHINE_WORLD_DATA_FETCHED_H
#define OUTSHINE_WORLD_DATA_FETCHED_H

#include <optional>
#include <cstdint>
#include <utility>
#include <vector>

#include "FetchFailure.h"
#include "Transport.h"

namespace outshine::Data {

/// Provider interpretation before shared scheduling and retry policy.
enum class Meaning : uint8_t {
  Bytes,   ///< Complete declared payload or one received original interval with its receipt.
  Absent,  ///< Authoritative absence.
  Refused, ///< Terminal acquisition refusal.
  Retry    ///< Retryable acquisition failure.
};

/// Evidence supporting absence; access failures do not constitute absence.
enum class AbsenceEvidence : uint8_t {
  Unknown,     ///< No authoritative HTTP absence evidence.
  HttpNotFound ///< HTTP 404 was returned by the selected source.
};

/// Move-only provider reply owning source bytes and failure classification.
/// Working retains no ticket; the query owns active work. Take consumes a settled
/// reply once. Moves are constant-time; factories may allocate the payload vector.
class Fetched {
public:
  /// Reply lifecycle; consumed values expose no payload.
  enum class State {
    Working, ///< Work remains pending.
    Settled, ///< Terminal reply can be consumed.
    Consumed ///< Payload has already transferred or the value was moved from.
  };

  /// Payload ownership cannot be implicitly copied.
  Fetched(const Fetched &) = delete;
  /// Payload ownership cannot be implicitly duplicated.
  Fetched &operator=(const Fetched &) = delete;

  /// Transfer payload ownership without allocation.
  /// @param other Reply left consumed.
  Fetched(Fetched &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        What_(std::exchange(other.What_, Meaning::Refused)),
        Bytes_(std::move(other.Bytes_)),
        Range_(std::move(other.Range_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Reason_(other.Reason_),
        Evidence_(other.Evidence_),
        HttpStatus_(other.HttpStatus_) {}

  /// Release the old payload and transfer ownership without allocation.
  /// @param other Reply left consumed; self-assignment has no effect.
  /// @return This reply.
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

  /// Report unfinished work without allocation.
  /// @return Working reply, with no owned bytes.
  [[nodiscard]] static Fetched Working() { return {State::Working, Meaning::Retry, {}}; }

  /// Settle a classification without payload or absence evidence.
  /// @param what Result interpretation.
  /// @param reason Failure reason when relevant.
  /// @param httpStatus Observed HTTP status; empty without an HTTP response.
  /// @return Settled reply; no allocation.
  [[nodiscard]] static Fetched
  Meant(Meaning what,
        FetchFailureReason reason = FetchFailureReason::ProviderRefused,
        std::optional<int> httpStatus = std::nullopt) {
    Fetched made(State::Settled, what, {});
    made.Reason_ = reason;
    made.HttpStatus_ = httpStatus;
    return made;
  }

  /// Settle a classification with a provider retry delay.
  /// @param what Result interpretation.
  /// @param retryAfterS Retry delay in seconds; shared scheduling interprets it.
  /// @param reason Failure reason when relevant.
  /// @param httpStatus Observed HTTP status; empty without an HTTP response.
  /// @return Settled reply; no allocation.
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

  /// Record an authoritative HTTP 404 independently of other failures.
  /// @return Settled absence carrying HttpNotFound evidence; no allocation.
  [[nodiscard]] static Fetched NotFound() {
    auto made = Meant(Meaning::Absent, FetchFailureReason::ConfirmedAbsent, kHttpNotFound);
    made.Evidence_ = AbsenceEvidence::HttpNotFound;
    return made;
  }

  /// Transfer original source bytes without copying or allocating.
  /// @param bytes Owned response, validated by the native consumer.
  /// @param range Validated HTTP receipt for a partial payload; empty for a whole payload.
  /// @return Settled Bytes reply owning the vector.
  [[nodiscard]] static Fetched Delivered(std::vector<uint8_t> bytes,
                                         std::optional<RangeResponse> range = std::nullopt) {
    Fetched made(State::Settled, Meaning::Bytes, std::move(bytes));
    made.Range_ = std::move(range);
    return made;
  }

  /// Inspect the reply lifecycle.
  /// @return Current state; constant-time, no allocation.
  [[nodiscard]] State Where() const noexcept { return Where_; }

  /// Inspect the provider retry delay.
  /// @return Seconds; constant-time, no allocation.
  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  /// Owned terminal classification and encoded source payload.
  struct Settled {
    Meaning What = Meaning::Refused;                                 ///< Result interpretation.
    FetchFailureReason Reason = FetchFailureReason::ProviderRefused; ///< Failure reason.
    AbsenceEvidence Evidence = AbsenceEvidence::Unknown; ///< Authoritative absence evidence.
    std::vector<uint8_t> Bytes; ///< Owned source bytes; consumer enforces byte limits.
    std::optional<RangeResponse> Range = std::nullopt; ///< Original partial-response identity.
    std::optional<int> HttpStatus = std::nullopt; ///< HTTP status, when a response was received.
  };

  /// Consume a settled reply exactly once by moving its bytes.
  /// @return Owned terminal reply or nothing when not settled; no allocation.
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
