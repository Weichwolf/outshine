#ifndef OUTSHINE_EVENT_H
#define OUTSHINE_EVENT_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace outshine {

struct Argument {
  enum class Kind : uint8_t { Number, Text };
  Kind Is = Kind::Number;
  double Number = 0.0;
  std::string_view Text;
};

class Host {
public:
  virtual ~Host() = default;
  [[nodiscard]] virtual bool calls(std::string_view name, std::span<const Argument> args) = 0;
};

struct Measure {
  std::string What;
  double How = 0.0;
  std::string Unit;
};

}

#endif
