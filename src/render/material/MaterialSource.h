#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALSOURCE_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALSOURCE_H

#include "Geometry.h"
#include "MaterialBinding.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::Render {
class MaterialLifetime;

struct MaterialSource {
  Geometry Source;
  std::vector<MaterialBinding> Slots;
  std::vector<uint32_t> RegisteredRows;
  std::shared_ptr<MaterialLifetime> Lifetime;
};
}
#endif
