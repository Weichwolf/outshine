#ifndef OUTSHINE_GENERATORS_SHIPPED_H
#define OUTSHINE_GENERATORS_SHIPPED_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <generate/Generate.h>

#include "DrawSet.h"
#include "RoadMesher.h"
#include "StructureMesher.h"
#include "Forest.h"
#include "GeneratorSet.h"
#include "VegetationTemplates.h"

namespace outshine::Generators {

class Shipping {
public:
  Shipping();
  ~Shipping();
  Shipping(const Shipping &) = delete;
  Shipping &operator=(const Shipping &) = delete;

  [[nodiscard]] bool Stands(const outshine::Ground::VegetationTemplates &declared,
                            std::string_view speciesDir,
                            std::string &error);

  [[nodiscard]] bool Ready() const { return !Made_.empty(); }

  [[nodiscard]] const GeneratorSet &Placing() const { return Placing_; }

  [[nodiscard]] const DrawSet &Drawing() const { return Drawing_; }

  [[nodiscard]] const StructureMesher &Shaping() const { return *Shaper_; }

  [[nodiscard]] const RoadMesher &Paving() const { return *Paver_; }

  [[nodiscard]] const Generator &Offered() const { return *Offered_; }

private:
  std::unique_ptr<Generator> Offered_;
  std::unique_ptr<StructureMesher> Shaper_;
  std::unique_ptr<RoadMesher> Paver_;
  std::vector<Forest::Stem> Stems_;
  std::vector<float> PerM2_;
  std::vector<std::unique_ptr<Making>> Made_;
  std::vector<std::unique_ptr<DrawSource>> Draws_;
  GeneratorSet Placing_;
  DrawSet Drawing_;
};

} // namespace outshine::Generators
#endif
