#ifndef OUTSHINE_WORLD_DATA_DELIVERY_H
#define OUTSHINE_WORLD_DATA_DELIVERY_H

#include <string>
#include <utility>
#include <optional>
#include <vector>

#include <world/data/Address.h>
#include <world/data/FetchFailure.h>

namespace outshine::Data {

class Delivery {
public:
  enum class State { Delivered, Pending, Vacant, Undeclared, Refused, Consumed };

  Delivery(const Delivery &) = delete;
  Delivery &operator=(const Delivery &) = delete;

  Delivery(Delivery &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        AfterMs_(std::exchange(other.AfterMs_, 0.0)),
        SourceId_(std::move(other.SourceId_)),
        SourceRevision_(std::move(other.SourceRevision_)),
        Answer_(std::move(other.Answer_)),
        Failure_(std::exchange(other.Failure_, std::nullopt)) {}

  Delivery &operator=(Delivery &&other) noexcept {
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
  };

  [[nodiscard]] static Delivery From(std::string sourceId,
                                     std::string sourceRevision,
                                     Address at,
                                     std::vector<uint8_t> bytes,
                                     std::string sourceKey = {}) {
    Delivery d(State::Delivered);
    d.Answer_.SourceId = std::move(sourceId);
    d.Answer_.SourceRevision = std::move(sourceRevision);
    d.Answer_.SourceKey = std::move(sourceKey);
    d.Answer_.At = at;
    d.Answer_.Bytes = std::move(bytes);
    return d;
  }

  [[nodiscard]] static Delivery Consumed() { return Delivery(State::Consumed); }

  [[nodiscard]] static Delivery Waiting() { return Delivery(State::Pending); }

  [[nodiscard]] static Delivery Nothing() { return Delivery(State::Vacant); }

  [[nodiscard]] static Delivery NoSource() { return Delivery(State::Undeclared); }

  [[nodiscard]] static Delivery Wire() { return Delivery(State::Refused); }

  [[nodiscard]] static Delivery WireAfter(double afterMs,
                                          std::string sourceId = {},
                                          std::string sourceRevision = {},
                                          std::optional<FetchFailure> failure = std::nullopt) {
    Delivery d(State::Refused);
    d.AfterMs_ = afterMs > 0.0 ? afterMs : 0.0;
    d.SourceId_ = std::move(sourceId);
    d.SourceRevision_ = std::move(sourceRevision);
    d.Failure_ = std::move(failure);
    return d;
  }

  [[nodiscard]] const std::optional<FetchFailure> &Failure() const noexcept { return Failure_; }

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
  explicit Delivery(State where) : Where_(where) {}

  State Where_;
  double AfterMs_ = 0.0;
  std::string SourceId_;
  std::string SourceRevision_;
  Answer Answer_;
  std::optional<FetchFailure> Failure_;
};

}
#endif
