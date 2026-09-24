#ifndef OUTSHINE_EVENT_H
#define OUTSHINE_EVENT_H

#include <cstdint>
#include <span>
#include <string_view>

namespace outshine {

/// Tagged callback value; numeric data is copied, text remains borrowed.
/// Read only the field selected by Is. Copies do not extend Text's lifetime.
/// Arguments handed to Host::calls are valid only for that invocation; copy text
/// into owned storage to retain it. No implicit conversion or unit is specified.
struct Argument {
  /// Active payload selected by the sender.
  enum class Kind : uint8_t {
    Number, ///< Number is active.
    Text    ///< Text is active.
  };
  Kind Is = Kind::Number; ///< Discriminant; callers must provide a valid Kind.
  double Number = 0.0;    ///< Numeric payload; action semantics define units and valid range.
  std::string_view Text;  ///< Borrowed text payload, not necessarily null-terminated.
};

/// Application-owned synchronous receiver of named engine actions.
/// Engine::setInputHost borrows this object until replacement, detachment or engine destruction.
/// Calls execute on the invoking engine thread; there is no dispatcher or synchronization.
/// Keep callbacks bounded, do not throw, and do not reenter or destroy the invoking engine.
/// Multiple engines sharing one host require external synchronization or a thread-safe host.
class Host {
public:
  /// Release the receiver; detach it from every live engine before destruction.
  virtual ~Host() = default;
  /// Process one action without retaining borrowed input storage.
  /// @param name Exact action identifier, borrowed for this call; no terminator is promised.
  /// @param args Ordered borrowed values, valid only until this call returns.
  /// @return True if handled; false if refused or unsupported. False does not roll back
  /// application side effects. A rejected script call stops the current script execution.
  [[nodiscard]] virtual bool calls(std::string_view name, std::span<const Argument> args) = 0;
};

}

#endif
