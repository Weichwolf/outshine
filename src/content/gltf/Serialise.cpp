// THE EXPORT SIDE OF THE SAME DOOR. `writeGlb` is a door verb -- `include/Generate.h` declares it and
// `geo/ScoreWhatTheGeneratorDoorHolds` proves it -- but its BODY speaks glTF, and it stood in
// `src/generators/`, which put six spellings of the importer's namespace in the tier whose job is to
// GROW things. Import and export are the same door seen from two sides; the rule that keeps one
// behind `src/content/gltf/` keeps the other.

#include "Generate.h"

#include "Emit.h"
#include "Subject.h"

namespace outshine {

bool writeGlb(const Geometry &what, std::vector<uint8_t> &glb, std::string &error) {
  Gltf::Subject stood;
  if (!stood.Assemble(what)) {
    error = stood.Error().empty() ? "the geometry would not assemble into a subject"
                                  : stood.Error();
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

}
