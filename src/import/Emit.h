#ifndef OUTSHINE_IMPORT_EMIT_H
#define OUTSHINE_IMPORT_EMIT_H

#include <span>
#include <cstdint>
#include <string>
#include <vector>

#include "Subject.h"
#include "Types.h"

namespace outshine::Gltf {

struct Emission {
  const Subject *Geometry = nullptr;
  std::span<const MaterialRef> Materials;
  std::string Generator;
};

[[nodiscard]] bool Emit(const Emission &what, std::vector<uint8_t> &glb, std::string &error);

struct GlbParts {
  size_t JsonBytes = 0;
  size_t BinaryBytes = 0;
};

[[nodiscard]] bool GlbFits(GlbParts of);

} // namespace outshine::Gltf
#endif
