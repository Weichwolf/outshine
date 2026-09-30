#ifndef OUTSHINE_WORLD_DATA_TRANSPORT_H
#define OUTSHINE_WORLD_DATA_TRANSPORT_H

#include <chrono>
#include <thread>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FetchFailure.h"
#include <expected>

namespace outshine::Data {

/// HTTP status for one successful, validated partial-content response.
inline constexpr int kHttpPartialContent = 206;

/// Maximum retained HTTP revision-tag bytes, including quotes.
inline constexpr size_t MaximumEntityTagBytes = 1024;

/// Validate a bounded strong HTTP ETag, including its surrounding quotes.
/// @param tag Borrowed HTTP field value; weak tags and control bytes are rejected.
/// @return True for a strong tag; no allocation, at most MaximumEntityTagBytes inspected.
[[nodiscard]] bool StrongEntityTag(std::string_view tag) noexcept;

/// Transport/provider query identity; None denotes no cancellable in-flight work.
enum class Ticket : uint64_t {
  None = 0 ///< No active cancellable query.
};

/// One requested contiguous source-byte interval; construction does not validate it.
struct ByteRange {
  uint64_t First = 0;  ///< First byte offset in the unencoded source object.
  uint64_t Length = 0; ///< Positive number of bytes; First + Length - 1 must fit uint64_t.

  /// Validate positive length and inclusive-end arithmetic without allocation.
  /// @return True when the original interval can be represented; constant-time.
  [[nodiscard]] bool Valid() const noexcept;

  /// Compare start and length without allocation.
  /// @return True for identical byte intervals.
  [[nodiscard]] bool operator==(const ByteRange &) const noexcept = default;
};

/// Validated partial-response identity, owned independently of the transport ticket.
struct RangeResponse {
  ByteRange Bytes;         ///< Exact interval supplied by this response.
  uint64_t TotalBytes = 0; ///< Known complete source-object length, including this interval.
  std::string EntityTag;   ///< Strong HTTP ETag, including quotes, for subsequent If-Match.

  /// Validate received length, complete-object bounds and a bounded strong revision tag.
  /// @param received Actual original body bytes, without a cache envelope.
  /// @return True for a consistent receipt; no allocation.
  [[nodiscard]] bool Valid(size_t received) const noexcept;
};

/// Move-only transport reply owning an HTTP body or acquisition failure.
/// Query tickets remain owned by the source/query. Take transfers an answered body
/// exactly once; moves transfer storage without allocation.
class Wire {
public:
  /// Transport reply lifecycle.
  enum class State {
    Working,     ///< Request remains active.
    Answered,    ///< An HTTP response can be consumed.
    Unreachable, ///< Transport failure.
    Never,       ///< Terminal refusal.
    Consumed     ///< Response was transferred or this value was moved from.
  };

  /// Response ownership cannot be implicitly copied.
  Wire(const Wire &) = delete;
  /// Response ownership cannot be implicitly duplicated.
  Wire &operator=(const Wire &) = delete;

  /// Transfer response ownership without allocation.
  /// @param other Value left consumed.
  Wire(Wire &&other) noexcept
      : Where_(std::exchange(other.Where_, State::Consumed)),
        Status_(std::exchange(other.Status_, 0)),
        Body_(std::move(other.Body_)),
        RetryAfterS_(std::exchange(other.RetryAfterS_, 0.0)),
        Range_(std::move(other.Range_)),
        Reason_(other.Reason_) {}

  /// Release old bytes and transfer response ownership without allocation.
  /// @param other Value left consumed; self-assignment has no effect.
  /// @return This reply.
  Wire &operator=(Wire &&other) noexcept {
    if (this == &other) { return *this; }
    Where_ = std::exchange(other.Where_, State::Consumed);
    Status_ = std::exchange(other.Status_, 0);
    Body_ = std::move(other.Body_);
    RetryAfterS_ = std::exchange(other.RetryAfterS_, 0.0);
    Range_ = std::move(other.Range_);
    Reason_ = other.Reason_;
    return *this;
  }

  /// Report unfinished transport work without allocation.
  /// @return Working reply.
  [[nodiscard]] static Wire Working() { return {State::Working, 0, {}, 0.0}; }

  /// Transfer a terminal HTTP response without copying bytes.
  /// @param status HTTP status code.
  /// @param body Owned encoded response.
  /// @return Answered reply; no allocation.
  [[nodiscard]] static Wire Answered(int status, std::vector<uint8_t> body) {
    return {State::Answered, status, std::move(body), 0.0};
  }

  /// Transfer an HTTP response and its parsed retry delay.
  /// @param status HTTP status code.
  /// @param body Owned encoded response.
  /// @param retryAfterS Retry-After delay in seconds.
  /// @return Answered reply; no allocation.
  [[nodiscard]] static Wire Answered(int status, std::vector<uint8_t> body, double retryAfterS) {
    return {State::Answered, status, std::move(body), retryAfterS};
  }

  /// Transfer validated partial bytes and their source-object identity without copying.
  /// @param body Owned bytes matching range.Bytes.Length.
  /// @param range Validated interval, complete length and strong ETag.
  /// @return HTTP-206 reply owning body and metadata; no allocation.
  [[nodiscard]] static Wire Answered(std::vector<uint8_t> body, RangeResponse range) {
    Wire wire(State::Answered, kHttpPartialContent, std::move(body), 0.0);
    wire.Range_ = std::move(range);
    return wire;
  }

  /// Report retryable transport failure without allocation.
  /// @param reason Failure classification; cancellation/capacity may be terminal.
  /// @return Unreachable reply.
  [[nodiscard]] static Wire
  Unreachable(FetchFailureReason reason = FetchFailureReason::Unavailable) {
    Wire wire(State::Unreachable, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  /// Report terminal transport refusal without allocation.
  /// @param reason Failure classification.
  /// @return Never reply.
  [[nodiscard]] static Wire Never(FetchFailureReason reason = FetchFailureReason::ProviderRefused) {
    Wire wire(State::Never, 0, {}, 0.0);
    wire.Reason_ = reason;
    return wire;
  }

  /// Inspect the failure classification.
  /// @return Stored reason; constant-time, no allocation.
  [[nodiscard]] FetchFailureReason FailureReason() const noexcept { return Reason_; }

  /// Inspect reply lifecycle.
  /// @return Current state; constant-time, no allocation.
  [[nodiscard]] State Where() const noexcept { return Where_; }

  /// Inspect parsed retry delay.
  /// @return Seconds; constant-time, no allocation.
  [[nodiscard]] double RetryAfterS() const noexcept { return RetryAfterS_; }

  /// Owned HTTP status and complete encoded response body.
  struct Response {
    int Status = 0;                     ///< HTTP status code.
    std::vector<uint8_t> Body;          ///< Owned complete response bytes.
    std::optional<RangeResponse> Range; ///< Owned validated partial-response metadata, if any.
  };

  /// Consume an answered response exactly once by moving its body.
  /// @return Owned response or nothing for other states; no allocation.
  [[nodiscard]] std::optional<Response> Take() {
    if (Where_ != State::Answered) { return std::nullopt; }
    Where_ = State::Consumed;
    return Response{.Status = Status_, .Body = std::move(Body_), .Range = std::move(Range_)};
  }

private:
  Wire(State where, int status, std::vector<uint8_t> body, double retryAfterS)
      : Where_(where), Status_(status), Body_(std::move(body)), RetryAfterS_(retryAfterS) {}

  State Where_;
  int Status_;
  std::vector<uint8_t> Body_;
  double RetryAfterS_;
  std::optional<RangeResponse> Range_;
  FetchFailureReason Reason_ = FetchFailureReason::ProviderRefused;
};

/// Owned active ticket or classified start failure; no ticket exists on failure.
using FetchStart = std::expected<Ticket, FetchFailureReason>;

/// Concurrent bounded acquisition service borrowed by sources. Implementations
/// synchronize query state and enforce their work/byte limits. Begin, Collect and
/// Cancel run on IO workers, never the render thread. Await may block an IO worker
/// for its finite caller budget. Stop outstanding work before destruction.
class Transport {
public:
  /// Destroy after all outstanding tickets have settled or been cancelled.
  virtual ~Transport() = default;

  /// Start acquisition of a copied URL without waiting for its response.
  /// @param url Borrowed URL; copy it if retained.
  /// @return Active ticket or start failure; may allocate bounded query storage.
  [[nodiscard]] virtual FetchStart Begin(const std::string &url) = 0;

  /// Start one bounded unencoded byte interval, optionally pinned to a strong ETag.
  /// @param url Borrowed URL; implementation copies retained values.
  /// @param range Positive interval fitting uint64_t and the implementation's body budget.
  /// @param entityTag Empty for initial discovery, otherwise quoted strong If-Match value.
  /// @return Active ticket or classified failure. Default refuses unsupported ranges.
  /// A successful reply validates HTTP 206, exact bytes, total length and strong ETag;
  /// ignored or mismatched ranges fail. HTTP errors retain their status. No waiting for IO.
  [[nodiscard]] virtual FetchStart
  Begin(const std::string &url, ByteRange range, std::string_view entityTag = {}) {
    (void)url;
    (void)range;
    (void)entityTag;
    return std::unexpected(FetchFailureReason::ProviderRefused);
  }

  /// Poll one active acquisition without waiting for remote IO.
  /// @param ticket Active identity returned by Begin.
  /// @return Working, owned HTTP response, retryable failure or refusal.
  [[nodiscard]] virtual Wire Collect(Ticket ticket) = 0;
  /// Cancel acquisition and release its transport-owned state.
  /// @param ticket Active identity returned by Begin; no response is consumed afterward.
  virtual void Cancel(Ticket ticket) = 0;

  /// Wait on an IO worker for bounded progress; default yields by sleeping.
  /// @param forMs Finite nonnegative wait budget in milliseconds within long range.
  /// @return Whether progress was observed; default returns false.
  [[nodiscard]] virtual bool Await(double forMs) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<long>(forMs > 0.0 ? forMs : 0.0)));
    return false;
  }

  /// Read the transport clock used for retry/deadline scheduling.
  /// @return Monotonic milliseconds; no allocation.
  [[nodiscard]] virtual double NowMs() {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
  }
};

}
#endif
