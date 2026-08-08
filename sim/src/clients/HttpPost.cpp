#include "HttpPost.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <curl/curl.h>
#endif

namespace outshine::Clients {

#ifdef __EMSCRIPTEN__

bool HttpPost(const std::string &url, const void *body, size_t bytes, const char *contentType) {
  /* A COPY IS TAKEN INSIDE THE CALL, because fetch keeps the buffer alive past this frame while the
   * caller's std::string is about to be cleared. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
  EM_ASM({
    var postBody = HEAPU8.slice($1, $1 + $2);
    fetch(UTF8ToString($0), {method: 'POST', headers: {'Content-Type': UTF8ToString($3)},
                             body: postBody}).catch(function () {});
  }, url.c_str(), body, (int)bytes, contentType);
#pragma clang diagnostic pop
  return true;
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
