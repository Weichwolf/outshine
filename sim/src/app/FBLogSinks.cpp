#include "FBLogSinks.h"

namespace FlightBox {

namespace {
const char *LevelStr(FBLogLevel l) {
  switch (l) {
    case FBLogLevel::Debug: return "DEBUG";
    case FBLogLevel::Info:  return "INFO";
    case FBLogLevel::Warn:  return "WARN";
    case FBLogLevel::Error: return "ERROR";
  }
  return "?";
}

void WriteLine(FILE *f, double t, FBLogLevel level, const char *tag, const char *event,
              const std::vector<FBLogField> &fields) {
  fprintf(f, "t=%.1f %s %s %s", t, LevelStr(level), tag, event);
  for (const auto &fld : fields) {
    bool quote = fld.Value.find(' ') != std::string::npos;
    if (quote) fprintf(f, " %s=\"%s\"", fld.Key, fld.Value.c_str());
    else fprintf(f, " %s=%s", fld.Key, fld.Value.c_str());
  }
  fprintf(f, "\n");
  fflush(f);
}
} // namespace

void FBStdoutLogSink::Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
                            const std::vector<FBLogField> &fields) {
  WriteLine(stdout, simTimeS, level, tag, event, fields);
}

void FBFileLogSink::Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
                          const std::vector<FBLogField> &fields) {
  if (F) WriteLine(F, simTimeS, level, tag, event, fields);
}

void FBCompositeLogSink::Write(double simTimeS, FBLogLevel level, const char *tag, const char *event,
                               const std::vector<FBLogField> &fields) {
  for (auto *c : Children) c->Write(simTimeS, level, tag, event, fields);
}

} // namespace FlightBox
