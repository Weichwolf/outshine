#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#include "BuildingFieldStreaming.h"
#include "BuildingFieldRequest.h"
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

namespace outshine {
struct BuildingFieldStreaming::State {
  enum class Phase { Waiting, Pending, Ready, Rejected };

  struct Part {
    std::vector<StoredVertex> Vertices;
    std::vector<float> Tangents;
    std::vector<uint32_t> Triangles;
    Render::MaterialRegistration Material;
  };

  struct Group {
    std::optional<SourceProductSelection::Product> Selected;
    Box Bounds;
    Phase Where = Phase::Waiting;
    std::optional<BuildingFieldRequest> Request;
    std::optional<ImpostorViewAssets::Description> Metadata;
    std::optional<BuildingFieldRequest::ProjectedSupport> Support;
    std::optional<Part> Surface;
    std::string AssetKey;
  };

  struct Cell {
    Generators::PreparedBuildingAssets::SourceCell Source;
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
    std::vector<Render::GeometryPageResidency::CellSelection> Originals;
    std::vector<Cell> Cells;
    std::optional<SourceProductSelection> Selection;
    std::optional<SourceProductSelection::Cut> Cut;
    uint64_t Active = 0, SelectedKey = 0, SelectedCoverage = 0;
  };

  struct Metadata {
    std::atomic_bool Done{false};
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
  static void PrepareCells(Tile &tile,
                           const Generators::PreparedBuildingAssets::SourceGroups &groups);
  void StartMetadata(Context &context);
  void RequestOne(Context &context);
  bool RequestGroup(Context &context, Tile &tile, Cell &cell, Group &group);
  void PollCell(Context &context, Tile &tile, Cell &cell);
  void PollFields(Context &context);
  [[nodiscard]] bool Select(Context &context, std::string &error);
  [[nodiscard]] static bool Stage(Context &context, Tile &tile, Cell &cell, std::string &error);
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
