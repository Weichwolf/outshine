
#include "generate/Generate.h"

#include "Emit.h"
#include "Subject.h"
#include <vector>
#include <cstdint>
#include <string>
#include <cstddef>
#include <utility>

namespace outshine {

bool writeGlb(const Geometry &what, std::vector<uint8_t> &glb, std::string &error) {
  Gltf::Subject stood;
  if (!stood.Assemble(what)) {
    error =
        stood.Error().empty() ? "the geometry would not assemble into a subject" : stood.Error();
    return false;
  }
  std::vector<Gltf::MaterialRef> wearing;
  wearing.reserve(stood.Surfaces().size());
  for (size_t at = 0; at < stood.Surfaces().size(); ++at) {
    Gltf::MaterialRef one;
    one.Name = "surface" + std::to_string(at);
    one.Surface = stood.Surfaces()[at];
    wearing.push_back(std::move(one));
  }
  Gltf::Emission emission;
  emission.Geometry = &stood;
  emission.Materials = Span<const Gltf::MaterialRef>(wearing.data(), wearing.size());
  emission.Generator = "outshine generators";
  return Gltf::Emit(emission, glb, error);
}

} // namespace outshine
