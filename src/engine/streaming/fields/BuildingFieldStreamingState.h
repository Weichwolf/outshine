#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#include "BuildingFieldStreaming.h"
#include "BuildingFieldRequest.h"
#include "BuildingGeometrySource.h"
#include "GeometryAssetRequest.h"
#include "BuildingSourceDemand.h"
#include "PreparedBuildingAssets.h"
#include "GeometryPageResidency.h"
#include "Geometry.h"
#include "SourceProductSelection.h"
#include "MaterialRegistration.h"
#include "StoredVertex.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace outshine {
struct BuildingFieldStreaming::State {
  enum class Phase { Waiting, Pending, Ready, Rejected };

  struct Part {
    std::vector<StoredVertex> Vertices;
    std::vector<float> Tangents;
    std::vector<uint32_t> Triangles;
    std::vector<float> Colours;
    std::vector<float> Uv1;
    uint32_t MaterialIndex = 0;
    Render::MaterialRegistration Material;
  };

  struct PreparedGeometry {
    Geometry Materials;
    std::vector<Part> Parts;

    [[nodiscard]] size_t HeapBytes() const noexcept {
      size_t bytes =
          sizeof(PreparedGeometry) + Materials.storageBytes() + Parts.capacity() * sizeof(Part);
      for (const auto &part : Parts) {
        bytes += part.Vertices.capacity() * sizeof(StoredVertex) +
                 part.Triangles.capacity() * sizeof(uint32_t) +
                 (part.Tangents.capacity() + part.Colours.capacity() + part.Uv1.capacity()) *
                     sizeof(float);
      }
      return bytes;
    }
  };

  struct Group {
    std::vector<uint32_t> Sources;
    std::optional<LevelOfDetail> GeometryDetail;
    double GeometryErrorM = 0;
    Box Bounds;
    std::optional<SourceCoverage> Coverage;
    Phase Where = Phase::Waiting;
    std::optional<BuildingFieldRequest> Request;
    std::optional<GeometryAssetRequest> GeometryRequest;
    std::shared_ptr<const Generators::BuildingGeometrySource::Recipe> GeometryRecipe;
    std::optional<AssetPipeline::Snapshot> Blocked;
    std::shared_ptr<PreparedGeometry> Native;
    std::optional<ImpostorViewAssets::Description> Metadata;
    std::optional<BuildingFieldRequest::ProjectedSupport> Support;
    std::optional<Part> Surface;
    std::string AssetKey;
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
  };

  struct Tile {
    uint32_t Index = 0;
    uint64_t SourceKey = 0;
    BuildingSourceDemand::Source Source;
    uint64_t SourceCount = 0;
    std::vector<Render::GeometryPageResidency::CellSelection> Originals;
    std::vector<Cell> Cells;
    uint64_t Active = 0, SelectedKey = 0, SelectedCoverage = 0;
  };

  struct Metadata {
    std::atomic_bool Done{false};
    Vec3 Eye;
    ProjectedErrorBudget Projection;
    std::optional<Generators::PreparedBuildingAssets::SourceGroups> Groups;
    std::optional<SourceProductSelection> Selection;
    std::optional<SourceProductSelection::Cut> Cut;
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

  void Discover(Context &context);
  void PollMetadata();
  [[nodiscard]] static bool
  PrepareCellSources(Tile &tile, const Generators::PreparedBuildingAssets::SourceGroups &groups);
  [[nodiscard]] static bool
  PrepareCells(Tile &tile,
               const Generators::PreparedBuildingAssets::SourceGroups &groups,
               const SourceProductSelection &selection,
               const SourceProductSelection::Cut &cut,
               const Vec3 &eye,
               ProjectedErrorBudget projection);
  [[nodiscard]] static bool PrepareAlternatives(Tile &tile, Cell &cell);
  void StartMetadata(Context &context);
  void RequestOne(Context &context);
  bool RequestGroup(Context &context, Tile &tile, Cell &cell, Group &group);
  [[nodiscard]] static bool RequestGeometry(Context &context, Tile &tile, Cell &cell, Group &group);
  [[nodiscard]] static std::expected<bool, std::string> PollGeometry(Context &context,
                                                                     Group &group);
  [[nodiscard]] static std::expected<PreparedGeometry, std::string>
  PrepareGeometry(const GeometryAsset::Product &product, const Vec3 &anchor);
  [[nodiscard]] static std::expected<bool, std::string>
  PollGroup(Context &context, Tile &tile, Cell &cell, Group &group);
  void PollCell(Context &context, Tile &tile, Cell &cell);
  void PollFields(Context &context);
  [[nodiscard]] bool Select(Context &context, std::string &error);
  [[nodiscard]] static bool Stage(Context &context, Tile &tile, Cell &cell, std::string &error);
  [[nodiscard]] static bool StageProduct(
      Context &context, const Tile &tile, const Cell &cell, Group &group, std::string &error);
  [[nodiscard]] static bool StageGroup(Context &context,
                                       const Tile &tile,
                                       Group &group,
                                       const BuildingFieldRequest::Product &product,
                                       std::string &error);
  static void Reject(Cell &cell);
  [[nodiscard]] static bool ValidPose(const Context &context, const Group &group);
  [[nodiscard]] static Render::CameraBasis WorldCamera(const Context &context);
};
}
#endif
