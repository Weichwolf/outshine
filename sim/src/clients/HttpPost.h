#ifndef HTTPPOST_H
#define HTTPPOST_H

#include <cstddef>
#include <string>

namespace outshine::Clients {

/* THE ONE PLACE THIS TREE TALKS TO fb-sim. Two translations, two transports — libcurl natively,
 * fetch in the browser — and the seam is here rather than in every caller, because a caller that
 * knows which one it is would have to be written twice.
 *
 * FIRE AND FORGET, by contract: a run may not die because the collector is down, and it may not
 * wait for it either. The browser call returns before the request leaves; the native one blocks on
 * the socket, which is why its callers batch (see ServerLog.h). */
bool HttpPost(const std::string &url, const void *body, size_t bytes, const char *contentType);

} // namespace outshine::Clients
#endif
