#ifndef OUTSHINE_GLTF_EMIT_H
#define OUTSHINE_GLTF_EMIT_H

#include <cstdint>
#include <string>
#include <vector>

#include "Span.h"

#include "Subject.h"
#include "Types.h"

namespace outshine::Gltf {

struct Emission {
  const Subject *Geometry = nullptr;
  Span<const MaterialRef> Materials;
  std::string Generator;
};

[[nodiscard]] bool Emit(const Emission &what, std::vector<uint8_t> &glb, std::string &error);

// the GLB container's 32-bit ceiling: header + two chunk headers + both padded chunks
// must fit uint32, or the header would lie about its own length -- exposed so the bound
// is provable without four gibibytes of fixture
[[nodiscard]] bool GlbFits(size_t jsonBytes, size_t binaryBytes);

}
#endif
