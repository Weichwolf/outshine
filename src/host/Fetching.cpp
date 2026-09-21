#include "Fetching.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace outshine {

namespace {

constexpr double kMicrosecondsPerMillisecond = 1000.0;
constexpr long kPollMostMs = 1000;

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

Fetching::Fetching(Config config) : Config_(std::move(config)) {
  Config_.ConcurrentTransfers = std::max(Config_.ConcurrentTransfers, 1);
  Config_.ConnectionsPerHost = std::max(Config_.ConnectionsPerHost, 1);
  Config_.TimeoutS = std::max(Config_.TimeoutS, 1L);
  Config_.MaxRequests = std::max(Config_.MaxRequests, size_t{1});
  Config_.MaxBodyBytes = std::max(Config_.MaxBodyBytes, size_t{1});
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
  Worker_ = std::thread([this] { Work(); });
}

Fetching::~Fetching() {
  {
    const std::scoped_lock lock(Mutex_);
    Stopping_ = true;
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

Data::Ticket Fetching::Begin(const std::string &url) {
  if (url.empty() || Multi_ == nullptr) { return Data::Ticket::None; }
  uint64_t ticket = 0;
  {
    const std::scoped_lock lock(Mutex_);
    if (Stopping_ || Transfers_.size() >= Config_.MaxRequests ||
        NextTicket_ == std::numeric_limits<uint64_t>::max()) {
      return Data::Ticket::None;
    }
    ticket = NextTicket_++;
    Transfers_.try_emplace(ticket);
    Transfers_.at(ticket).Ticket = ticket;
    Transfers_.at(ticket).Url = url;
    Transfers_.at(ticket).MaxBodyBytes = Config_.MaxBodyBytes;
    Queue_.push_back(ticket);
  }
  WakeWorker();
  return static_cast<Data::Ticket>(ticket);
}

Data::Wire Fetching::Collect(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) { return Data::Wire::Unreachable(); }
  const std::scoped_lock lock(Mutex_);
  const auto found = Transfers_.find(static_cast<uint64_t>(ticket));
  if (found == Transfers_.end()) { return Data::Wire::Unreachable(); }
  Transfer &done = found->second;
  if (!done.Done) { return Data::Wire::Working(); }
  const bool unreachable = done.Unreachable;
  const int status = done.Status;
  const double retryAfterS = done.RetryAfterS;
  std::vector<uint8_t> body = std::move(done.Body);
  Transfers_.erase(found);
  if (unreachable) { return Data::Wire::Unreachable(); }
  return Data::Wire::Answered(status, std::move(body), retryAfterS);
}

void Fetching::Cancel(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) { return; }
  {
    const std::scoped_lock lock(Mutex_);
    const auto found = Transfers_.find(static_cast<uint64_t>(ticket));
    if (found == Transfers_.end()) { return; }
    if (found->second.Done) {
      Transfers_.erase(found);
      return;
    }
    found->second.Cancelled.store(true, std::memory_order_relaxed);
    const auto queued = std::ranges::find(Queue_, static_cast<uint64_t>(ticket));
    if (queued != Queue_.end()) {
      Queue_.erase(queued);
      Transfers_.erase(found);
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
             [this, stood] { return Stopping_ || Completions_ != stood; }) &&
         Completions_ != stood;
}

void Fetching::WakeWorker() noexcept {
  if (Multi_ != nullptr) { (void)curl_multi_wakeup(static_cast<CURLM *>(Multi_)); }
}

bool Fetching::ConfigureTransfer(void *handle, Transfer &transfer) const {
  auto *const easy = static_cast<CURL *>(handle);
  const auto progress = +[](void *data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
    const auto &active = *static_cast<Transfer *>(data);
    return static_cast<int>(active.Cancelled.load(std::memory_order_relaxed));
  };
  const auto write = +[](const void *data, size_t, size_t byteCount, void *user) -> size_t {
    auto &active = *static_cast<Transfer *>(user);
    if (byteCount > active.MaxBodyBytes || active.Body.size() > active.MaxBodyBytes - byteCount) {
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
         curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_TIMEOUT, Config_.TimeoutS) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "") == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_USERAGENT, Config_.UserAgent.c_str()) == CURLE_OK &&
         curl_easy_setopt(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS) == CURLE_OK;
}

size_t Fetching::AddQueuedTransfers(void *multiHandle, size_t active) {
  auto *const multi = static_cast<CURLM *>(multiHandle);
  const auto capacity = static_cast<size_t>(Config_.ConcurrentTransfers);
  const std::scoped_lock lock(Mutex_);
  while (!Stopping_ && active < capacity && !Queue_.empty()) {
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
    transfer.Unreachable = true;
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
      continue;
    }
    transfer->Done = true;
    transfer->Unreachable = message->data.result != CURLE_OK;
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
    for (auto &[ticket, transfer] : Transfers_) {
      (void)ticket;
      if (transfer.Done) { continue; }
      transfer.Done = true;
      transfer.Unreachable = true;
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
      if (Stopping_) { break; }
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
