
#include <span>
#include "export/GltfExporter.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include <expected>
#include <algorithm>
#include <cmath>

#include "Emit.h"
#include "Subject.h"
#include <vector>
#include <cstdint>
#include <string>
#include <cstddef>
#include <utility>

namespace outshine {

namespace {
namespace Says {
constexpr auto Unsupported = "GLB export cannot preserve these images or material properties";
constexpr auto Assembly = "native geometry cannot be assembled for GLB export";
}

[[nodiscard]] bool Unit(float value) noexcept {
  return std::isfinite(value) && value >= 0 && value <= 1;
}

[[nodiscard]] bool Supported(const Material &material) noexcept {
  Material encoded;
  encoded.BaseColour = material.BaseColour;
  encoded.Metalness = material.Metalness;
  encoded.Roughness = material.Roughness;
  encoded.Emission = material.Emission;
  encoded.Alpha = material.Alpha;
  encoded.DoubleSided = material.DoubleSided;
  encoded.CoverageCut = material.CoverageCut;
  encoded.Unlit = material.Unlit;
  encoded.NeedsTangents = material.NeedsTangents;
  return encoded == material && std::ranges::all_of(material.BaseColour, Unit) &&
         std::ranges::all_of(material.Emission, Unit) && Unit(material.Metalness) &&
         Unit(material.Roughness) && std::isfinite(material.CoverageCut) &&
         material.CoverageCut >= 0 &&
         (material.Alpha == AlphaMode::Opaque || material.Alpha == AlphaMode::Masked ||
          material.Alpha == AlphaMode::Blended);
}
}

std::expected<std::vector<uint8_t>, std::string> exportGlb(const Geometry &geometry) {
  if (geometry.images() != 0) { return std::unexpected(Says::Unsupported); }
  for (int at = 0; at < geometry.surfaces(); ++at) {
    if (!Supported(geometry.surfaceAt(MaterialInstance(at)))) {
      return std::unexpected(Says::Unsupported);
    }
  }
  Gltf::Subject stood;
  if (!stood.Assemble(geometry)) {
    return std::unexpected(stood.Error().empty() ? std::string(Says::Assembly) : stood.Error());
  }
  std::vector<Gltf::Material> wearing;
  wearing.reserve(stood.Surfaces().size());
  for (size_t at = 0; at < stood.Surfaces().size(); ++at) {
    Gltf::Material one;
    one.Name = geometry.surfaceNameOf(static_cast<int>(at));
    one.Surface = stood.Surfaces()[at];
    wearing.push_back(std::move(one));
  }
  Gltf::Emission emission;
  emission.Geometry = &stood;
  emission.Materials = std::span<const Gltf::Material>(wearing.data(), wearing.size());
  emission.Generator = "outshine";
  std::vector<uint8_t> glb;
  std::string error;
  if (!Gltf::Emit(emission, glb, error)) { return std::unexpected(std::move(error)); }
  return glb;
}

}
