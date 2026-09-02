#ifndef OUTSHINE_WORLD_DATA_TRANSPORT_H
#define OUTSHINE_WORLD_DATA_TRANSPORT_H

#include <chrono>
#include <thread>
#include <optional>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Data {

enum class Ticket : uint64_t { None = 0 };

class Wire {
public:
  enum class State { Working, Answered, Unreachable, Never };

  static Wire Working() { return {State::Working, 0, {}, 0.0}; }

  static Wire Answered(int status, std::vector<uint8_t> body) {
    return {State::Answered, status, std::move(body), 0.0};
  }

  static Wire Answered(int status, std::vector<uint8_t> body, double retryAfterS) {
    return {State::Answered, status, std::move(body), retryAfterS};
  }

  static Wire Unreachable() { return {State::Unreachable, 0, {}, 0.0}; }

  static Wire Never() { return {State::Never, 0, {}, 0.0}; }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Response {
    int Status = 0;
    std::vector<uint8_t> Body;
  };

  [[nodiscard]] std::optional<Response> Take() {
    if (Where_ != State::Answered) { return std::nullopt; }
    return Response{.Status = Status_, .Body = std::move(Body_)};
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body, double retryAfterS)
      : Where_(where), Status_(status), Body_(std::move(body)), RetryAfterS_(retryAfterS) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
  double RetryAfterS_;
};

class Transport {
public:
  virtual ~Transport() = default;

  [[nodiscard]] virtual Ticket Begin(const std::string &url) = 0;

  [[nodiscard]] virtual Wire Collect(Ticket ticket) = 0;
  virtual void Cancel(Ticket ticket) = 0;

  [[nodiscard]] virtual bool Await(double forMs) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<long>(forMs > 0.0 ? forMs : 0.0)));
    return false;
  }

  [[nodiscard]] virtual double NowMs() {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
  }
};

} // namespace outshine::Data
#endif
