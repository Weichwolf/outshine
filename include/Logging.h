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

  /// Two `const char *` in a row, which `bugprone-easily-swappable-parameters` flags and which is
  /// kept ANYWAY: dropping it makes `{"name", "car"}` bind to the `bool` overload and narrow, and
  /// the alternative -- a strong key type -- would put ceremony at every one of the tree's logging
  /// sites. The finding stands recorded rather than traded for something worse.
  LogField(const char *key, const char *v);
  LogField(const char *key, std::string v);
};

class LogSink {
public:
  virtual ~LogSink() = default;

  /// Who is speaking and about what. Three `const char *` in a row, and a line written with the
  /// tag and the event reversed reads as a different subsystem saying nothing recognisable.
  struct Saying {
    const char *Unit = nullptr;
    const char *Tag = nullptr;
    const char *Event = nullptr;
  };

  /// One line.
  /// @param simTimeS The simulation clock when it was said.
  /// @param level How loud.
  /// @param who Which unit, which tag, which event.
  /// @param fields The named values that go with it.
  virtual void
  Write(double simTimeS, LogLevel level, Saying who, std::span<const LogField> fields) = 0;
};

} // namespace outshine

#endif
