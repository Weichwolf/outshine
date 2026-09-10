#ifndef OUTSHINE_WORLD_DATA_DELIVERY_H
#define OUTSHINE_WORLD_DATA_DELIVERY_H

#include <string>
#include <utility>
#include <optional>
#include <vector>

#include "Address.h"

namespace outshine::Data {

class Delivery {
public:
  enum class State { Delivered, Pending, Vacant, Undeclared, Refused, Consumed };

  Delivery(const Delivery &) = delete;
  Delivery &operator=(const Delivery &) = delete;

  Delivery(Delivery &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        AfterMs_(std::exchange(other.AfterMs_, 0.0)),
        Answer_(std::move(other.Answer_)) {}

  Delivery &operator=(Delivery &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    AfterMs_ = std::exchange(other.AfterMs_, 0.0);
    Answer_ = std::move(other.Answer_);
    return *this;
  }

  struct Answer {
    std::string SourceId;
    Address At = Address::Whole(0);
    std::vector<uint8_t> Bytes;
  };

  [[nodiscard]] static Delivery From(std::string sourceId, Address at, std::vector<uint8_t> bytes) {
    Delivery d(State::Delivered);
    d.Answer_.SourceId = std::move(sourceId);
    d.Answer_.At = at;
    d.Answer_.Bytes = std::move(bytes);
    return d;
  }

  [[nodiscard]] static Delivery Waiting() { return Delivery(State::Pending); }

  [[nodiscard]] static Delivery Nothing() { return Delivery(State::Vacant); }

  [[nodiscard]] static Delivery NoSource() { return Delivery(State::Undeclared); }

  [[nodiscard]] static Delivery Wire() { return Delivery(State::Refused); }

  [[nodiscard]] static Delivery WireAfter(double afterMs) {
    Delivery d(State::Refused);
    d.AfterMs_ = afterMs > 0.0 ? afterMs : 0.0;
    return d;
  }

  [[nodiscard]] double AfterMs() const noexcept { return AfterMs_; }

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
  Answer Answer_;
};

}
#endif
