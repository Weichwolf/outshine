#ifndef OUTSHINE_RENDER_MATERIAL_MATERIALSOURCE_H
#define OUTSHINE_RENDER_MATERIAL_MATERIALSOURCE_H

#include "Geometry.h"
#include "MaterialBinding.h"
#include <vector>

namespace outshine::Render {
struct MaterialSource {
  Geometry Source;
  std::vector<MaterialBinding> Slots;
};
}
#endif
