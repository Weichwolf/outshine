#ifndef OUTSHINE_SOURCES_SOURCETRANSPORT_H
#define OUTSHINE_SOURCES_SOURCETRANSPORT_H

#include <chrono>
#include <cstddef>
#include <thread>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SourceFailureReason.h"
#include <expected>

namespace outshine::Data {

inline constexpr int kHttpPartialContent = 206;

inline constexpr size_t MaximumEntityTagBytes = 1024;
inline constexpr size_t MaximumResponseHeaderBytes = size_t{16} * 1024;

[[nodiscard]] bool StrongEntityTag(std::string_view tag) noexcept;
// A duplicate or folded field cannot establish a single response identity.
[[nodiscard]] std::optional<std::string_view> SingleResponseHeader(std::string_view headers,
                                                                   std::string_view name) noexcept;

enum class Ticket : uint64_t { None = 0 };

struct ByteRange {
  uint64_t First = 0;
  uint64_t Length = 0;

  [[nodiscard]] bool Valid() const noexcept;

  [[nodiscard]] bool operator==(const ByteRange &) const noexcept = default;
};

struct RangeResponse {
  ByteRange Bytes;
  uint64_t TotalBytes = 0;
  std::string EntityTag;

  [[nodiscard]] bool Valid(size_t received) const noexcept;
};

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
        Range_(std::move(other.Range_)),
        Headers_(std::move(other.Headers_)),
        Reason_(other.Reason_) {}

  Wire &operator=(Wire &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    Status_ = std::exchange(other.Status_, 0);
    Body_ = std::move(other.Body_);
    RetryAfterS_ = std::exchange(other.RetryAfterS_, 0.0);
    Range_ = std::move(other.Range_);
    Headers_ = std::move(other.Headers_);
    Reason_ = other.Reason_;
    return *this;
  }

  [[nodiscard]] static Wire Working() { return {State::Working, 0, {}, 0.0}; }

  [[nodiscard]] static Wire Answered(int status,
                                     std::vector<uint8_t> body,
                                     double retryAfterS = 0.0,
                                     std::string headers = {}) {
    Wire wire(State::Answered, status, std::move(body), retryAfterS);
    wire.Headers_ = std::move(headers);
    return wire;
  }

  [[nodiscard]] static Wire
  Answered(std::vector<uint8_t> body, RangeResponse range, std::string headers = {}) {
    Wire wire(State::Answered, kHttpPartialContent, std::move(body), 0.0);
    wire.Range_ = std::move(range);
    wire.Headers_ = std::move(headers);
    return wire;
  }

  [[nodiscard]] static Wire
  Unreachable(SourceFailureReason reason = SourceFailureReason::Unavailable) {
    Wire wire(State::Unreachable, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  [[nodiscard]] static Wire
  Never(SourceFailureReason reason = SourceFailureReason::ProviderRefused) {
    Wire wire(State::Never, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  [[nodiscard]] SourceFailureReason FailureReason() const noexcept { return Reason_; }

  [[nodiscard]] State Where() const noexcept { return Where_; }

  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  struct Response {
    int Status = 0;
    std::vector<uint8_t> Body;
    std::optional<RangeResponse> Range;
    std::string Headers;
  };

  [[nodiscard]] std::optional<Response> Take() {
    if (Where_ != State::Answered) { return std::nullopt; }
    Where_ = State::Consumed;
    return Response{.Status = Status_,
                    .Body = std::move(Body_),
                    .Range = std::move(Range_),
                    .Headers = std::move(Headers_)};
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body, double retryAfterS)
      : Where_(where), Status_(status), Body_(std::move(body)), RetryAfterS_(retryAfterS) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
  double RetryAfterS_;
  std::optional<RangeResponse> Range_;
  std::string Headers_;
  SourceFailureReason Reason_ = SourceFailureReason::ProviderRefused;
};

using FetchStart = std::expected<Ticket, SourceFailureReason>;

class SourceTransport {
public:
  virtual ~SourceTransport() = default;

  [[nodiscard]] virtual FetchStart Begin(const std::string &url) = 0;

  [[nodiscard]] virtual FetchStart
  Begin(const std::string &url, ByteRange range, std::string_view entityTag = {}) {
    (void)url;
    (void)range;
    (void)entityTag;
    return std::unexpected(SourceFailureReason::ProviderRefused);
  }

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
