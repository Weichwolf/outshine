#ifndef OUTSHINE_LOGGING_H
#define OUTSHINE_LOGGING_H

#include <cstdint>
#include <span>
#include <concepts>
#include <string>

namespace outshine {

enum class LogLevel { Debug, Info, Warn, Error };

/// WHICH SUBSYSTEM IS SPEAKING, and it is a closed set rather than a string.
///
/// A `const char *` here is a bin: every caller may spell its own, so `ground` and `Ground` and
/// `terrain` become three subsystems that are one and no query over the log finds them all. The
/// tree has four, measured, and four is a type. The EVENT stays text because it is open -- a new
/// diagnostic names a new event -- but the speaker is not.
///
/// **A log is for events a HUMAN reads backwards after something went wrong.** Frequency decides
/// the channel: once per run or per load is a log line; once per frame is a number the ledger
/// carries; once per entity per frame is neither, and outshine will hold hundreds of thousands of
/// those. A thousand NPCs losing their target is ONE number, never a thousand lines.
enum class LogTag : uint8_t { Ground, Render, Veg, World };

/// The tag as it is written in a line.
/// @param tag Which subsystem.
/// @return Its spelling, stable enough to grep for.
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

  LogField(const char *key, long long v);

  /// Constrained to `bool` ITSELF, and that constraint is the whole reason this type needs no
  /// `const char *` overload beside its `std::string` one: an unconstrained `bool` parameter wins
  /// `{"name", "car"}` outright, because pointer-to-bool is a standard conversion and
  /// pointer-to-string is a user-defined one. Narrow the greedy overload and the right one wins.
  template <typename B>
    requires std::same_as<B, bool>
  LogField(const char *key, B v) : Key(key), Value(v ? "1" : "0") {}

  LogField(const char *key, std::string v);
};

class LogSink {
public:
  virtual ~LogSink() = default;

  /// Who is speaking and about what. Three `const char *` in a row, and a line written with the
  /// tag and the event reversed reads as a different subsystem saying nothing recognisable.
  struct Saying {
    const char *Unit = nullptr;
    LogTag Tag = LogTag::World;
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
