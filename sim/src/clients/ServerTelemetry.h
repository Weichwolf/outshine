#ifndef SERVERTELEMETRY_H
#define SERVERTELEMETRY_H

#include <string>

#include "HttpPost.h"
#include "Telemetry.h"

namespace outshine::Clients {

/* THE STATE CHANNEL, ON THE SERVER — one CSV per run beside its log. Same batching as ServerLog and
 * for the same reason: a request per row would put the network inside the frame the row measures.
 * A row is emitted once a second, so the buffer is tiny and the flush is rare. */
class ServerTelemetry : public TelemetrySink {
public:
  ServerTelemetry(const std::string &base, const std::string &runId)
      : Url_(base + "/telemetry/" + runId) {}
  ~ServerTelemetry() override;
  ServerTelemetry(const ServerTelemetry &) = delete;
  ServerTelemetry &operator=(const ServerTelemetry &) = delete;

  void Header(const std::vector<std::string> &columns) override;
  void Row(const std::vector<std::string> &fields) override;
  /* Pushes what it can and says whether anything is still owed (ServerLog.h). */
  bool Flush();

private:
  static constexpr size_t kFlushBytes = 16u * 1024u;

  std::string Url_, Pending_;
  HttpPost Post_;
  size_t Dropped_ = 0;
};

} // namespace outshine::Clients
#endif
