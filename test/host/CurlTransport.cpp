#include "CurlTransport.h"

#include <curl/curl.h>

#include <algorithm>
#include <utility>

namespace outshine::Host {
namespace {

/* [SET] Threads when the host is not asked. Eight, because the tile pool's ceiling is six workers
 * plus the frame thread's own oracle ask, and a transport narrower than its caller turns parallel
 * asks into a queue. */
constexpr int kDefaultThreads = 8;

struct Sink {
  std::vector<uint8_t> *Out;
  size_t Max;
};

size_t Write(void *data, size_t size, size_t members, void *user) {
  Sink *sink = (Sink *)user;
  const size_t add = size * members;
  /* Returning short aborts the transfer, which curl reports as an error — the partial bytes are
   * never handed back as an answer. */
  if (sink->Max && sink->Out->size() + add > sink->Max) return 0;
  const uint8_t *bytes = (const uint8_t *)data;
  sink->Out->insert(sink->Out->end(), bytes, bytes + add);
  return add;
}

} // namespace

CurlTransport::CurlTransport(const Config &config) : Config_(config) {
  /* curl_easy_init's lazy global init is not thread-safe, and every thread below has its own easy
   * handle. */
  curl_global_init(CURL_GLOBAL_DEFAULT);
  int n = Config_.Threads;
  if (n <= 0) {
    const unsigned hardware = std::thread::hardware_concurrency();
    n = hardware > 0u ? std::min((int)hardware, kDefaultThreads) : kDefaultThreads;
  }
  Threads_.reserve((size_t)n);
  for (int i = 0; i < n; i++) Threads_.emplace_back([this] { Work(); });
}

CurlTransport::~CurlTransport() {
  {
    std::lock_guard<std::mutex> lock(Mutex_);
    Stopping_ = true;
  }
  Wake_.notify_all();
  for (std::thread &t : Threads_) t.join();
  curl_global_cleanup();
}

Data::Ticket CurlTransport::Begin(const std::string &url) {
  if (url.empty()) return Data::Ticket::None;
  std::lock_guard<std::mutex> lock(Mutex_);
  const uint64_t ticket = NextTicket_++;
  Transfers_[ticket].Url = url;
  Queue_.push_back(ticket);
  Wake_.notify_one();
  return (Data::Ticket)ticket;
}

Data::Wire CurlTransport::Collect(Data::Ticket ticket) {
  if (ticket == Data::Ticket::None) return Data::Wire::Unreachable();
  std::lock_guard<std::mutex> lock(Mutex_);
  const auto found = Transfers_.find((uint64_t)ticket);
  /* A ticket collected twice, or one that was cancelled: there is no answer to have. */
  if (found == Transfers_.end()) return Data::Wire::Unreachable();
  if (!found->second.Done) return Data::Wire::Working();
  Transfer done = std::move(found->second);
  Transfers_.erase(found);
  if (done.Unreachable) return Data::Wire::Unreachable();
  return Data::Wire::Answered(done.Status, std::move(done.Body));
}

void CurlTransport::Cancel(Data::Ticket ticket) {
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
    /* Never started: it leaves without touching the wire at all. */
    Queue_.erase(queued);
    Transfers_.erase(found);
    return;
  }
  /* Already under way. It cannot be taken off the wire from here, so it is marked and the worker
   * drops its product when it lands — which is what stops a cancelled transfer from filling the
   * table for the life of the run. */
  found->second.Cancelled = true;
}

void CurlTransport::Work() {
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
    }

    std::lock_guard<std::mutex> lock(Mutex_);
    const auto found = Transfers_.find(ticket);
    if (found == Transfers_.end()) continue;
    if (found->second.Cancelled) {
      Transfers_.erase(found);
      continue;
    }
    found->second.Done = true;
    if (result != CURLE_OK) {
      found->second.Unreachable = true;
    } else {
      found->second.Status = (int)status;
      found->second.Body = std::move(body);
    }
  }
  if (handle) curl_easy_cleanup(handle);
}

} // namespace outshine::Host
