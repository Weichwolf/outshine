#ifndef OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORCAPTUREMATERIAL_H
#define OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORCAPTUREMATERIAL_H

#include "Material.h"
#include <optional>

namespace outshine {
[[nodiscard]] std::optional<Material> CapturedImpostorMaterial(const Material &source);
}
#endif
