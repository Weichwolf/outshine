#ifndef OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#define OUTSHINE_ENGINE_STREAMING_FIELDS_BUILDINGFIELDSTREAMINGSTATE_H
#include "BuildingFieldStreaming.h"
#include "BuildingFieldRequest.h"
#include "BuildingSourceDemand.h"
#include "PreparedBuildingAssets.h"
#include "GeometryPageResidency.h"
#include "Geometry.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine {
struct BuildingFieldStreaming::State {
  struct Cell {
    enum class Phase { Waiting, Pending, Ready, Rejected };
    Generators::PreparedBuildingAssets::SourceCell Source;
    Phase Where = Phase::Waiting;
    std::optional<BuildingFieldRequest> Request;
    std::shared_ptr<const BuildingFieldRequest::Product> Product;
    std::optional<ImpostorViewAssets::Description> Metadata;
    std::optional<BuildingFieldRequest::ProjectedSupport> Support;
    std::string AssetKey;
    bool Staged = false;
  };

  struct Tile {
    uint32_t Index = 0;
    uint64_t SourceKey = 0;
    BuildingSourceDemand::Source Source;
    std::vector<Render::GeometryPageResidency::CellSelection> Originals;
    std::vector<Cell> Cells;
    uint64_t Active = 0, SelectedKey = 0, SelectedCoverage = 0;
  };

  struct Metadata {
    std::atomic_bool Done{false};
    std::optional<std::vector<Generators::PreparedBuildingAssets::SourceCell>> Cells;
  };

  BuildingSourceDemand Sources;
  std::vector<Tile> Tiles;
  std::shared_ptr<Metadata> Reading;
  std::optional<Tile> ReadingTile;
  size_t NextSource = 0;
  Counts Statistics;
  std::string LastRejection;

  void Discover(Context &context);
  void PollMetadata();
  void StartMetadata(Context &context);
  void RequestOne(Context &context);
  void PollFields(Context &context);
  [[nodiscard]] bool Select(Context &context, std::string &error);
  [[nodiscard]] static bool Stage(Context &context, Tile &tile, Cell &cell, std::string &error);
  [[nodiscard]] static bool ValidPose(const Context &context, const Cell &cell);
  [[nodiscard]] static Render::CameraBasis WorldCamera(const Context &context);
};
}
#endif
