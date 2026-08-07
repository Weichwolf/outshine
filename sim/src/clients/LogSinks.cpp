#include "LogSinks.h"

namespace outshine::Clients {

namespace {
const char *LevelStr(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "?";
}

void WriteLine(FILE *f, double t, LogLevel level, const char *tag, const char *event,
              const std::vector<LogField> &fields) {
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

void StdoutLogSink::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                            const std::vector<LogField> &fields) {
  WriteLine(stdout, simTimeS, level, tag, event, fields);
}

void FileLogSink::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                          const std::vector<LogField> &fields) {
  if (F) WriteLine(F, simTimeS, level, tag, event, fields);
}

void CompositeLogSink::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                               const std::vector<LogField> &fields) {
  for (auto *c : Children) c->Write(simTimeS, level, tag, event, fields);
}

void BufferedLogSink::Write(double simTimeS, LogLevel level, const char *tag, const char *event,
                              const std::vector<LogField> &fields) {
  Lines.push_back({simTimeS, level, tag, event, fields});
}

void BufferedLogSink::Drain(LogSink &out) {
  for (const auto &l : Lines) out.Write(l.TimeS, l.Level, l.Tag.c_str(), l.Event.c_str(), l.Fields);
  Lines.clear();
}

} // namespace outshine::Clients
