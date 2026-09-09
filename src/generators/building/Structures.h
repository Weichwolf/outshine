#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURES_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURES_H

#include <generation/Generate.h>

namespace outshine::Generators {

class Structures final : public Generator {
public:
  [[nodiscard]] std::string_view kind() const override { return nameOf(Shipped::Structures); }

  [[nodiscard]] bool make(const Request &asked, Geometry &into) const override;
};

}

#endif
