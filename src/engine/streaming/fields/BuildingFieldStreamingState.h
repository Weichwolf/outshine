#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#include "BuildingFieldStreaming.h"
#include "AssetPipeline.h"
#include "BuildingFieldRequest.h"
#include "BuildingSourceDemand.h"
#include "BuildingRoofTile.h"
#include "PreparedBuildingAssets.h"
#include "GeometryPageResidency.h"
#include "Geometry.h"
#include "SourceProductSelection.h"
#include "MaterialRegistration.h"
#include "StoredVertex.h"
#include "ShadowDemand.h"
#include "PreparedTerraceAsset.h"
#include "UrbanPrismPreparation.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
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
    std::string TerraceKey;
    bool TerraceRejected = false;
    bool TerraceQualified = false;
    bool Staged = false;
    bool Qualified = false;
    bool ShadowNeeded = true;
    std::shared_ptr<AssetPipeline::Consumer> Consumer;

    [[nodiscard]] std::string_view SelectedKey() const noexcept {
      return TerraceQualified ? TerraceKey : AssetKey;
    }
  };

  struct Tile {
    struct RoofProduct {
      struct Shadow {
        std::string Key;
        GeometryErrorBound Error;
      };

      BuildingRoofTile::Support Projected;
      std::string Key;
      SourceCoverage Coverage;
      uint64_t Cells;
      size_t Count;
      std::optional<Shadow> Caster = std::nullopt;
    };

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
    uint64_t ShadowCells = 0, ResidentOriginals = 0, RoofOriginalsReplaced = 0;
    uint64_t PublishedShadowCells = 0;
    bool Complete = false;
    std::optional<BuildingRoofTile> RoofUpload = std::nullopt;
    std::optional<RoofProduct> ResidentRoof = std::nullopt;
    bool RoofActive = false;
    bool RoofQualified = false;
    bool RoofCasterActive = false;
  };

  struct OriginalProduct {
    Render::GeometryPageResidency::CellSelection Legacy;
    std::string_view Key;
    const SourceCoverage *Coverage = nullptr;
  };

  struct Metadata {
    std::atomic_bool Done{false};
    BuildingRoofTile::ReadCost RoofRead;
    std::optional<Generators::PreparedBuildingAssets::SourceGroups> Groups;
    std::optional<SourceProductSelection> Selection;
    std::optional<SourceProductSelection::Cut> Cut;
    std::optional<BuildingRoofTile> Roof;
  };

  struct TerraceWork {
    struct DetailLoad {
      std::atomic_bool Done{false};
      std::stop_source Stop;
      std::expected<std::optional<GeometryAsset::Product>, std::string> Result;
    };

    uint32_t Tile;
    uint64_t Revision;
    uint32_t Cell;
    Generators::Osm::TerraceAssetRequest Request;
    std::unique_ptr<Generators::Osm::UrbanPrismPreparation> Job;
    std::shared_ptr<DetailLoad> Detail;
    bool Retiring = false;

    void Cancel() noexcept {
      Job->Cancel();
      if (Detail) { Detail->Stop.request_stop(); }
      Retiring = true;
    }
  };

  struct RemainderParts {
    std::vector<std::vector<StoredVertex>> Vertices;
    std::vector<Render::GeometryPagePartView> Parts;
  };

  struct ProductCut {
    std::vector<std::string_view> Main;
    std::vector<std::string_view> Shadow;
    uint64_t ActiveCells = 0;
    size_t ActiveGroups = 0;
    bool RoofActive = false;
    bool RoofCasterActive = false;
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
  std::optional<TerraceWork> Terrace;
  std::shared_ptr<BuildingSourceRead> TerraceSourceRead;
  std::optional<Render::MaterialRegistration> TerraceMaterial;

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
  void CollectCounts();
  void AdvanceTerrace(Context &context);
  void PollTerrace(Context &context, const Vec3 &eye, TerraceWork &work);
  void RequestTerrace(Context &context, const Vec3 &eye);
  [[nodiscard]] static bool
  PrepareTerraceDetail(Context &context, const Cell &cell, TerraceWork &work);
  [[nodiscard]] bool PublishTerrace(Context &context,
                                    const Tile &tile,
                                    Cell &cell,
                                    TerraceWork &work,
                                    const Generators::Osm::UrbanPrismPreparation::Product &ready);
  [[nodiscard]] static bool
  AllowsTerrace(const Context &context, const Tile &tile, const Cell &cell, const Vec3 &eye);
  [[nodiscard]] bool StageTerrace(Context &context,
                                  const Tile &tile,
                                  Cell &cell,
                                  const Generators::Osm::UrbanPrismPreparation::Product &ready,
                                  const std::string &key,
                                  const std::optional<GeometryAsset::Product> &detail);
  [[nodiscard]] static RemainderParts MeshParts(const GeometryAsset::Product &asset,
                                                const Render::MaterialRegistration &materials);
  [[nodiscard]] static std::expected<Render::MaterialRegistration, std::string>
  RegisterPaint(Render::SceneRenderer &renderer, const std::array<Material, 3> &surfaces);
  [[nodiscard]] static Mat4 ToScene(const TangentFrame &frame, const Mat4 &world);
  bool RequestGroup(Context &context, Tile &tile, Cell &cell, Group &group);
  [[nodiscard]] static std::expected<void, std::string> PrepareCellMetadata(Cell &cell);
  void RecordAbortedPreflight(const Cell &cell);
  [[nodiscard]] static std::expected<void, std::string>
  LoadCellFields(Context &context, Tile &tile, Cell &cell);
  void PollCell(Context &context, Tile &tile, Cell &cell);
  void PollFields(Context &context);
  static void PollRoof(Context &context, Tile &tile);
  [[nodiscard]] bool Select(Context &context, std::string &error);
  [[nodiscard]] bool SelectTile(Context &context, Tile &tile, std::string &error);
  [[nodiscard]] std::vector<const Cell *> EvaluateCells(Context &context, Tile &tile);
  [[nodiscard]] static std::expected<std::vector<OriginalProduct>, std::string>
  BindOriginals(Context &context, const Tile &tile, uint64_t sourceKey);
  [[nodiscard]] static std::optional<ProductCut>
  SelectReplacements(const SourceCoverage &domain,
                     std::span<const OriginalProduct> originals,
                     std::span<const Cell *const> candidates,
                     uint64_t shadowCells,
                     const Tile::RoofProduct *roof,
                     bool roofCaster);
  [[nodiscard]] static const Tile::RoofProduct *EligibleRoof(
      const Context &context, const Tile &tile, std::span<const OriginalProduct> originals);
  [[nodiscard]] static std::expected<SourceCoverage, std::string> OriginalCoverage(const Tile &tile,
                                                                                   uint64_t cells);
  [[nodiscard]] static std::expected<bool, std::string>
  Stage(Context &context, Tile &tile, Cell &cell);
  [[nodiscard]] static bool StageGroup(Context &context,
                                       const Tile &tile,
                                       Group &group,
                                       const BuildingFieldRequest::Product &product,
                                       Geometry geometry,
                                       std::string &error);
  static void Reject(Cell &cell);
  [[nodiscard]] static bool ValidPose(const Context &context, Group &group);
  [[nodiscard]] static Render::CameraBasis WorldCamera(const Context &context);
};
}
#endif
