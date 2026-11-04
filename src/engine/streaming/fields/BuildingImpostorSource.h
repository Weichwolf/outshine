#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGIMPOSTORSOURCE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGIMPOSTORSOURCE_H

#include "BuildingSourceView.h"
#include "ImpostorViewAssets.h"
#include "PreparedBuildingAssets.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Render {
class ImpostorBaker;
}

namespace outshine {
class BuildingSourceRead;

class BuildingImpostorSource {
public:
  struct Recipe {
    std::string BaseKey;
    std::vector<uint32_t> Cells;
    std::vector<uint32_t> Sources;
    Generators::BuildingCaptureFrame Frame;
    std::vector<Vec3> Directions;
    std::array<Material, 3> WallRoofMetreMaterials;
    uint32_t Pixels = 128;
    [[nodiscard]] std::string Key() const;
  };

  BuildingImpostorSource(Recipe recipe,
                         std::shared_ptr<Generators::PreparedBuildingAssets> assets,
                         std::shared_ptr<BuildingSourceRead> read = {});

  [[nodiscard]] const std::string &Provenance() const noexcept { return Provenance_; }

  // Execute on the content worker; only complete selected source sets publish metadata.
  [[nodiscard]] std::expected<ImpostorViewAssets, std::string>
  ResolveMetadata(AssetCache &cache, const std::stop_token &stop = {});
  [[nodiscard]] std::expected<ImpostorAtlas, std::string>
  Generate(ImpostorViewAssets::ViewIndex view,
           size_t bytesMost,
           Generators::BuildingSourceBudget budget,
           const std::stop_token &stop = {});
  // The supplied capture context belongs to this calling content thread.
  [[nodiscard]] std::expected<ImpostorAtlas, std::string>
  Generate(Render::ImpostorBaker &captures,
           ImpostorViewAssets::ViewIndex view,
           size_t bytesMost,
           Generators::BuildingSourceBudget budget,
           const std::stop_token &stop = {});

private:
  [[nodiscard]] std::expected<Generators::BuildingSourceView, std::string> Source() const;
  [[nodiscard]] ImpostorViewAssets::Description
  Description(const Generators::BuildingSourceView &source) const;
  std::shared_ptr<BuildingSourceRead> Read_;
  Recipe Recipe_;
  std::string Provenance_;
  std::shared_ptr<Generators::PreparedBuildingAssets> Assets_;
};
}
#endif
