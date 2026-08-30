#ifndef OUTSHINE_LOGGING_H
#define OUTSHINE_LOGGING_H

#include <span>
#include <string>

namespace outshine {

enum class LogLevel { Debug, Info, Warn, Error };

struct LogField {
  const char *Key;
  std::string Value;
  LogField(const char *key, double v);
  LogField(const char *key, int v);

  LogField(const char *key, long long v);
  LogField(const char *key, bool v);
  LogField(const char *key, const char *v);
  LogField(const char *key, const std::string &v);
};

class LogSink {
public:
  virtual ~LogSink() = default;
  virtual void Write(double simTimeS,
                     LogLevel level,
                     const char *unit,
                     const char *tag,
                     const char *event,
                     std::span<const LogField> fields) = 0;
};

} // namespace outshine

#endif
