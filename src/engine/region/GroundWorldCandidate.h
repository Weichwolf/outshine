#ifndef OUTSHINE_ENGINE_REGION_GROUNDWORLDCANDIDATE_H
#define OUTSHINE_ENGINE_REGION_GROUNDWORLDCANDIDATE_H

#include "SceneLighting.h"
#include "EngineState.h"
#include "WorldCandidate.h"
#include "TerrainClassificationMap.h"
#include "PackedTerrainClassification.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine {
struct GroundBuildProducts {
  HeightSheets Sheets;
  ::outshine::Generators::Osm::BuildingField Footprints;
  Render::GeometryPageResidency Pieces;
  Geometry Ground;
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
  std::vector<float> ClassPalette;
  std::shared_ptr<const TerrainClassificationMap> TerrainClassificationMap;
  std::shared_ptr<const Render::PackedTerrainClassification> ClassUpload;
  Material GroundMaterial;
  MaterialInstance GroundSurface;
  std::shared_ptr<const Path::Network> StreetGraph;
  size_t StreetGraphWayCount = 0;
  bool StreetGraphComplete = true;
  std::vector<NamedRoadAlignment> RoadAlignments;
  size_t RimsMissing = 0;
  std::optional<BuildingPageMaterials> Surfaces;

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept;
};

class GroundWorldCandidate {
public:
  struct RestoreBudget {
    size_t Pieces;
    size_t HeightPages;
  };

  struct PublicationMetrics {
    double WorldMs = 0.0;
    double ProductsMs = 0.0;
    double PiecesMs = 0.0;
    double BindingMs = 0.0;
    double RevisionMs = 0.0;
    size_t OrphanStructurePieces = 0;
  };

  GroundWorldCandidate(
      Render::SceneRenderer &renderer,
      const WorldRegionState &world,
      const ::outshine::Generators::Osm::BuildingField &footprints,
      Render::SceneResources::PieceSources pieces = Render::SceneResources::PieceSources::Copy);

  GroundWorldCandidate(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate &operator=(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate(GroundWorldCandidate &&) = delete;
  GroundWorldCandidate &operator=(GroundWorldCandidate &&) = delete;

  [[nodiscard]] GroundBuildProducts &Products() noexcept { return Products_; }

  [[nodiscard]] const GroundBuildProducts &Products() const noexcept { return Products_; }

  [[nodiscard]] const ::outshine::Generators::Osm::RegionSources &Sources() const noexcept {
    return Sources_;
  }

  void RestoreWater(Generators::WaterAsset water) { Sources_.WaterBodies = std::move(water); }

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    const size_t sharedVectors = Sources_.Vectors && Sources_.Vectors.get() == SharedVectors_
                                     ? Sources_.Vectors->HeapBytes()
                                     : 0;
    return Products_.OwnedHeapBytes() + Sources_.HeapBytes() - sharedVectors;
  }

  void Grounding(const Vec3 &albedo) { World_.Grounding(albedo); }

  [[nodiscard]] const Vec3 &GroundAlbedo() { return World_.GroundAlbedo(); }

  [[nodiscard]] const Render::EnvironmentLighting &AmbientStanding() {
    return World_.AmbientStanding();
  }

  void GroundIs(int surface) { World_.GroundIs(surface); }

  void Digests(bool enabled) { World_.Digests(enabled); }

  [[nodiscard]] bool SetGroundGeometry(Geometry geometry, size_t carried, std::string &error) {
    return World_.SetGeometry(std::move(geometry), carried, error);
  }

  [[nodiscard]] bool SetGroundGeometry(Geometry geometry,
                                       size_t carried,
                                       const Material &material,
                                       std::string &error) {
    return World_.SetGeometry(std::move(geometry), carried, material, error);
  }

  [[nodiscard]] std::expected<void, std::string>
  BeginGroundGeometryBuild(Geometry geometry, MaterialInstance groundSurface, Material material) {
    return World_.BeginGeneratedGeometryBuild(std::move(geometry), groundSurface, material);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGroundGeometryBuild(size_t itemsMost) {
    return World_.AdvanceGeometryBuild(itemsMost);
  }

  [[nodiscard]] bool GroundGeometryBuildActive() const noexcept {
    return World_.GeometryBuildActive();
  }

  [[nodiscard]] const PublicationMetrics &Publication() const noexcept {
    return PublicationMetrics_;
  }

  [[nodiscard]] bool
  BeginGroundClasses(std::shared_ptr<const Render::PackedTerrainClassification> packed,
                     std::vector<float> palette,
                     std::string &error,
                     Render::GroundClassUploadMetrics *metrics = nullptr);

  [[nodiscard]] std::expected<bool, std::string>
  AdvanceGroundClasses(size_t bytesMost, Render::GroundClassUploadMetrics *metrics = nullptr) {
    return World_.Renderer().AdvanceGroundClasses(bytesMost, metrics);
  }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Core::RuntimeScene &previous,
                                                         const Ui::Font *font);

  [[nodiscard]] std::expected<bool, std::string> AdvancePreparation(
      const Core::RuntimeScene &previous, const Ui::Font *font, RestoreBudget budget);

  [[nodiscard]] std::expected<void, std::string>
  Publish(WorldRegionState &world,
          ::outshine::Generators::Osm::BuildingField &footprints,
          std::unique_ptr<Core::RuntimeScene> &published,
          const GroundRevision &revision);

private:
  GroundBuildProducts Products_;
  Core::WorldCandidate World_;
  ::outshine::Generators::Osm::RegionSources Sources_;
  const ::outshine::Generators::Osm::OsmField *SharedVectors_ = nullptr;
  Render::SceneResources::PieceSources PieceSources_ = Render::SceneResources::PieceSources::Copy;
  size_t NextPiece_ = 0;
  size_t NextHeightPage_ = 0;
  bool WorldPrepared_ = false;
  bool Prepared_ = false;
  PublicationMetrics PublicationMetrics_;
  static_assert(std::is_nothrow_move_assignable_v<HeightSheets>);
  static_assert(std::is_nothrow_move_assignable_v<::outshine::Generators::Osm::BuildingField>);
  static_assert(std::is_nothrow_move_assignable_v<Render::GeometryPageResidency>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::PositionsM)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Indices)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::ClassPalette)>);
  static_assert(
      std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::TerrainClassificationMap)>);
  static_assert(std::is_nothrow_move_assignable_v<Material>);
  static_assert(std::is_nothrow_move_assignable_v<MaterialInstance>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::StreetGraph)>);
};
}
#endif
