/* The Log sink implementation over a destination core/io owns (TextTarget.h). Line format:
 * "t=<simTime> LEVEL tag event k=v k=v ...", generated once here. */
#ifndef LOGSINKS_H
#define LOGSINKS_H
#include <cstdio>
#include <vector>
#include "Log.h"
#include "TextTarget.h"

namespace outshine::Clients {

/* Flushed EVERY line, so a run killed mid-mission does not lose the tail. */
class TextLogSink : public LogSink {
public:
  explicit TextLogSink(const TextTarget &target) : File_(target.File()) {}
  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
            const std::vector<LogField> &fields) override;

private:
  std::FILE *File_;   /* borrowed from the target, which outlives this sink */
};

/* Log::SetSink takes a BORROWED pointer, so EVERY path out of the owning scope — including a
 * mission-load failure's early return — must unset it, or the next log call writes through a dangling
 * pointer into an fclose'd FILE*. Declare it AFTER the sinks it installs, so it is destroyed first. */
class LogSinkScope {
public:
  explicit LogSinkScope(LogSink *sink) { Log::SetSink(sink); }
  ~LogSinkScope() { Log::SetSink(nullptr); }
  LogSinkScope(const LogSinkScope &) = delete;
  LogSinkScope &operator=(const LogSinkScope &) = delete;
};

} // namespace outshine::Clients
#endif /* LOGSINKS_H */
