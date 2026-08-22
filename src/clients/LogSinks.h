#ifndef LOGSINKS_H
#define LOGSINKS_H
#include <cstdio>
#include <vector>
#include "Log.h"
#include "TextTarget.h"

namespace outshine::Clients {

class TextLogSink : public LogSink {
public:
  explicit TextLogSink(const TextTarget &target) : File_(target.File()) {}
  void Write(double simTimeS, LogLevel level, const char *unit, const char *tag,
             const char *event,
            std::span<const LogField> fields) override;

private:
  std::FILE *File_;
};

class LogSinkScope {
public:
  explicit LogSinkScope(LogSink *sink) { Log::SetSink(sink); }
  ~LogSinkScope() { Log::SetSink(nullptr); }
  LogSinkScope(const LogSinkScope &) = delete;
  LogSinkScope &operator=(const LogSinkScope &) = delete;
};

}
#endif
