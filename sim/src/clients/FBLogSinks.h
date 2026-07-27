/* The FBLog sink implementations: app/ is the one place raw stdio is allowed, everything else calls
 * the facade. Line format: "t=<simTime> LEVEL tag event k=v k=v ...", generated once here. */
#ifndef FBLOGSINKS_H
#define FBLOGSINKS_H
#include <cstdio>
#include <memory>
#include <vector>
#include "FBLog.h"

namespace FlightBox {

/* Browser console / interactive stdout. */
class FBStdoutLogSink : public FBLogSink {
public:
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;
};

/* Flushed EVERY line, so a crash mid-mission does not lose the tail. */
class FBFileLogSink : public FBLogSink {
public:
  explicit FBFileLogSink(FILE *f) : F(f) {}
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;

private:
  FILE *F;
};

/* Fan-out to N borrowed children: the runner wants console AND events.log from the same call sites. */
class FBCompositeLogSink : public FBLogSink {
public:
  void Add(FBLogSink *sink) { Children.push_back(sink); }
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;

private:
  std::vector<FBLogSink *> Children;
};

/* Captures lines instead of writing them: one buffer per UNIT in the parallel step phase, drained into
 * the real sink at the barrier in unit order. THAT is what keeps events.log byte-identical between
 * `--threads 1` and `--threads N` — no line's position depends on who got there first. Drain() keeps
 * its capacity, so a steady-state tick allocates nothing. `tag`/`event` are COPIED: a buffered line
 * outlives the Emit() call, and only a string literal would survive that by accident. */
class FBBufferedLogSink : public FBLogSink {
public:
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;
  void Drain(FBLogSink &out);

private:
  struct FBBufferedLine {
    double TimeS;
    FBLogLevel Level;
    std::string Tag, Event;
    std::vector<FBLogField> Fields;
  };
  std::vector<FBBufferedLine> Lines;
};

/* FBLog::SetSink takes a BORROWED pointer, so EVERY path out of the owning scope — including a
 * mission-load failure's early return — must unset it, or the next log call writes through a dangling
 * pointer into an fclose'd FILE*. Declare it AFTER the sinks it installs, so it is destroyed first. */
class FBLogSinkScope {
public:
  explicit FBLogSinkScope(FBLogSink *sink) { FBLog::SetSink(sink); }
  ~FBLogSinkScope() { FBLog::SetSink(nullptr); }
  FBLogSinkScope(const FBLogSinkScope &) = delete;
  FBLogSinkScope &operator=(const FBLogSinkScope &) = delete;
};

/* Owning FILE* handle, for the same reason. */
using FBFileHandle = std::unique_ptr<FILE, int (*)(FILE *)>;
inline FBFileHandle FBOpenFile(const char *path, const char *mode) {
  return FBFileHandle(fopen(path, mode), &fclose);
}

} // namespace FlightBox
#endif /* FBLOGSINKS_H */
