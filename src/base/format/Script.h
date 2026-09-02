#ifndef OUTSHINE_BASE_FORMAT_SCRIPT_H
#define OUTSHINE_BASE_FORMAT_SCRIPT_H

#include <span>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Script {

inline constexpr size_t kMaxTokens = 4096;
inline constexpr size_t kMaxNodes = 2048;
inline constexpr size_t kMaxDepth = 32;
inline constexpr size_t kMaxSteps = 100000;
inline constexpr size_t kMaxNames = 128;
inline constexpr size_t kMaxArgs = 8;

enum class Kind : uint8_t {
  Nothing,
  Number,
  Text,

  Ref,
};

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

  [[nodiscard]] bool Truth() const;

  [[nodiscard]] std::string AsText() const;
};

struct Host {
  virtual ~Host() = default;

  [[nodiscard]] virtual Value Global(std::string_view name) {
    (void)name;
    return {};
  }

  [[nodiscard]] virtual Value Member(const Value &object, std::string_view name) {
    (void)object;
    (void)name;
    return {};
  }

  [[nodiscard]] virtual bool
  SetMember(const Value &object, std::string_view name, const Value &to) {
    (void)object;
    (void)name;
    (void)to;
    return false;
  }

  [[nodiscard]] virtual bool Call(const Value &callee, std::span<const Value> args, Value &out) {
    (void)callee;
    (void)args;
    (void)out;
    return false;
  }
};

[[nodiscard]] const char *WhyOutside(std::string_view name);

class Program {
public:
  struct Node;

  Program();
  ~Program();
  Program(const Program &) = delete;
  Program &operator=(const Program &) = delete;
  Program(Program &&) noexcept;
  Program &operator=(Program &&) noexcept;

  [[nodiscard]] bool Read(std::string_view text, std::string &error);
  [[nodiscard]] bool Held() const;

  [[nodiscard]] size_t NodeCount() const;

  [[nodiscard]] bool Run(Host &host, std::string &error);

  void Reset();

  [[nodiscard]] size_t Steps() const { return Steps_; }

  [[nodiscard]] const Value *Named(std::string_view name) const;

  [[nodiscard]] const std::string &Stopped() const { return Stopped_; }

private:
  [[nodiscard]] bool Evaluate(size_t at, Host &host, Value &out, std::string &error);
  [[nodiscard]] bool Perform(size_t at, Host &host, std::string &error);

  std::vector<Node> Nodes_;
  size_t Root_ = 0;
  std::vector<std::string> Names_;
  std::vector<Value> Held_;
  size_t Steps_ = 0;
  std::string Stopped_;
};

} // namespace outshine::Script
#endif
