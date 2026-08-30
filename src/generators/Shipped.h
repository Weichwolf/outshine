#ifndef OUTSHINE_GENERATORS_SHIPPED_H
#define OUTSHINE_GENERATORS_SHIPPED_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "DrawSet.h"
#include "Forest.h"
#include "GeneratorSet.h"
#include "VegetationTemplates.h"

namespace outshine::Generators {

class Shipping {
public:
  [[nodiscard]] bool Stands(const outshine::Ground::VegetationTemplates &declared,
                            std::string_view speciesDir,
                            std::string &error);

  [[nodiscard]] bool Ready() const { return !Made_.empty(); }

  [[nodiscard]] const GeneratorSet &Placing() const { return Placing_; }

  [[nodiscard]] const DrawSet &Drawing() const { return Drawing_; }

private:
  std::vector<Forest::Stem> Stems_;
  std::vector<float> PerM2_;
  std::vector<std::unique_ptr<Making>> Made_;
  std::vector<std::unique_ptr<DrawSource>> Draws_;
  GeneratorSet Placing_;
  DrawSet Drawing_;
};

} // namespace outshine::Generators
#endif
