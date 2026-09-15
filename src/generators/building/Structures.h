#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURES_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURES_H

#include <generation/Generate.h>

namespace outshine::Generators {

class Structures final : public Generator {
public:
  [[nodiscard]] std::string_view kind() const override { return nameOf(Shipped::Structures); }

  [[nodiscard]] Product make(const Request &asked) const override;
};

}

#endif
