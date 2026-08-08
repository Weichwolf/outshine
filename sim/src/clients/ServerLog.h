#ifndef SERVERLOG_H
#define SERVERLOG_H

#include <string>

#include "Log.h"

namespace outshine::Clients {

/* THE LOG, ON THE SERVER. Until now every line ended in the stderr of a process nobody watches, and
 * in the browser it ended in a console nobody has open. A run is reconstructible from fb-sim's copy
 * alone: which mod, which scene, which build, which numbers.
 *
 * BATCHED, NOT PER LINE. One request per line would put the network inside the frame, which is
 * exactly the measurement this log carries. Lines accumulate and go out in one body when the buffer
 * passes `kFlushBytes` or when the run ends; a run's whole log is a few hundred kilobytes, so in
 * practice that is ONE request, after the last frame, and no frame pays for it.
 *
 * A DEAD COLLECTOR IS NOT A DEAD RUN. A refused POST drops its batch, counts the loss and returns;
 * the count is posted with the next batch that gets through, so a gap is visible rather than
 * silent. */
class ServerLog : public LogSink {
public:
  /* WHAT MAKES A RUN IDENTIFIABLE. `Build` is the wasm hash in the browser and the binary's hash
   * natively — the environment supplies it, because neither translation can hash the file it is
   * currently executing without reading it back off the network it was loaded from. */
  struct Identity {
    std::string Mod, Scene, Client, Build, Host;
  };

  ServerLog(const std::string &base, const Identity &id);
  ~ServerLog() override;
  ServerLog(const ServerLog &) = delete;
  ServerLog &operator=(const ServerLog &) = delete;

  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
             const std::vector<LogField> &fields) override;
  void Flush();

  const std::string &RunId() const { return RunId_; }

private:
  static constexpr size_t kFlushBytes = 1u << 20;

  std::string Url_, RunId_, Pending_;
  size_t Dropped_ = 0, Sent_ = 0;
};

} // namespace outshine::Clients
#endif
