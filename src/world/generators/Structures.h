#ifndef OUTSHINE_WORLD_GENERATORS_STRUCTURES_H
#define OUTSHINE_WORLD_GENERATORS_STRUCTURES_H

#include <Generate.h>

namespace outshine::Generators {

class Structures final : public Generates {
public:
  [[nodiscard]] std::string_view Kind() const override { return "structures"; }
  [[nodiscard]] bool Make(const Ask &ask, Geometry &into) const override;
};

}

#endif
