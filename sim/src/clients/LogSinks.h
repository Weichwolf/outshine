/* The Log sink implementations: app/ is the one place raw stdio is allowed, everything else calls
 * the facade. Line format: "t=<simTime> LEVEL tag event k=v k=v ...", generated once here. */
#ifndef LOGSINKS_H
#define LOGSINKS_H
#include <cstdio>
#include <memory>
#include <vector>
#include "Log.h"

namespace outshine::Clients {

/* Browser console / interactive stdout. */
class StdoutLogSink : public LogSink {
public:
  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
            const std::vector<LogField> &fields) override;
};

/* Flushed EVERY line, so a crash mid-mission does not lose the tail. */
class FileLogSink : public LogSink {
public:
  explicit FileLogSink(FILE *f) : F(f) {}
  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
            const std::vector<LogField> &fields) override;

private:
  FILE *F;
};

/* Fan-out to N borrowed children: the runner wants console AND events.log from the same call sites. */
class CompositeLogSink : public LogSink {
public:
  void Add(LogSink *sink) { Children.push_back(sink); }
  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
            const std::vector<LogField> &fields) override;

private:
  std::vector<LogSink *> Children;
};

/* Captures lines instead of writing them: one buffer per UNIT in the parallel step phase, drained into
 * the real sink at the barrier in unit order. THAT is what keeps events.log byte-identical between
 * `--threads 1` and `--threads N` — no line's position depends on who got there first. Drain() keeps
 * its capacity, so a steady-state tick allocates nothing. `tag`/`event` are COPIED: a buffered line
 * outlives the Emit() call, and only a string literal would survive that by accident. */
class BufferedLogSink : public LogSink {
public:
  void Write(double simTimeS, LogLevel level, const char *tag, const char *event,
            const std::vector<LogField> &fields) override;
  void Drain(LogSink &out);

private:
  struct BufferedLine {
    double TimeS;
    LogLevel Level;
    std::string Tag, Event;
    std::vector<LogField> Fields;
  };
  std::vector<BufferedLine> Lines;
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

/* Owning FILE* handle, for the same reason. */
using FileHandle = std::unique_ptr<FILE, int (*)(FILE *)>;
inline FileHandle OpenFile(const char *path, const char *mode) {
  return FileHandle(fopen(path, mode), &fclose);
}

} // namespace outshine::Clients
#endif /* LOGSINKS_H */
