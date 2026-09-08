#ifndef OUTSHINE_LOGGING_H
#define OUTSHINE_LOGGING_H

#include <concepts>
#include <cstdint>
#include <span>
#include <string>

namespace outshine {

/// Severity ordered from diagnostic detail to operational failure.
enum class LogLevel {
  Debug, ///< Detailed diagnostics, normally disabled in routine operation.
  Info,  ///< Normal lifecycle events.
  Warn,  ///< Recoverable anomalies or degraded operation.
  Error  ///< An operation failed.
};

/// Subsystem identifiers. Event names remain application-defined text.
enum class LogTag : uint8_t {
  Ground, ///< Terrain data and geometry.
  Render, ///< Graphics resources and rendering.
  Veg,    ///< Vegetation generation and residency.
  World   ///< World orchestration and other simulation events.
};

/// Obtain the stable textual subsystem identifier.
/// @param tag Subsystem to describe.
/// @return Static storage; an empty string for an invalid enum value.
[[nodiscard]] constexpr const char *nameOf(LogTag tag) {
  switch (tag) {
    case LogTag::Ground: return "ground";
    case LogTag::Render: return "render";
    case LogTag::Veg: return "veg";
    case LogTag::World: return "world";
  }
  return "";
}

/// A diagnostic field with a borrowed key and owned, already formatted value.
/// Copying the field copies the value but does not extend the key's lifetime.
struct LogField {
  const char *Key;   ///< Non-null, null-terminated key; must outlive every use of this field.
  std::string Value; ///< Owned text; escaping for an output format is the sink's responsibility.

  /// Format a floating-point diagnostic using C %g, default precision and the current C locale.
  /// @param key Borrowed field name, not null.
  /// @param v Numeric value; infinities and NaN are represented as diagnostic text.
  LogField(const char *key, double v);

  /// Format a signed integer in decimal.
  /// @param key Borrowed field name, not null.
  /// @param v Integer to represent.
  LogField(const char *key, int v);

  /// Format a wide signed integer in decimal.
  /// @param key Borrowed field name, not null.
  /// @param v Integer to represent without floating-point conversion.
  LogField(const char *key, long long v);

  /// Store a boolean as "1" or "0". Only bool matches, so strings never convert to boolean.
  /// @tparam B Exactly bool.
  /// @param key Borrowed field name, not null.
  /// @param v Boolean value.
  template <typename B>
    requires std::same_as<B, bool>
  LogField(const char *key, B v) : Key(key), Value(v ? "1" : "0") {}

  /// Take ownership of a textual value.
  /// @param key Borrowed field name, not null.
  /// @param v Text copied or moved into this field.
  LogField(const char *key, std::string v);
};

/// Synchronous diagnostic callback interface. The registered sink is borrowed by the engine.
/// Producers may call Write concurrently; implementations must protect their mutable state.
/// Registration/replacement requires quiescent producers, and the sink must outlive registration
/// and outstanding calls. A queued sink must copy all borrowed data before Write returns.
class LogSink {
public:
  /// Destroy only after unregistering and completing outstanding callbacks.
  virtual ~LogSink() = default;

  /// Borrowed event identity, valid for the duration of Write.
  struct Saying {
    const char *Unit = nullptr; ///< Optional null-terminated emitter label; null means unspecified.
    LogTag Tag = LogTag::World; ///< Subsystem emitting this event.
    const char *Event = nullptr; ///< Required non-null event name when passed to Write.
  };

  /// Consume one event on the emitting thread. Exceptions are not caught at this boundary;
  /// implementations should report sink failures through an independent channel.
  /// @param simTimeS Emitting thread's simulation time, in seconds; not wall-clock time.
  /// @param level Event severity.
  /// @param who Borrowed identity; copy the strings before retaining it beyond this call.
  /// @param fields Borrowed fields; retaining them requires copying values and key strings.
  virtual void
  Write(double simTimeS, LogLevel level, Saying who, std::span<const LogField> fields) = 0;
};

}

#endif
