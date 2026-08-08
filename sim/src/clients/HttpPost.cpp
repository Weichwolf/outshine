#include "HttpPost.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <curl/curl.h>
#endif

namespace outshine::Clients {

#ifdef __EMSCRIPTEN__

/* AWAITED, AND THE STATUS IS THE ANSWER. Fire-and-forget returned true for a POST the collector had
 * refused, so a 64 MB class dump that hit the host's body limit was reported as a delivered product
 * — a run that says rc=0 and leaves nothing behind. The wait costs one ASYNCIFY unwind on a call
 * that happens a handful of times per run, never inside a frame that is being measured.
 *
 * A COPY IS TAKEN INSIDE THE CALL, because fetch keeps the buffer alive past this turn while the
 * caller's std::string is about to be cleared. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
EM_ASYNC_JS(int, fb_post, (const char *url, const uint8_t *body, int bytes, const char *ct), {
  try {
    var r = await fetch(UTF8ToString(url),
                        {method: 'POST', headers: {'Content-Type': UTF8ToString(ct)},
                         body: HEAPU8.slice(body, body + bytes)});
    return r.status | 0;
  } catch (e) { return 0; }
})
#pragma clang diagnostic pop

bool HttpPost(const std::string &url, const void *body, size_t bytes, const char *contentType) {
  const int status = fb_post(url.c_str(), (const uint8_t *)body, (int)bytes, contentType);
  return status >= 200 && status < 300;
}

#else

bool HttpPost(const std::string &url, const void *body, size_t bytes, const char *contentType) {
  CURL *c = curl_easy_init();
  if (!c) return false;
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
  /* A 404 is a CURLE_OK that stored nothing — the collector's answer decides, not the socket's. */
  return rc == CURLE_OK && status >= 200 && status < 300;
}

#endif

}  // namespace outshine::Clients
