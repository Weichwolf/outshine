#ifndef OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H
#define OUTSHINE_ENGINE_GROUNDWORLDCANDIDATE_H

#include "EngineHeld.h"
#include "WorldCandidate.h"
#include "ClassStructure.h"
#include <chrono>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine {
struct GroundBuildProducts {
  HeightSheets Sheets;
  Ground::BuildingField Footprints;
  TilePieces Pieces;
  Geometry Ground;
  std::vector<float> PositionsM;
  std::vector<uint32_t> Indices;
  std::vector<float> ClassPalette;
  std::shared_ptr<const ClassStructure> ClassStructure;
  Material GroundMaterial;
  MaterialInstance GroundSurface;
  std::shared_ptr<const Path::Network> StreetGraph;
  size_t StreetGraphWayCount = 0;
  std::vector<NamedRoadAlignment> RoadAlignments;
  size_t RimsMissing = 0;
  std::optional<TilePieces::Surfaces> Surfaces;

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    size_t alignmentBytes = RoadAlignments.capacity() * sizeof(NamedRoadAlignment);
    for (const NamedRoadAlignment &route : RoadAlignments) {
      alignmentBytes += route.Id.capacity();
      if (route.Alignment) {
        alignmentBytes += sizeof(Generators::RoadAlignment) + route.Alignment->OwnedHeapBytes();
      }
      if (route.Surface) {
        alignmentBytes += sizeof(Generators::RoadSurface) + route.Surface->OwnedHeapBytes();
      }
    }
    return Sheets.HeapBytes() + Footprints.HeapBytes() + Pieces.HeapBytes() + alignmentBytes +
           Ground.storageBytes() + PositionsM.capacity() * sizeof(float) +
           Indices.capacity() * sizeof(uint32_t) + ClassPalette.capacity() * sizeof(float);
  }
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
      const Surrounds &world,
      const Ground::BuildingField &footprints,
      Render::SceneResources::PieceSources pieces = Render::SceneResources::PieceSources::Copy)
      : Products_{.Sheets = world.Sheets,
                  .Footprints = footprints.SnapshotAccepted(),
                  .Pieces = world.Pieces,
                  .Ground = {},
                  .PositionsM = {},
                  .Indices = {},
                  .ClassPalette = {},
                  .ClassStructure = {},
                  .GroundMaterial = {},
                  .GroundSurface = {},
                  .StreetGraph = world.StreetGraph,
                  .StreetGraphWayCount = world.StreetGraphWayCount,
                  .RoadAlignments = {},
                  .RimsMissing = world.RimsMissing,
                  .Surfaces = world.StructureSurfaces},
        World_(renderer),
        Sources_(Ground::RegionSources::Snapshot(
            world.Stack.Vectors(), world.Stack.Ways(), world.Stack.WaterBodies())),
        PieceSources_(pieces) {}

  GroundWorldCandidate(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate &operator=(const GroundWorldCandidate &) = delete;
  GroundWorldCandidate(GroundWorldCandidate &&) = delete;
  GroundWorldCandidate &operator=(GroundWorldCandidate &&) = delete;

  [[nodiscard]] GroundBuildProducts &Products() noexcept { return Products_; }

  [[nodiscard]] const GroundBuildProducts &Products() const noexcept { return Products_; }

  [[nodiscard]] const Ground::RegionSources &Sources() const noexcept { return Sources_; }

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    return Products_.OwnedHeapBytes() + Sources_.HeapBytes();
  }

  void Grounding(const Vec3 &albedo) { World_.Grounding(albedo); }

  [[nodiscard]] const Render::SubjectEnvironment &AmbientStanding() {
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

  [[nodiscard]] bool BeginGroundClasses(std::shared_ptr<const ClassStructure> structure,
                                        std::vector<float> palette,
                                        std::string &error,
                                        Render::GroundClassUploadMetrics *metrics = nullptr) {
    if (!structure) {
      error = "ground classification has no structure";
      return false;
    }
    const auto prepareAt = std::chrono::steady_clock::now();
    const auto retainedPalette = std::make_shared<const std::vector<float>>(std::move(palette));
    Render::GroundClassificationSource source{.Classes = {structure, structure->Words()},
                                              .ClassWords = structure->Bytes() / sizeof(uint32_t),
                                              .Palette = {retainedPalette, retainedPalette->data()},
                                              .PaletteFloats = retainedPalette->size()};
    const double preparationMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - prepareAt)
            .count();
    const bool accepted = World_.Renderer().BeginGroundClasses(std::move(source), error, metrics);
    if (metrics != nullptr) { metrics->SourcePreparationMs = preparationMs; }
    return accepted;
  }

  [[nodiscard]] std::expected<bool, std::string>
  AdvanceGroundClasses(size_t bytesMost, Render::GroundClassUploadMetrics *metrics = nullptr) {
    return World_.Renderer().AdvanceGroundClasses(bytesMost, metrics);
  }

  [[nodiscard]] std::expected<void, std::string> Prepare(const Core::RuntimeScene &previous,
                                                         const Ui::Font *font) {
    for (;;) {
      auto prepared = AdvancePreparation(previous,
                                         font,
                                         {.Pieces = std::numeric_limits<size_t>::max(),
                                          .HeightPages = std::numeric_limits<size_t>::max()});
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      if (*prepared) { return {}; }
    }
  }

  [[nodiscard]] std::expected<bool, std::string> AdvancePreparation(
      const Core::RuntimeScene &previous, const Ui::Font *font, RestoreBudget budget) {
    if (Prepared_) { return true; }
    if (!WorldPrepared_) {
      if (PieceSources_ == Render::SceneResources::PieceSources::Copy) {
        Products_.Pieces.Into(&World_.Renderer());
        std::string sourceError;
        if (!Products_.Pieces.ValidateSources(sourceError)) {
          return std::unexpected("candidate snapshot before preparation: " + sourceError);
        }
      }
      auto prepared = World_.Prepare(previous,
                                     font,
                                     PieceSources_,
                                     Core::SubjectGeometrySources::Driven,
                                     Core::ResourceRestoreMode::Deferred);
      if (!prepared) { return std::unexpected(std::move(prepared.error())); }
      WorldPrepared_ = true;
      return false;
    }
    auto pieces = World_.AdvancePieceResourceRestore(NextPiece_, budget.Pieces);
    if (!pieces) { return std::unexpected(std::move(pieces.error())); }
    if (!*pieces) { return false; }
    auto ground = World_.AdvanceGroundResourceRestore(NextHeightPage_, budget.HeightPages);
    if (!ground) { return std::unexpected(std::move(ground.error())); }
    if (!*ground) { return false; }
    Products_.Sheets.Into(&World_.Renderer());
    Products_.Pieces.Into(&World_.Renderer());
    if (PieceSources_ == Render::SceneResources::PieceSources::Omit) {
      Products_.Pieces.Clear();
      Products_.Pieces.Into(&World_.Renderer());
    }
    Prepared_ = true;
    return true;
  }

  [[nodiscard]] std::expected<void, std::string>
  Publish(Surrounds &world,
          Ground::BuildingField &footprints,
          std::unique_ptr<Core::RuntimeScene> &published,
          const GroundRevision &revision) {
    if (!world.GroundPublished.CanPublish()) {
      return std::unexpected("a capture holds the published ground");
    }
    PublicationMetrics_ = {};
    const auto reconciled = World_.Renderer().ReconcileStructurePieces(Products_.Pieces.Handles());
    if (!reconciled) { return std::unexpected(reconciled.error()); }
    PublicationMetrics_.OrphanStructurePieces = *reconciled;
    auto region = std::make_shared<const Ground::PublishedRegion>(
        std::move(Sources_), Products_.Footprints.SnapshotAccepted());
    auto phaseAt = std::chrono::steady_clock::now();
    if (auto publishedWorld = World_.Publish(published); !publishedWorld) { return publishedWorld; }
    PublicationMetrics_.WorldMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    phaseAt = std::chrono::steady_clock::now();
    footprints = std::move(Products_.Footprints);
    world.Sheets = std::move(Products_.Sheets);
    world.GroundPositionsM = std::move(Products_.PositionsM);
    world.GroundIndex = std::move(Products_.Indices);
    world.StreetGraph = std::move(Products_.StreetGraph);
    world.Region = std::move(region);
    world.StreetGraphWayCount = Products_.StreetGraphWayCount;
    world.RoadAlignments = std::move(Products_.RoadAlignments);
    world.RimsMissing = Products_.RimsMissing;
    PublicationMetrics_.ProductsMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    phaseAt = std::chrono::steady_clock::now();
    world.Pieces = std::move(Products_.Pieces);
    if (Products_.Surfaces) { world.Pieces.Wears(*Products_.Surfaces); }
    world.StructureSurfaces = Products_.Surfaces;
    PublicationMetrics_.PiecesMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    phaseAt = std::chrono::steady_clock::now();
    world.BindSceneResources(World_.Renderer());
    PublicationMetrics_.BindingMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    phaseAt = std::chrono::steady_clock::now();
    if (!world.GroundPublished.Publish(revision)) {
      return std::unexpected("a capture holds the published ground");
    }
    ++world.Relaid;
    PublicationMetrics_.RevisionMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    return {};
  }

private:
  GroundBuildProducts Products_;
  Core::WorldCandidate World_;
  Ground::RegionSources Sources_;
  Render::SceneResources::PieceSources PieceSources_ = Render::SceneResources::PieceSources::Copy;
  size_t NextPiece_ = 0;
  size_t NextHeightPage_ = 0;
  bool WorldPrepared_ = false;
  bool Prepared_ = false;
  PublicationMetrics PublicationMetrics_;
  static_assert(std::is_nothrow_move_assignable_v<HeightSheets>);
  static_assert(std::is_nothrow_move_assignable_v<Ground::BuildingField>);
  static_assert(std::is_nothrow_move_assignable_v<TilePieces>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::PositionsM)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::Indices)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::ClassPalette)>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::ClassStructure)>);
  static_assert(std::is_nothrow_move_assignable_v<Material>);
  static_assert(std::is_nothrow_move_assignable_v<MaterialInstance>);
  static_assert(std::is_nothrow_move_assignable_v<decltype(GroundBuildProducts::StreetGraph)>);
};
}
#endif
