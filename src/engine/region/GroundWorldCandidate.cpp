#include "GroundWorldCandidate.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
[[nodiscard]] size_t GroundBuildProducts::OwnedHeapBytes() const noexcept {
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

GroundWorldCandidate::GroundWorldCandidate(
    Render::SceneRenderer &renderer,
    const WorldRegionState &world,
    const ::outshine::Generators::Osm::BuildingField &footprints,
    Render::SceneResources::PieceSources pieces)
    : Products_{.Sheets = world.Sheets,
                .Footprints = footprints.SnapshotAccepted(),
                .Pieces = world.Pieces,
                .Ground = {},
                .PositionsM = {},
                .Indices = {},
                .ClassPalette = {},
                .TerrainClassificationMap = {},
                .ClassUpload = {},
                .GroundMaterial = {},
                .GroundSurface = {},
                .StreetGraph = world.StreetGraph,
                .StreetGraphWayCount = world.StreetGraphWayCount,
                .RoadAlignments = {},
                .RimsMissing = world.RimsMissing,
                .Surfaces = world.StructureSurfaces},
      World_(renderer),
      Sources_(::outshine::Generators::Osm::RegionSources::Snapshot(
          world.Stack.Vectors(),
          world.Stack.Ways(),
          world.Stack.WaterBodies(),
          world.Region ? world.Region->VectorSnapshot() : nullptr)),
      SharedVectors_(world.Region ? world.Region->Vectors() : nullptr),
      PieceSources_(pieces) {
  Products_.Sheets.ForgetsFields();
}

[[nodiscard]] bool GroundWorldCandidate::BeginGroundClasses(
    std::shared_ptr<const Render::PackedTerrainClassification> packed,
    std::vector<float> palette,
    std::string &error,
    Render::GroundClassUploadMetrics *metrics) {
  if (!packed) {
    error = "ground classification has no upload product";
    return false;
  }
  const auto prepareAt = std::chrono::steady_clock::now();
  const auto retainedPalette = std::make_shared<const std::vector<float>>(std::move(palette));
  Render::GroundClassificationSource source{.Classes = {packed, packed->Words().data()},
                                            .ClassWords = packed->Words().size(),
                                            .Palette = {retainedPalette, retainedPalette->data()},
                                            .PaletteFloats = retainedPalette->size()};
  const double preparationMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - prepareAt)
          .count();
  const bool accepted = World_.Renderer().BeginGroundClasses(std::move(source), error, metrics);
  if (metrics != nullptr) { metrics->SourcePreparationMs = preparationMs; }
  return accepted;
}

[[nodiscard]] std::expected<void, std::string>
GroundWorldCandidate::Prepare(const Core::RuntimeScene &previous, const Ui::Font *font) {
  for (;;) {
    auto prepared = AdvancePreparation(previous,
                                       font,
                                       {.Pieces = std::numeric_limits<size_t>::max(),
                                        .HeightPages = std::numeric_limits<size_t>::max()});
    if (!prepared) { return std::unexpected(std::move(prepared.error())); }
    if (*prepared) { return {}; }
  }
}

[[nodiscard]] std::expected<bool, std::string> GroundWorldCandidate::AdvancePreparation(
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
GroundWorldCandidate::Publish(WorldRegionState &world,
                              ::outshine::Generators::Osm::BuildingField &footprints,
                              std::unique_ptr<Core::RuntimeScene> &published,
                              const GroundRevision &revision) {
  if (!world.GroundPublished.CanPublish()) {
    return std::unexpected("a capture holds the published ground");
  }
  PublicationMetrics_ = {};
  const auto reconciled = World_.Renderer().ReconcileStructurePieces(Products_.Pieces.Handles());
  if (!reconciled) { return std::unexpected(reconciled.error()); }
  PublicationMetrics_.OrphanStructurePieces = *reconciled;
  auto region = std::make_shared<const ::outshine::Generators::Osm::PublishedRegion>(
      std::move(Sources_), Products_.Footprints.SnapshotAccepted());
  auto phaseAt = std::chrono::steady_clock::now();
  if (auto publishedWorld = World_.Publish(published); !publishedWorld) { return publishedWorld; }
  PublicationMetrics_.WorldMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
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
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  phaseAt = std::chrono::steady_clock::now();
  world.Pieces = std::move(Products_.Pieces);
  if (Products_.Surfaces) { world.Pieces.SetSurfaces(*Products_.Surfaces); }
  world.StructureSurfaces = Products_.Surfaces;
  PublicationMetrics_.PiecesMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  phaseAt = std::chrono::steady_clock::now();
  world.BindSceneResources(World_.Renderer());
  PublicationMetrics_.BindingMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  phaseAt = std::chrono::steady_clock::now();
  if (!world.GroundPublished.Publish(revision)) {
    return std::unexpected("a capture holds the published ground");
  }
  ++world.Relaid;
  PublicationMetrics_.RevisionMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  return {};
}
}
