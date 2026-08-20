#ifndef SCRIPT_H
#define SCRIPT_H

#include <cstddef>
#include <cstdint>
#include <string>
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

  static Value OfNumber(double number) {
    Value out;
    out.What = Kind::Number;
    out.Number = number;
    return out;
  }
  static Value OfText(std::string text) {
    Value out;
    out.What = Kind::Text;
    out.Text = std::move(text);
    return out;
  }
  static Value OfRef(int ref) {
    Value out;
    out.What = Kind::Ref;
    out.Ref = ref;
    return out;
  }

  [[nodiscard]] bool Truth(void) const;

  [[nodiscard]] std::string AsText(void) const;
};

struct Host {
  virtual ~Host() = default;

  [[nodiscard]] virtual Value Global(const std::string &name) {
    (void)name;
    return {};
  }
  [[nodiscard]] virtual Value Member(const Value &object, const std::string &name) {
    (void)object;
    (void)name;
    return {};
  }
  [[nodiscard]] virtual bool SetMember(const Value &object, const std::string &name,
                                       const Value &to) {
    (void)object;
    (void)name;
    (void)to;
    return false;
  }

  [[nodiscard]] virtual bool Call(const Value &callee, const Value *args, size_t count, Value &out) {
    (void)callee;
    (void)args;
    (void)count;
    (void)out;
    return false;
  }
};

[[nodiscard]] const char *WhyOutside(const std::string &name);

class Program {
public:

  struct Node;

  Program();
  ~Program();
  Program(const Program &) = delete;
  Program &operator=(const Program &) = delete;
  Program(Program &&) noexcept;
  Program &operator=(Program &&) noexcept;

  [[nodiscard]] bool Read(const std::string &text, std::string &error);
  [[nodiscard]] bool Held(void) const;

  [[nodiscard]] size_t NodeCount(void) const;

  [[nodiscard]] bool Run(Host &host, std::string &error);

  void Reset(void);

  [[nodiscard]] size_t Steps(void) const { return Steps_; }

  [[nodiscard]] const Value *Named(const std::string &name) const;

  [[nodiscard]] const std::string &Stopped(void) const { return Stopped_; }

private:
  [[nodiscard]] bool Evaluate(size_t node, Host &host, Value &out, std::string &error);
  [[nodiscard]] bool Perform(size_t node, Host &host, std::string &error);

  std::vector<Node> Nodes_;
  size_t Root_ = 0;
  std::vector<std::string> Names_;
  std::vector<Value> Held_;
  size_t Steps_ = 0;
  std::string Stopped_;
};

}
#endif
