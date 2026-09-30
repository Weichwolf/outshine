#include <expected>
#include "Fetching.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace outshine {

namespace {

constexpr double kMicrosecondsPerMillisecond = 1000.0;
constexpr long kPollMostMs = 1000;
constexpr int kHttpSuccessFirst = 200;
constexpr int kHttpRedirectFirst = 300;

using Data::StrongEntityTag;

[[nodiscard]] bool HeaderName(std::string_view name, std::string_view wanted) {
  return std::ranges::equal(name, wanted, [](char left, char right) {
    return (left >= 'A' && left <= 'Z' ? left + ('a' - 'A') : left) == right;
  });
}

[[nodiscard]] std::string_view TrimHeader(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) { return {}; }
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

[[nodiscard]] std::optional<Data::RangeResponse> ParseRange(std::string_view text) {
  if (!text.starts_with("bytes ")) { return std::nullopt; }
  text.remove_prefix(6);
  uint64_t first = 0;
  uint64_t last = 0;
  uint64_t total = 0;
  const char *const end = text.end();
  const auto a = std::from_chars(text.begin(), end, first);
  if (a.ec != std::errc{} || a.ptr == end || *a.ptr != '-') { return std::nullopt; }
  const auto b = std::from_chars(a.ptr + 1, end, last);
  if (b.ec != std::errc{} || b.ptr == end || *b.ptr != '/') { return std::nullopt; }
  const auto c = std::from_chars(b.ptr + 1, end, total);
  if (c.ec != std::errc{} || c.ptr != end || last < first || last >= total) { return std::nullopt; }
  return Data::RangeResponse{
      .Bytes = {.First = first, .Length = last - first + 1}, .TotalBytes = total, .EntityTag = {}};
}

class CurlRuntime {
public:
  CurlRuntime() : Ready_(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {}

  [[nodiscard]] bool Ready() const noexcept { return Ready_; }

private:
  bool Ready_ = false;
};

[[nodiscard]] CurlRuntime &Runtime() {
  static CurlRuntime runtime;
  return runtime;
}

}

Fetching::Transfer::~Transfer() {
  curl_slist_free_all(static_cast<curl_slist *>(Headers));
}

void Fetching::Transfer::ReadHeader(std::string_view line) {
  if (!Range) { return; }
  if (line.starts_with("HTTP/")) {
    Partial.reset();
    EntityTag.clear();
    InvalidRangeHeaders = false;
    Body.clear();
    return;
  }
  const auto colon = line.find(':');
  if (colon == std::string_view::npos) { return; }
  const auto name = line.substr(0, colon);
  const auto value = TrimHeader(line.substr(colon + 1));
  if (HeaderName(name, "content-range")) {
    if (Partial) { InvalidRangeHeaders = true; }
    Partial = ParseRange(value);
    if (!Partial) { InvalidRangeHeaders = true; }
  } else if (HeaderName(name, "etag")) {
    if (!EntityTag.empty() || !StrongEntityTag(value)) {
      InvalidRangeHeaders = true;
    } else {
      EntityTag = value;
    }
  } else if (HeaderName(name, "content-encoding") && value != "identity") {
    InvalidRangeHeaders = true;
  }
}

Fetching::Fetching(Config config) : Config_(std::move(config)) {
  Config_.ConcurrentTransfers = std::max(Config_.ConcurrentTransfers, 1);
  Config_.ConnectionsPerHost = std::max(Config_.ConnectionsPerHost, 1);
  Config_.TimeoutS = std::max(Config_.TimeoutS, 1L);
  Config_.MaxRequests = std::max(Config_.MaxRequests, size_t{1});
  Config_.MaxBodyBytes = std::max(Config_.MaxBodyBytes, size_t{1});
  CancelledTickets_.reserve(Config_.MaxRequests);
  if (!Runtime().Ready()) { return; }
  CURLM *const multi = curl_multi_init();
  if (multi == nullptr) { return; }
  Multi_ = multi;
  const auto concurrent = static_cast<long>(Config_.ConcurrentTransfers);
  const auto perHost = static_cast<long>(Config_.ConnectionsPerHost);
  const bool configured =
      curl_multi_setopt(multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX) == CURLM_OK &&
      curl_multi_setopt(multi, CURLMOPT_MAX_HOST_CONNECTIONS, perHost) == CURLM_OK &&
      curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, concurrent) == CURLM_OK &&
      curl_multi_setopt(multi, CURLMOPT_MAX_CONCURRENT_STREAMS, concurrent) == CURLM_OK;
  if (!configured) {
    curl_multi_cleanup(multi);
    Multi_ = nullptr;
    return;
  }
  State_ = State::Ready;
  Worker_ = std::thread([this] { Work(); });
}

Fetching::~Fetching() {
  {
    const std::scoped_lock lock(Mutex_);
    State_ = State::Stopping;
    for (auto &[ticket, transfer] : Transfers_) {
      (void)ticket;
      transfer.Cancelled.store(true, std::memory_order_relaxed);
    }
  }
  WakeWorker();
  if (Worker_.joinable()) { Worker_.join(); }
  if (Multi_ != nullptr) {
    curl_multi_cleanup(static_cast<CURLM *>(Multi_));
    Multi_ = nullptr;
  }
}

Data::FetchStart Fetching::Begin(const std::string &url) {
  return Start(url, std::nullopt, {});
}

Data::FetchStart
Fetching::Begin(const std::string &url, Data::ByteRange range, std::string_view entityTag) {
  if (!range.Valid() || (!entityTag.empty() && !StrongEntityTag(entityTag))) {
    return std::unexpected(Data::FetchFailureReason::InvalidRequest);
  }
  if (range.Length > Config_.MaxBodyBytes) {
    return std::unexpected(Data::FetchFailureReason::CapacityRefused);
  }
  return Start(url, range, entityTag);
}

Data::FetchStart Fetching::Start(const std::string &url,
                                 std::optional<Data::ByteRange> range,
                                 std::string_view entityTag) {
  if (url.empty() || url.contains('\0')) {
    return std::unexpected(Data::FetchFailureReason::InvalidRequest);
  }
  uint64_t ticket = 0;
  {
    const std::scoped_lock lock(Mutex_);
    if (State_ != State::Ready) { return std::unexpected(Data::FetchFailureReason::Unavailable); }
    if (Transfers_.size() >= Config_.MaxRequests ||
        NextTicket_ == std::numeric_limits<uint64_t>::max()) {
      return std::unexpected(Data::FetchFailureReason::CapacityRefused);
    }
    ticket = NextTicket_++;
    Transfers_.try_emplace(ticket);
    auto &transfer = Transfers_.at(ticket);
    transfer.Ticket = ticket;
    transfer.Url = url;
    transfer.Range = range;
    transfer.IfMatch = entityTag;
    transfer.MaxBodyBytes = range ? static_cast<size_t>(range->Length) : Config_.MaxBodyBytes;
    if (range) {
      transfer.RangeText =
          std::to_string(range->First) + "-" + std::to_string(range->First + range->Length - 1);
    }
    Queue_.push_back(ticket);
  }
  WakeWorker();
  return static_cast<Data::Ticket>(ticket);
}

Data::Wire Fetching::Collect(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) { return Data::Wire::Unreachable(); }
  const std::scoped_lock lock(Mutex_);
  const auto found = Transfers_.find(static_cast<uint64_t>(ticket));
  if (found == Transfers_.end()) {
    const auto cancelled = std::ranges::find(CancelledTickets_, static_cast<uint64_t>(ticket));
    if (cancelled == CancelledTickets_.end()) { return Data::Wire::Unreachable(); }
    CancelledTickets_.erase(cancelled);
    return Data::Wire::Unreachable(Data::FetchFailureReason::Cancelled);
  }
  Transfer &done = found->second;
  if (!done.Done) { return Data::Wire::Working(); }
  const auto failure = done.Failure;
  const int status = done.Status;
  const double retryAfterS = done.RetryAfterS;
  auto partial = std::move(done.Partial);
  if (partial) { partial->EntityTag = std::move(done.EntityTag); }
  std::vector<uint8_t> body = std::move(done.Body);
  Transfers_.erase(found);
  if (failure) { return Data::Wire::Unreachable(*failure); }
  if (status == Data::kHttpPartialContent && partial) {
    return Data::Wire::Answered(std::move(body), std::move(*partial));
  }
  return Data::Wire::Answered(status, std::move(body), retryAfterS);
}

void Fetching::RememberCancellation(uint64_t ticket) {
  if (std::ranges::find(CancelledTickets_, ticket) != CancelledTickets_.end()) { return; }
  if (CancelledTickets_.size() == Config_.MaxRequests) {
    CancelledTickets_.erase(CancelledTickets_.begin());
  }
  CancelledTickets_.push_back(ticket);
}

void Fetching::Cancel(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) { return; }
  {
    const std::scoped_lock lock(Mutex_);
    const auto found = Transfers_.find(static_cast<uint64_t>(ticket));
    if (found == Transfers_.end()) { return; }
    RememberCancellation(static_cast<uint64_t>(ticket));
    if (found->second.Done) {
      Transfers_.erase(found);
      ++Completions_;
      Landed_.notify_all();
      return;
    }
    found->second.Cancelled.store(true, std::memory_order_relaxed);
    const auto queued = std::ranges::find(Queue_, static_cast<uint64_t>(ticket));
    if (queued != Queue_.end()) {
      Queue_.erase(queued);
      Transfers_.erase(found);
      ++Completions_;
      Landed_.notify_all();
      return;
    }
  }
  WakeWorker();
}

bool Fetching::Await(double forMs) {
  if (!std::isfinite(forMs) || forMs <= 0.0) { return false; }
  std::unique_lock<std::mutex> lock(Mutex_);
  const uint64_t stood = Completions_;
  if (Transfers_.empty()) { return false; }
  return Landed_.wait_for(
             lock,
             std::chrono::microseconds(static_cast<long long>(forMs * kMicrosecondsPerMillisecond)),
             [this, stood] { return State_ != State::Ready || Completions_ != stood; }) &&
         Completions_ != stood;
}

void Fetching::WakeWorker() noexcept {
  if (Multi_ != nullptr) { (void)curl_multi_wakeup(static_cast<CURLM *>(Multi_)); }
}

bool Fetching::ConfigureTransfer(void *handle, Transfer &transfer) const {
  auto *const easy = static_cast<CURL *>(handle);
  if (transfer.Range) { transfer.Body.reserve(transfer.MaxBodyBytes); }
  if (!transfer.IfMatch.empty()) {
    transfer.Headers = curl_slist_append(nullptr, ("If-Match: " + transfer.IfMatch).c_str());
    if (transfer.Headers == nullptr) { return false; }
  }
  const auto header = +[](const char *data, size_t, size_t byteCount, void *user) -> size_t {
    auto &active = *static_cast<Transfer *>(user);
    active.ReadHeader(std::string_view(data, byteCount));
    return byteCount;
  };
  const auto progress = +[](void *data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
    const auto &active = *static_cast<Transfer *>(data);
    return static_cast<int>(active.Cancelled.load(std::memory_order_relaxed));
  };
  const auto write = +[](const void *data, size_t, size_t byteCount, void *user) -> size_t {
    auto &active = *static_cast<Transfer *>(user);
    if (byteCount > active.MaxBodyBytes || active.Body.size() > active.MaxBodyBytes - byteCount) {
      active.Failure = Data::FetchFailureReason::CapacityRefused;
      return 0;
    }
    const auto *source = static_cast<const uint8_t *>(data);
    active.Body.insert(active.Body.end(), source, source + byteCount);
    return byteCount;
  };
  return curl_easy_setopt(easy, CURLOPT_URL, transfer.Url.c_str()) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_PRIVATE, static_cast<void *>(&transfer)) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_XFERINFODATA, static_cast<void *>(&transfer)) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, progress) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_WRITEDATA, static_cast<void *>(&transfer)) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, header) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HEADERDATA, static_cast<void *>(&transfer)) == CURLE_OK &&
         curl_easy_setopt(easy,
                          CURLOPT_RANGE,
                          transfer.Range ? transfer.RangeText.c_str() : nullptr) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HTTPHEADER, static_cast<curl_slist *>(transfer.Headers)) ==
             CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_TIMEOUT, Config_.TimeoutS) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, transfer.Range ? "identity" : "") ==
             CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HTTP_CONTENT_DECODING, transfer.Range ? 0L : 1L) ==
             CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_USERAGENT, Config_.UserAgent.c_str()) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS) == CURLE_OK;
}

size_t Fetching::AddQueuedTransfers(void *multiHandle, size_t active) {
  auto *const multi = static_cast<CURLM *>(multiHandle);
  const auto capacity = static_cast<size_t>(Config_.ConcurrentTransfers);
  const std::scoped_lock lock(Mutex_);
  while (State_ == State::Ready && active < capacity && !Queue_.empty()) {
    const uint64_t ticket = Queue_.front();
    Queue_.pop_front();
    const auto found = Transfers_.find(ticket);
    if (found == Transfers_.end()) { continue; }
    Transfer &transfer = found->second;
    CURL *const easy = curl_easy_init();
    if (easy != nullptr && ConfigureTransfer(easy, transfer) &&
        curl_multi_add_handle(multi, easy) == CURLM_OK) {
      transfer.Handle = easy;
      ++active;
      continue;
    }
    if (easy != nullptr) { curl_easy_cleanup(easy); }
    transfer.Done = true;
    transfer.Failure = Data::FetchFailureReason::Unavailable;
    ++Completions_;
    Landed_.notify_all();
  }
  return active;
}

void Fetching::CollectCompletions(void *multiHandle, size_t &active) {
  auto *const multi = static_cast<CURLM *>(multiHandle);
  int messages = 0;
  while (const CURLMsg *message = curl_multi_info_read(multi, &messages)) {
    if (message->msg != CURLMSG_DONE) { continue; }
    Transfer *transfer = nullptr;
    (void)curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &transfer);
    long status = 0;
    curl_off_t retryAfter = 0;
    if (message->data.result == CURLE_OK) {
      (void)curl_easy_getinfo(message->easy_handle, CURLINFO_RESPONSE_CODE, &status);
      (void)curl_easy_getinfo(message->easy_handle, CURLINFO_RETRY_AFTER, &retryAfter);
    }
    (void)curl_multi_remove_handle(multi, message->easy_handle);
    curl_easy_cleanup(message->easy_handle);
    --active;
    const std::scoped_lock lock(Mutex_);
    if (transfer == nullptr) { continue; }
    transfer->Handle = nullptr;
    if (transfer->Cancelled.load(std::memory_order_relaxed)) {
      Transfers_.erase(transfer->Ticket);
      ++Completions_;
      Landed_.notify_all();
      continue;
    }
    transfer->Done = true;
    if (message->data.result != CURLE_OK && !transfer->Failure) {
      transfer->Failure = message->data.result == CURLE_OPERATION_TIMEDOUT
                              ? Data::FetchFailureReason::TimedOut
                              : Data::FetchFailureReason::Unavailable;
    }
    if (!transfer->Failure && transfer->Range && status >= kHttpSuccessFirst &&
        status < kHttpRedirectFirst &&
        (status != Data::kHttpPartialContent || transfer->InvalidRangeHeaders ||
         !transfer->Partial || transfer->Partial->Bytes != *transfer->Range ||
         transfer->Body.size() != transfer->Range->Length ||
         !StrongEntityTag(transfer->EntityTag) ||
         (!transfer->IfMatch.empty() && transfer->EntityTag != transfer->IfMatch))) {
      transfer->Failure = Data::FetchFailureReason::CorruptPayload;
    }
    transfer->Status = static_cast<int>(status);
    transfer->RetryAfterS = static_cast<double>(retryAfter);
    ++Completions_;
    Landed_.notify_all();
  }
}

void Fetching::FinishTransfers(void *multiHandle, bool failed) {
  auto *const multi = static_cast<CURLM *>(multiHandle);
  const std::scoped_lock lock(Mutex_);
  for (auto &[ticket, transfer] : Transfers_) {
    (void)ticket;
    if (transfer.Handle == nullptr) { continue; }
    auto *const easy = static_cast<CURL *>(transfer.Handle);
    (void)curl_multi_remove_handle(multi, easy);
    curl_easy_cleanup(easy);
    transfer.Handle = nullptr;
  }
  if (failed) {
    State_ = State::Failed;
    for (auto &[ticket, transfer] : Transfers_) {
      (void)ticket;
      if (transfer.Done) { continue; }
      transfer.Done = true;
      if (!transfer.Failure) { transfer.Failure = Data::FetchFailureReason::Unavailable; }
      ++Completions_;
    }
    Landed_.notify_all();
  }
}

void Fetching::Work() {
  auto *const multi = static_cast<CURLM *>(Multi_);
  const auto capacity = static_cast<size_t>(Config_.ConcurrentTransfers);
  size_t active = 0;
  bool failed = false;
  for (;;) {
    active = AddQueuedTransfers(multi, active);
    {
      const std::scoped_lock lock(Mutex_);
      if (State_ != State::Ready) { break; }
    }
    int running = 0;
    if (curl_multi_perform(multi, &running) != CURLM_OK) {
      failed = true;
      break;
    }
    CollectCompletions(multi, active);
    {
      const std::scoped_lock lock(Mutex_);
      if (!Queue_.empty() && active < capacity) { continue; }
    }
    int descriptors = 0;
    (void)curl_multi_poll(multi, nullptr, 0, kPollMostMs, &descriptors);
  }
  FinishTransfers(multi, failed);
}

}
