#include "HttpPost.h"

#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <curl/curl.h>
#endif

namespace outshine::Clients {
namespace {

/* The status cell of a post nobody waited for. The promise still resolves and still writes, so the
 * four bytes are kept rather than freed — a run abandons a handful of them at most, and the
 * alternative is a write into memory something else now owns. Two facts elsewhere make the static
 * safe and both would take it away if they changed: `-sEXIT_RUNTIME=0` (sim/Makefile) means this
 * destructor never runs, so the cells outlive every promise; and native curl is synchronous, so a
 * post is never in flight at destruction and nothing is ever added here off the browser. */
std::vector<std::unique_ptr<int>> gAbandoned;

/* The cell's own alphabet: zero while the transport has not answered, an HTTP status when it has,
 * negative when there was no answer to have. The browser's `catch` writes the same -1 by hand,
 * because JS cannot see a C++ constant. */
constexpr int kInFlight = 0;
#ifndef __EMSCRIPTEN__
constexpr int kNoAnswer = -1;
#endif

}  // namespace

#ifdef __EMSCRIPTEN__
/* EM_JS spells its own symbols with a dollar and -Wpedantic sees a C identifier; the suppression is
 * around the one block that has to say it. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
EM_JS(void, fb_post, (const char *url, const uint8_t *body, int bytes, const char *ct, int *cell), {
  var slot = cell >> 2;
  fetch(UTF8ToString(url),
        {method: 'POST', headers: {'Content-Type': UTF8ToString(ct)},
         body: HEAPU8.slice(body, body + bytes)})
      .then(function (r) { HEAP32[slot] = r.status | 0; })
      .catch(function () { HEAP32[slot] = -1; });
})
#pragma clang diagnostic pop
#endif

HttpPost::~HttpPost() {
  if (Status_ && *Status_ == kInFlight) gAbandoned.push_back(std::move(Status_));
}

#ifdef __EMSCRIPTEN__

void HttpPost::Begin(const std::string &url, const void *body, size_t bytes,
                     const char *contentType) {
  Status_ = std::make_unique<int>(kInFlight);
  fb_post(url.c_str(), (const uint8_t *)body, (int)bytes, contentType, Status_.get());
}

#else

void HttpPost::Begin(const std::string &url, const void *body, size_t bytes,
                     const char *contentType) {
  Status_ = std::make_unique<int>(kNoAnswer);
  CURL *c = curl_easy_init();
  if (!c) return;
  struct curl_slist *hdr = nullptr;
  const std::string ct = std::string("Content-Type: ") + contentType;
  hdr = curl_slist_append(hdr, ct.c_str());
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_POST, 1L);
  curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bytes);
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
  curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 4000L);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 1000L);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, +[](char *, size_t sz, size_t n, void *) {
    return sz * n;
  });
  const CURLcode rc = curl_easy_perform(c);
  long status = 0;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(hdr);
  curl_easy_cleanup(c);
  *Status_ = (rc == CURLE_OK) ? (int)status : kNoAnswer;
}

#endif

HttpPost::State HttpPost::Ask() const {
  if (!Status_) return State::Idle;
  const int s = *Status_;
  if (s == kInFlight) return State::InFlight;
  return (s >= 200 && s < 300) ? State::Delivered : State::Refused;
}

HttpPost::State HttpPost::Take() {
  const State st = Ask();
  if (st == State::Delivered || st == State::Refused) Status_.reset();
  return st;
}

} // namespace outshine::Clients
