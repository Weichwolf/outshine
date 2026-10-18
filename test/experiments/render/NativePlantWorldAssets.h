#ifndef OUTSHINE_TEST_NATIVE_PLANT_WORLD_ASSETS_H
#define OUTSHINE_TEST_NATIVE_PLANT_WORLD_ASSETS_H

#include "PlantCrownAsset.h"
#include "PlantLimbAsset.h"
#include "content/AssetGeneration.h"
#include "content/GeometryAsset.h"
#include "Sha256.h"
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::NativePlantWorldProbe {
constexpr size_t kAssetBytesMost = size_t{16} * 1024 * 1024;

inline std::string Request(std::string_view root, std::string_view kind) {
  const std::string text = std::string(root) + "/" + std::string(kind);
  return Sha256Hex(text.data(), text.size());
}

struct Source {
  Generators::PlantCrownPlan Crown;
  std::vector<Generators::PlantLimbPlan> Wood;
  Geometry Blades;
};

inline std::expected<Source, std::string> Load(AssetCache &cache, std::string_view request) {
  const auto crown = ResolveAssetRequest(cache, Request(request, "crown"), kAssetBytesMost, {});
  const auto wood = ResolveAssetRequest(cache, Request(request, "wood"), kAssetBytesMost, {});
  const auto blades = ResolveAssetRequest(cache, Request(request, "blades"), kAssetBytesMost, {});
  if (!crown || !wood || !blades) { return std::unexpected("prepared plant package is missing"); }
  if (crown->Record().Kind != "plant-crown-plan" || wood->Record().Kind != "plant-limb-plan" ||
      blades->Record().Kind != "plant-blade-geometry" ||
      crown->Record().Package != wood->Record().Package ||
      crown->Record().Package != blades->Record().Package ||
      wood->Record().Parent != crown->Record().Key ||
      blades->Record().Parent != crown->Record().Key) {
    return std::unexpected("prepared plant package has incompatible source bindings");
  }
  auto leaves = Generators::DecodePlantCrownPlan(crown->Bytes(), kAssetBytesMost);
  auto limbs = Generators::DecodePlantLimbs(wood->Bytes(), kAssetBytesMost);
  auto geometry = DecodeGeometryAsset(blades->Bytes(), kAssetBytesMost);
  if (!leaves || !limbs || !geometry || geometry->parts() != 8) {
    return std::unexpected("prepared plant fields cannot be decoded");
  }
  return Source{
      .Crown = std::move(*leaves), .Wood = std::move(*limbs), .Blades = std::move(*geometry)};
}
}
#endif
