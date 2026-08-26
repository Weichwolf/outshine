#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <cstdint>
#include <string_view>

#include "Geometry.h"

namespace outshine {

struct Ask {
  double EastM = 0.0;
  double NorthM = 0.0;
  double ExtentM = 0.0;
  uint64_t Seed = 0;
};

class Generates {
public:
  virtual ~Generates() = default;
  Generates(const Generates &) = delete;
  Generates &operator=(const Generates &) = delete;

  [[nodiscard]] virtual std::string_view Kind() const = 0;
  [[nodiscard]] virtual bool Make(const Ask &ask, Geometry &into) const = 0;

protected:
  Generates() = default;
};

}

#endif
