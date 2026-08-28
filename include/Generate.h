#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <cstddef>
#include <cstdint>
#include <memory>
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

class Makers {
public:
  [[nodiscard]] bool Offers(const Generates &maker);

  [[nodiscard]] const Generates *Named(std::string_view kind) const;
  [[nodiscard]] size_t Count() const;

  Makers();
  ~Makers();
  Makers(Makers &&) noexcept;
  Makers &operator=(Makers &&) noexcept;
  Makers(const Makers &) = delete;
  Makers &operator=(const Makers &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}

#endif
