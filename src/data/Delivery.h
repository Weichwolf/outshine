#ifndef OUTSHINE_DATA_DELIVERY_H
#define OUTSHINE_DATA_DELIVERY_H

#include <string>
#include <utility>
#include <vector>

#include "Address.h"

namespace outshine::Data {

class Delivery {
public:
  enum class State { Delivered, Pending, Vacant, Undeclared, Refused };

  struct Answer {
    std::string SourceId;
    Address At = Address::Whole(0);
    std::vector<uint8_t> Bytes;
  };

  static Delivery From(std::string sourceId, Address at, std::vector<uint8_t> bytes) {
    Delivery d(State::Delivered);
    d.Answer_.SourceId = std::move(sourceId);
    d.Answer_.At = at;
    d.Answer_.Bytes = std::move(bytes);
    return d;
  }
  static Delivery Waiting() { return Delivery(State::Pending); }
  static Delivery Nothing() { return Delivery(State::Vacant); }
  static Delivery NoSource() { return Delivery(State::Undeclared); }
  static Delivery Wire() { return Delivery(State::Refused); }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] bool TryTake(Answer *out) {
    if (Where_ != State::Delivered) return false;
    *out = std::move(Answer_);
    return true;
  }

private:
  explicit Delivery(State where) : Where_(where) {}

  State Where_;
  Answer Answer_;
};

}
#endif
