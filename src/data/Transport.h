#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Data {

enum class Ticket : uint64_t { None = 0 };

class Wire {
public:
  enum class State { Working, Answered, Unreachable };

  static Wire Working() { return Wire(State::Working, 0, {}); }
  static Wire Answered(int status, std::vector<uint8_t> body) {
    return Wire(State::Answered, status, std::move(body));
  }

  static Wire Unreachable() { return Wire(State::Unreachable, 0, {}); }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] bool TryTake(int *status, std::vector<uint8_t> *body) {
    if (Where_ != State::Answered) return false;
    *status = Status_;
    *body = std::move(Body_);
    return true;
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body)
      : Where_(where), Status_(status), Body_(std::move(body)) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
};

class Transport {
public:
  virtual ~Transport() = default;

  [[nodiscard]] virtual Ticket Begin(const std::string &url) = 0;

  [[nodiscard]] virtual Wire Collect(Ticket ticket) = 0;
  virtual void Cancel(Ticket ticket) = 0;
};

}
#endif
