#ifndef OUTSHINE_WORLD_DATA_TRANSPORT_H
#define OUTSHINE_WORLD_DATA_TRANSPORT_H

#include <chrono>
#include <thread>
#include <optional>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "FetchFailure.h"
#include <expected>

namespace outshine::Data {

enum class Ticket : uint64_t { None = 0 };

class Wire {
public:
  enum class State { Working, Answered, Unreachable, Never, Consumed };

  Wire(const Wire &) = delete;
  Wire &operator=(const Wire &) = delete;

  Wire(Wire &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        Status_(std::exchange(other.Status_, 0)),
        Body_(std::move(other.Body_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Reason_(other.Reason_) {}

  Wire &operator=(Wire &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    Status_ = std::exchange(other.Status_, 0);
    Body_ = std::move(other.Body_);
    RetryAfterS_ = std::exchange(other.RetryAfterS_, 0.0);
    Reason_ = other.Reason_;
    return *this;
  }

  [[nodiscard]] static Wire Working() { return {State::Working, 0, {}, 0.0}; }

  [[nodiscard]] static Wire Answered(int status, std::vector<uint8_t> body) {
    return {State::Answered, status, std::move(body), 0.0};
  }

  [[nodiscard]] static Wire Answered(int status, std::vector<uint8_t> body, double retryAfterS) {
    return {State::Answered, status, std::move(body), retryAfterS};
  }

  [[nodiscard]] static Wire
  Unreachable(FetchFailureReason reason = FetchFailureReason::Unavailable) {
    Wire wire(State::Unreachable, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  [[nodiscard]] static Wire Never(FetchFailureReason reason = FetchFailureReason::ProviderRefused) {
    Wire wire(State::Never, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  [[nodiscard]] FetchFailureReason FailureReason() const noexcept { return Reason_; }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Response {
    int Status = 0;
    std::vector<uint8_t> Body;
  };

  [[nodiscard]] std::optional<Response> Take() {
    if (Where_ != State::Answered) { return std::nullopt; }
    Where_ = State::Consumed;
    return Response{.Status = Status_, .Body = std::move(Body_)};
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body, double retryAfterS)
      : Where_(where), Status_(status), Body_(std::move(body)), RetryAfterS_(retryAfterS) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
  double RetryAfterS_;
  FetchFailureReason Reason_ = FetchFailureReason::ProviderRefused;
};

using FetchStart = std::expected<Ticket, FetchFailureReason>;

class Transport {
public:
  virtual ~Transport() = default;

  [[nodiscard]] virtual FetchStart Begin(const std::string &url) = 0;

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

}
#endif
