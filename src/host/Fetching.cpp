#include "Fetching.h"

#include <curl/curl.h>

#include <algorithm>
#include <utility>

namespace outshine {
namespace {

constexpr int kDefaultThreads = 8;

struct Sink {
  std::vector<uint8_t> *Out;
  size_t Max;
};

size_t Write(void *data, size_t size, size_t members, void *user) {
  Sink *sink = (Sink *)user;
  const size_t add = size * members;

  if (sink->Max && sink->Out->size() + add > sink->Max) return 0;
  const uint8_t *bytes = (const uint8_t *)data;
  sink->Out->insert(sink->Out->end(), bytes, bytes + add);
  return add;
}

}

Fetching::Fetching(const Config &config) : Config_(config) {

  curl_global_init(CURL_GLOBAL_DEFAULT);
  int n = Config_.Threads;
  if (n <= 0) {
    const unsigned hardware = std::thread::hardware_concurrency();
    n = hardware > 0u ? std::min((int)hardware, kDefaultThreads) : kDefaultThreads;
  }
  Threads_.reserve((size_t)n);
  for (int i = 0; i < n; i++) Threads_.emplace_back([this] { Work(); });
}

Fetching::~Fetching() {
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    Stopping_ = true;
  }
  Wake_.notify_all();
  for (std::thread &t : Threads_) t.join();
  curl_global_cleanup();
}

Data::Ticket Fetching::Begin(const std::string &url) {
  if (url.empty()) return Data::Ticket::None;
  std::lock_guard<std::mutex> lock(Mutex_);
  const uint64_t ticket = NextTicket_++;
  Transfers_[ticket].Url = url;
  Queue_.push_back(ticket);
  Wake_.notify_one();
  return (Data::Ticket)ticket;
}

Data::Wire Fetching::Collect(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) return Data::Wire::Unreachable();
  std::lock_guard<std::mutex> lock(Mutex_);
  const auto found = Transfers_.find((uint64_t)ticket);

  if (found == Transfers_.end()) return Data::Wire::Unreachable();
  if (!found->second.Done) return Data::Wire::Working();
  Transfer done = std::move(found->second);
  Transfers_.erase(found);
  if (done.Unreachable) return Data::Wire::Unreachable();
  return Data::Wire::Answered(done.Status, std::move(done.Body), done.RetryAfterS);
}

void Fetching::Cancel(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) return;
  std::lock_guard<std::mutex> lock(Mutex_);
  const auto found = Transfers_.find((uint64_t)ticket);
  if (found == Transfers_.end()) return;
  if (found->second.Done) {
    Transfers_.erase(found);
    return;
  }
  const auto queued = std::find(Queue_.begin(), Queue_.end(), (uint64_t)ticket);
  if (queued != Queue_.end()) {

    Queue_.erase(queued);
    Transfers_.erase(found);
    return;
  }

  found->second.Cancelled = true;
}

bool Fetching::Await(double forMs) {
  std::unique_lock<std::mutex> lock(Mutex_);
  const uint64_t stood = Completions_;
  const bool outstanding = !Transfers_.empty();
  if (!outstanding) { return false; }
  return Landed_.wait_for(lock, std::chrono::microseconds((long long)(forMs * 1000.0)),
                          [this, stood] { return Stopping_ || Completions_ != stood; }) &&
         Completions_ != stood;
}

void Fetching::Work() {
  CURL *handle = curl_easy_init();
  for (;;) {
    uint64_t ticket = 0;
    std::string url;
    {
      std::unique_lock<std::mutex> lock(Mutex_);
      Wake_.wait(lock, [this] { return Stopping_ || !Queue_.empty(); });
      if (Stopping_) break;
      ticket = Queue_.front();
      Queue_.erase(Queue_.begin());
      const auto found = Transfers_.find(ticket);
      if (found == Transfers_.end()) continue;
      url = found->second.Url;
    }

    std::vector<uint8_t> body;
    Sink sink{&body, Config_.MaxBodyBytes};
    long status = 0;
    curl_off_t retryAfter = 0;
    CURLcode result = CURLE_FAILED_INIT;
    if (handle) {
      curl_easy_reset(handle);
      curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
      curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, Write);
      curl_easy_setopt(handle, CURLOPT_WRITEDATA, &sink);
      curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(handle, CURLOPT_TIMEOUT, Config_.TimeoutS);
      curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
      curl_easy_setopt(handle, CURLOPT_USERAGENT, Config_.UserAgent.c_str());
      result = curl_easy_perform(handle);
      if (result == CURLE_OK) curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
      if (result == CURLE_OK) curl_easy_getinfo(handle, CURLINFO_RETRY_AFTER, &retryAfter);
    }

    std::lock_guard<std::mutex> lock(Mutex_);
    const auto found = Transfers_.find(ticket);
    if (found == Transfers_.end()) continue;
    if (found->second.Cancelled) {
      Transfers_.erase(found);
      continue;
    }
    found->second.Done = true;
    ++Completions_;
    Landed_.notify_all();
    if (result != CURLE_OK) {
      found->second.Unreachable = true;
    } else {
      found->second.Status = (int)status;
      found->second.RetryAfterS = (double)retryAfter;
      found->second.Body = std::move(body);
    }
  }
  if (handle) curl_easy_cleanup(handle);
}

}
