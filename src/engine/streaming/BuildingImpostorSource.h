#ifndef OUTSHINE_ENGINE_STREAMING_BUILDINGIMPOSTORSOURCE_H
#define OUTSHINE_ENGINE_STREAMING_BUILDINGIMPOSTORSOURCE_H

#include "BuildingSourceView.h"
#include "ImpostorViewAssets.h"
#include "PreparedBuildingAssets.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine {
class BuildingImpostorSource {
public:
  struct Recipe {
    std::string BaseKey;
    std::vector<uint32_t> Cells;
    Generators::BuildingCaptureFrame Frame;
    std::vector<Vec3> Directions;
    std::array<Material, 3> WallRoofMetreMaterials;
    uint32_t Pixels = 128;
    [[nodiscard]] std::string Key() const;
  };

  class MetadataPreparation {
  public:
    MetadataPreparation() noexcept;
    MetadataPreparation(MetadataPreparation &&) noexcept;
    MetadataPreparation &operator=(MetadataPreparation &&) noexcept;
    ~MetadataPreparation();
    // Inspect between worker steps. Package capacities exclude decoded native sources.
    [[nodiscard]] size_t PublicationBytes() const noexcept;
    [[nodiscard]] uint64_t Preparations() const noexcept;

  private:
    friend class BuildingImpostorSource;
    struct State;
    std::unique_ptr<State> State_;
  };

  BuildingImpostorSource(Recipe recipe, std::shared_ptr<Generators::PreparedBuildingAssets> assets);

  [[nodiscard]] const std::string &Provenance() const noexcept { return Provenance_; }

  // Execute on the content worker; only complete selected source sets publish metadata.
  [[nodiscard]] std::expected<std::optional<Content::ImpostorViewAssets>, std::string>
  ResolveMetadata(AssetCache &cache,
                  MetadataPreparation &preparation,
                  const std::stop_token &stop = {});
  [[nodiscard]] std::expected<ImpostorAtlas, std::string>
  Generate(Content::ImpostorViewAssets::ViewIndex view,
           size_t bytesMost,
           Generators::BuildingPlaneCapture::Budget budget,
           const std::stop_token &stop = {});

private:
  [[nodiscard]] std::expected<const Generators::BuildingSourceView *, std::string> Source();
  [[nodiscard]] std::expected<const Generators::BuildingSourceView *, std::string>
  ReadMetadataSource(MetadataPreparation &preparation, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, std::string>
  PrepareMetadata(AssetCache &cache, MetadataPreparation &preparation, const std::stop_token &stop);
  // Lock_ is held; a previously published immutable view is never replaced.
  [[nodiscard]] std::expected<const Generators::BuildingSourceView *, std::string>
  SourceFromBase(const Generators::PreparedBuildingAssets::Base &base);
  [[nodiscard]] Content::ImpostorViewAssets::Description
  Description(const Generators::BuildingSourceView &source) const;
  Recipe Recipe_;
  std::string Provenance_;
  std::shared_ptr<Generators::PreparedBuildingAssets> Assets_;
  std::mutex Lock_;
  std::optional<Generators::BuildingSourceView> Source_;
  Vec3 AnchorEcef_;
};
}
#endif
