#ifndef OUTSHINE_SCRIPTING_SCRIPT_H
#define OUTSHINE_SCRIPTING_SCRIPT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Script {

inline constexpr size_t kMaxNames = 128;
inline constexpr size_t kMaxArgs = 8;

enum class Kind : uint8_t { Nothing, Number, Text, Ref, Boolean, Null };

struct Value {
  Kind What = Kind::Nothing;
  double Number = 0.0;
  std::string Text;
  int Ref = 0;

  [[nodiscard]] static Value OfNumber(double number) {
    Value out;
    out.What = Kind::Number;
    out.Number = number;
    return out;
  }

  [[nodiscard]] static Value OfText(std::string text) {
    Value out;
    out.What = Kind::Text;
    out.Text = std::move(text);
    return out;
  }

  [[nodiscard]] static Value OfRef(int ref) {
    Value out;
    out.What = Kind::Ref;
    out.Ref = ref;
    return out;
  }

  [[nodiscard]] static Value OfBoolean(bool value) {
    Value out;
    out.What = Kind::Boolean;
    out.Number = value ? 1.0 : 0.0;
    return out;
  }

  [[nodiscard]] static Value OfNull() {
    Value out;
    out.What = Kind::Null;
    return out;
  }

  [[nodiscard]] bool Truth() const;
};

struct Host {
  virtual ~Host() = default;

  [[nodiscard]] virtual Value Global([[maybe_unused]] std::string_view name) { return {}; }

  [[nodiscard]] virtual Value Member([[maybe_unused]] const Value &object,
                                     [[maybe_unused]] std::string_view name) {
    return {};
  }

  [[nodiscard]] virtual bool SetMember([[maybe_unused]] const Value &object,
                                       [[maybe_unused]] std::string_view name,
                                       [[maybe_unused]] const Value &value) {
    return false;
  }

  [[nodiscard]] virtual bool Call([[maybe_unused]] const Value &callee,
                                  [[maybe_unused]] std::span<const Value> args,
                                  [[maybe_unused]] Value &out) {
    return false;
  }
};

struct Budget {
  size_t InputBytes = size_t{256} * 1024;
  size_t MemoryBytes = size_t{8} * 1024 * 1024;
  size_t StackBytes = size_t{256} * 1024;
  size_t InterruptChecks = 128;
  size_t HostAccesses = 256;
  size_t HostBytes = size_t{64} * 1024;
  std::chrono::microseconds Time = std::chrono::milliseconds(2);
};

struct Runtime;

class Program {
public:
  explicit Program(Budget budget = {});
  ~Program();
  Program(const Program &) = delete;
  Program &operator=(const Program &) = delete;
  Program(Program &&) noexcept;
  Program &operator=(Program &&) noexcept;

  [[nodiscard]] bool Read(std::string_view text, std::string &error);
  // A successfully entered program binds this host instance until its next Read.
  [[nodiscard]] bool Run(Host &host, std::string &error);
  [[nodiscard]] bool Held() const;
  [[nodiscard]] size_t InterruptChecks() const;

private:
  Budget Budget_;
  std::unique_ptr<Runtime> Runtime_;
};

}
#endif
