#ifndef OUTSHINE_GENERATORS_STRUCTURES_H
#define OUTSHINE_GENERATORS_STRUCTURES_H

#include <Generate.h>

namespace outshine::Generators {

class Structures final : public Generates {
public:
  [[nodiscard]] std::string_view kind() const override { return nameOf(Ships::Structures); }

  [[nodiscard]] bool make(const Ask &ask, Geometry &into) const override;
};

} // namespace outshine::Generators

#endif
