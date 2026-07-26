/* FlightBox — FBLog sink implementations. app/ is the one place raw stdio is allowed (CLAUDE.md's
 * Engineering-Konventionen exception: "Sink-Implementierungen selbst"); core/systems/render/world/fdm
 * only ever call the FBLog facade. Line format: "t=<simTime> LEVEL tag event k=v k=v ..." — the shape
 * the legacy events.log already used, now generated once here instead of hand-formatted at every call
 * site. */
#ifndef FBLOGSINKS_H
#define FBLOGSINKS_H
#include <cstdio>
#include <memory>
#include <vector>
#include "FBLog.h"

namespace FlightBox {

/* Browser console (WASM) / interactive stdout (native). */
class FBStdoutLogSink : public FBLogSink {
public:
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;
};

/* events.log — one fopen'd FILE* for the runner's lifetime, flushed every line (matches the old
 * logEvent's per-call fflush so a crash mid-mission doesn't lose the tail). */
class FBFileLogSink : public FBLogSink {
public:
  explicit FBFileLogSink(FILE *f) : F(f) {}
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;

private:
  FILE *F;
};

/* Fan-out to N borrowed child sinks — the mission runner wants both console visibility and events.log
 * from the same FBLog call sites. */
class FBCompositeLogSink : public FBLogSink {
public:
  void Add(FBLogSink *sink) { Children.push_back(sink); }
  void Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
            const std::vector<FBLogField> &fields) override;

private:
  std::vector<FBLogSink *> Children;
};

/* Captures lines instead of writing them — one buffer per UNIT in the mission runner's parallel Run()
 * phase, which points each worker thread at the buffer of the unit it is stepping (FBLog::SetThreadSink)
 * and Drain()s all of them into the real sink at the tick barrier, in unit order. That indirection is
 * what keeps events.log byte-identical between `--threads 1` and `--threads N`: no line's position
 * depends on which thread got there first. Reused across ticks — Drain() empties the vector but keeps
 * its capacity, so a steady-state tick that logs nothing allocates nothing.
 * `tag`/`event` are copied rather than kept as the caller's const char*: a buffered line outlives the
 * Emit() call, and only string literals would survive that by accident. */
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

/* RAII scope for FBLog's process-wide sink pointer. FBLog::SetSink takes a BORROWED pointer, so every
 * path out of the scope that owns the sink objects — including the early returns a mission-load failure
 * takes — must unset it, or the next log call writes through a dangling pointer into an already-fclose'd
 * FILE*. Manual cleanup on the success path only is the classic version of that bug; this makes it
 * structural. Declare it AFTER the sinks it installs so it is destroyed FIRST (reverse declaration
 * order), i.e. the sink pointer is gone before the objects are. */
class FBLogSinkScope {
public:
  explicit FBLogSinkScope(FBLogSink *sink) { FBLog::SetSink(sink); }
  ~FBLogSinkScope() { FBLog::SetSink(nullptr); }
  FBLogSinkScope(const FBLogSinkScope &) = delete;
  FBLogSinkScope &operator=(const FBLogSinkScope &) = delete;
};

/* Owning FILE* handle for the same reason (every early return below used to need its own fclose). */
using FBFileHandle = std::unique_ptr<FILE, int (*)(FILE *)>;
inline FBFileHandle FBOpenFile(const char *path, const char *mode) {
  return FBFileHandle(fopen(path, mode), &fclose);
}

} // namespace FlightBox
#endif /* FBLOGSINKS_H */
