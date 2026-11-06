#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#include "BuildingFieldStreaming.h"
#include "AssetPipeline.h"
#include "BuildingFieldRequest.h"
#include "BuildingSourceDemand.h"
#include "PreparedBuildingAssets.h"
#include "GeometryPageResidency.h"
#include "Geometry.h"
#include "SourceProductSelection.h"
#include "MaterialRegistration.h"
#include "StoredVertex.h"
#include "ShadowDemand.h"
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine {
struct BuildingFieldStreaming::State {
  enum class Phase { Waiting, Pending, Prepared, Ready, Rejected };

  struct Part {
    std::vector<StoredVertex> Vertices;
    std::vector<float> Tangents;
    std::vector<uint32_t> Triangles;
    Render::MaterialRegistration Material;
  };

  struct Group {
    std::vector<uint32_t> Sources;
    Box Bounds;
    std::optional<SourceCoverage> Coverage;
    Phase Where = Phase::Waiting;
    std::optional<BuildingFieldRequest> Request;
    std::optional<ImpostorViewAssets::Description> Metadata;
    std::optional<BuildingFieldRequest::ProjectedSupport> Support;
    std::optional<Part> Surface;
    std::string AssetKey;
    std::optional<Box> SceneBounds;
  };

  struct Cell {
    Cell(Generators::PreparedBuildingAssets::SourceCell source, SourceCoverage coverage)
        : Source(source), Coverage(std::move(coverage)) {}

    Generators::PreparedBuildingAssets::SourceCell Source;
    SourceCoverage Coverage;
    Phase Where = Phase::Waiting;
    std::vector<Group> Groups;
    std::vector<Group> Alternatives;
    std::string AssetKey;
    bool Staged = false;
    bool Qualified = false;
    bool ShadowNeeded = true;
    std::shared_ptr<AssetPipeline::Consumer> Consumer;
  };

  struct Tile {
    uint32_t Index = 0;
    uint64_t SourceKey = 0;
    BuildingSourceDemand::Source Source;
    std::vector<Render::GeometryPageResidency::CellSelection> Originals;
    std::vector<Cell> Cells;
    std::optional<SourceCoverage> Coverage;
    std::vector<std::string> MainProducts, ShadowProducts;
    std::vector<std::string> CandidateProducts;
    std::vector<std::string> OriginalProducts;
    size_t ActiveGroups = 0;
    uint64_t Active = 0;
    uint64_t ShadowCells = 0, ResidentOriginals = 0;
    uint64_t PublishedShadowCells = 0;
    bool Complete = false;
  };

  struct OriginalProduct {
    Render::GeometryPageResidency::CellSelection Legacy;
    std::string_view Key;
    const SourceCoverage *Coverage = nullptr;
  };

  struct Metadata {
    std::atomic_bool Done{false};
    std::optional<Generators::PreparedBuildingAssets::SourceGroups> Groups;
    std::optional<SourceProductSelection> Selection;
    std::optional<SourceProductSelection::Cut> Cut;
  };

  struct ProductCut {
    std::vector<std::string_view> Main;
    std::vector<std::string_view> Shadow;
    uint64_t ActiveCells = 0;
    size_t ActiveGroups = 0;
    std::vector<const SourceCoverage *> MainSources, ShadowSources;
  };

  BuildingSourceDemand Sources;
  std::vector<Tile> Tiles;
  std::shared_ptr<Metadata> Reading;
  std::optional<Tile> ReadingTile;
  size_t NextSource = 0;
  std::shared_ptr<BuildingSourceRead> SourceRead;
  uint32_t ReadCell = 0;
  Counts Statistics;
  std::string LastRejection;
  std::optional<Render::ShadowDemand> Shadows;

  void Discover(Context &context);
  void PollMetadata();
  [[nodiscard]] static bool
  PrepareCellSources(Tile &tile, const Generators::PreparedBuildingAssets::SourceGroups &groups);
  [[nodiscard]] static bool
  PrepareCells(Tile &tile,
               const Generators::PreparedBuildingAssets::SourceGroups &groups,
               const SourceProductSelection &selection,
               const SourceProductSelection::Cut &cut);
  void StartMetadata(Context &context);
  void RequestOne(Context &context);
  bool RequestGroup(Context &context, Tile &tile, Cell &cell, Group &group);
  [[nodiscard]] static std::expected<void, std::string> PrepareCellMetadata(Cell &cell);
  void RecordAbortedPreflight(const Cell &cell);
  [[nodiscard]] static std::expected<void, std::string>
  LoadCellFields(Context &context, Tile &tile, Cell &cell);
  void PollCell(Context &context, Tile &tile, Cell &cell);
  void PollFields(Context &context);
  [[nodiscard]] bool Select(Context &context, std::string &error);
  [[nodiscard]] bool SelectTile(Context &context, Tile &tile, std::string &error);
  [[nodiscard]] std::vector<const Cell *> EvaluateCells(Context &context, Tile &tile);
  [[nodiscard]] static std::expected<std::vector<OriginalProduct>, std::string>
  BindOriginals(Context &context, const Tile &tile, uint64_t sourceKey);
  [[nodiscard]] static ProductCut SelectReplacements(std::span<const OriginalProduct> originals,
                                                     std::span<const Cell *const> candidates,
                                                     uint64_t shadowCells);
  [[nodiscard]] static std::expected<SourceCoverage, std::string> OriginalCoverage(const Tile &tile,
                                                                                   uint64_t cells);
  [[nodiscard]] static bool Stage(Context &context, Tile &tile, Cell &cell, std::string &error);
  [[nodiscard]] static bool StageGroup(Context &context,
                                       const Tile &tile,
                                       Group &group,
                                       const BuildingFieldRequest::Product &product,
                                       std::string &error);
  static void Reject(Cell &cell);
  [[nodiscard]] static bool ValidPose(const Context &context, Group &group);
  [[nodiscard]] static Render::CameraBasis WorldCamera(const Context &context);
};
}
#endif
