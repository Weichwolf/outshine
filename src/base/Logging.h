#ifndef OUTSHINE_LOGGING_H
#define OUTSHINE_LOGGING_H

#include <concepts>
#include <cstdint>
#include <span>
#include <string>

namespace outshine {

enum class LogLevel { Debug, Info, Warn, Error };

enum class LogTag : uint8_t { Ground, Render, Veg, World };

[[nodiscard]] constexpr const char *nameOf(LogTag tag) {
  switch (tag) {
    case LogTag::Ground: return "ground";
    case LogTag::Render: return "render";
    case LogTag::Veg: return "veg";
    case LogTag::World: return "world";
  }
  return "";
}

struct LogField {
  const char *Key;
  std::string Value;

  LogField(const char *key, double v);

  LogField(const char *key, int v);

  LogField(const char *key, long v);

  LogField(const char *key, long long v);

  template <typename B>
    requires std::same_as<B, bool>
  LogField(const char *key, B v) : Key(key), Value(v ? "1" : "0") {}

  LogField(const char *key, std::string v);
};

class LogSink {
public:
  virtual ~LogSink() = default;

  struct Saying {
    const char *Unit = nullptr;
    LogTag Tag = LogTag::World;
    const char *Event = nullptr;
  };

  virtual void
  Write(double simTimeS, LogLevel level, Saying who, std::span<const LogField> fields) noexcept = 0;
};

}

#endif
