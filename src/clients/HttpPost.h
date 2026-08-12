#ifndef HTTPPOST_H
#define HTTPPOST_H

#include <cstddef>
#include <memory>
#include <string>

namespace outshine::Clients {

/* THE ONE PLACE THIS TREE TALKS TO fb-sim. Two translations, two transports — libcurl natively,
 * fetch in the browser — and the seam is here rather than in every caller, because a caller that
 * knows which one it is would have to be written twice.
 *
 * STARTED, THEN ASKED. The status is the answer and not the departure: fire-and-forget once reported
 * a 64 MB class dump the host had refused as a delivered product — a run that says rc=0 and leaves
 * nothing behind. Waiting for it is not the fix either, because the thread that would wait is the
 * one the fetch completes on. So a caller that needs its product delivered asks until the answer
 * stops being InFlight, and does something else in between.
 *
 * A COPY IS TAKEN INSIDE Begin(): the body is fully consumed before it returns, so the caller may
 * clear its buffer on the next line. */
class HttpPost {
public:
  enum class State { Idle, InFlight, Delivered, Refused };

  HttpPost() = default;
  ~HttpPost();
  HttpPost(const HttpPost &) = delete;
  HttpPost &operator=(const HttpPost &) = delete;

  void Begin(const std::string &url, const void *body, size_t bytes, const char *contentType);
  [[nodiscard]] State Ask() const;
  /* The verdict, once: a final answer is reported and then forgotten, so a counter cannot count the
   * same delivery twice. */
  [[nodiscard]] State Take();

private:
  /* The transport writes the HTTP status here and the browser's does it from a promise, so the cell
   * has to outlive this object when a post is abandoned mid-flight. */
  std::unique_ptr<int> Status_;
};

} // namespace outshine::Clients
#endif
