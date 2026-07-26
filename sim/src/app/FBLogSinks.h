/* FlightBox — FBLog sink implementations. app/ is the one place raw stdio is allowed (CLAUDE.md's
 * Engineering-Konventionen exception: "Sink-Implementierungen selbst"); core/systems/render/world/fdm
 * only ever call the FBLog facade. Line format: "t=<simTime> LEVEL tag event k=v k=v ..." — the shape
 * the legacy events.log already used, now generated once here instead of hand-formatted at every call
 * site. */
#ifndef FBLOGSINKS_H
#define FBLOGSINKS_H
#include <cstdio>
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

} // namespace FlightBox
#endif /* FBLOGSINKS_H */
