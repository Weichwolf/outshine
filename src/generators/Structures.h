#ifndef OUTSHINE_GENERATORS_STRUCTURES_H
#define OUTSHINE_GENERATORS_STRUCTURES_H

#include <Generate.h>

namespace outshine::Generators {

class Structures final : public Generates {
public:
  [[nodiscard]] std::string_view Kind() const override { return NameOf(Ships::Structures); }
  [[nodiscard]] bool Make(const Ask &ask, Geometry &into) const override;
};

}

#endif
