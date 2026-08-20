#ifndef GLTF_EMIT_H
#define GLTF_EMIT_H

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

}
#endif
