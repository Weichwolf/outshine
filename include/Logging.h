#ifndef OUTSHINE_LOGGING_H
#define OUTSHINE_LOGGING_H

#include <span>
#include <string>

namespace outshine {

// WHAT THE ENGINE WOULD HAVE SAID, AND WHERE IT GOES. A client can run without any of this -- the
// engine refuses loudly through `error()` and reports through `measures()` -- but a client that
// wants the running commentary had to reach into `src/base/io` for the interface to receive it.
// The SINK is the client's; everything that decides what to emit stays behind the door.
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
