#ifndef OUTSHINE_SOURCES_SOURCEDELIVERY_H
#define OUTSHINE_SOURCES_SOURCEDELIVERY_H

#include <string>
#include <utility>
#include <optional>
#include <vector>

#include <Address.h>
#include <SourceFailure.h>
#include <SourceTransport.h>

namespace outshine::Data {

class SourceDelivery {
public:
  enum class State { Delivered, Pending, Vacant, Undeclared, Refused, Consumed };

  SourceDelivery(const SourceDelivery &) = delete;
  SourceDelivery &operator=(const SourceDelivery &) = delete;

  SourceDelivery(SourceDelivery &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        AfterMs_(std::exchange(other.AfterMs_, 0.0)),
        SourceId_(std::move(other.SourceId_)),
        SourceRevision_(std::move(other.SourceRevision_)),
        Answer_(std::move(other.Answer_)),
        Failure_(std::exchange(other.Failure_, std::nullopt)) {}

  SourceDelivery &operator=(SourceDelivery &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    AfterMs_ = std::exchange(other.AfterMs_, 0.0);
    SourceId_ = std::move(other.SourceId_);
    SourceRevision_ = std::move(other.SourceRevision_);
    Answer_ = std::move(other.Answer_);
    Failure_ = std::exchange(other.Failure_, std::nullopt);
    return *this;
  }

  struct Answer {
    std::string SourceId;
    std::string SourceRevision;
    std::string SourceKey;
    Address At = Address::Whole(0);
    std::vector<uint8_t> Bytes;
    std::optional<RangeResponse> Range = std::nullopt;
  };

  [[nodiscard]] static SourceDelivery From(std::string sourceId,
                                           std::string sourceRevision,
                                           Address at,
                                           std::vector<uint8_t> bytes,
                                           std::string sourceKey = {},
                                           std::optional<RangeResponse> range = std::nullopt) {
    SourceDelivery d(State::Delivered);
    d.Answer_.SourceId = std::move(sourceId);
    d.Answer_.SourceRevision = std::move(sourceRevision);
    d.Answer_.SourceKey = std::move(sourceKey);
    d.Answer_.At = at;
    d.Answer_.Bytes = std::move(bytes);
    d.Answer_.Range = std::move(range);
    return d;
  }

  [[nodiscard]] static SourceDelivery Consumed() { return SourceDelivery(State::Consumed); }

  [[nodiscard]] static SourceDelivery Waiting() { return SourceDelivery(State::Pending); }

  [[nodiscard]] static SourceDelivery Nothing() { return SourceDelivery(State::Vacant); }

  [[nodiscard]] static SourceDelivery NoSource() { return SourceDelivery(State::Undeclared); }

  [[nodiscard]] static SourceDelivery Wire() { return SourceDelivery(State::Refused); }

  [[nodiscard]] static SourceDelivery
  WireAfter(double afterMs,
            std::string sourceId = {},
            std::string sourceRevision = {},
            std::optional<SourceFailure> failure = std::nullopt) {
    SourceDelivery d(State::Refused);
    d.AfterMs_ = afterMs > 0.0 ? afterMs : 0.0;
    d.SourceId_ = std::move(sourceId);
    d.SourceRevision_ = std::move(sourceRevision);
    d.Failure_ = std::move(failure);
    return d;
  }

  [[nodiscard]] const std::optional<SourceFailure> &Failure() const noexcept { return Failure_; }

  [[nodiscard]] double AfterMs() const noexcept { return AfterMs_; }

  [[nodiscard]] const std::string &SourceId() const noexcept { return SourceId_; }

  [[nodiscard]] const std::string &SourceRevision() const noexcept { return SourceRevision_; }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] std::optional<Answer> Take() {
    if (Where_ != State::Delivered) { return std::nullopt; }
    Where_ = State::Consumed;
    return std::move(Answer_);
  }

private:
  explicit SourceDelivery(State where) : Where_(where) {}

  State Where_;
  double AfterMs_ = 0.0;
  std::string SourceId_;
  std::string SourceRevision_;
  Answer Answer_;
  std::optional<SourceFailure> Failure_;
};

}
#endif
