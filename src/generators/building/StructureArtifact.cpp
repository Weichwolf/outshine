#include "StructureArtifact.h"
#include "StructureBinary.h"
#include "StructurePositionPalette.h"
#include "BuildingFootprintFields.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <limits>
#include <type_traits>
#include <utility>

namespace outshine::Generators {
namespace {
static_assert(sizeof(size_t) == sizeof(uint64_t));
constexpr uint32_t kPreviousVersion = 10;
constexpr uint32_t kCeilingVersion = 11;
constexpr uint32_t kPartVersion = 12;
constexpr uint32_t kDirectionalVersion = 14;
constexpr uint32_t kPositionVersion = 15;
constexpr uint32_t kPackedNormalMaximum = 0x00ffffffu;
constexpr size_t kHashBytes = 64;
constexpr size_t kMostBytes = kStructureArtifactBytesMost;
constexpr std::string_view kMagic = "outshine-structure";

using StructureBinary::Reader;
using StructureBinary::Writer;

constexpr auto scalar = [](auto &archive, auto &value) { return archive.Number(value); };
constexpr auto bounds = [](auto &archive, auto &value) {
  return archive.Number(value.MinLonDeg) && archive.Number(value.MinLatDeg) &&
         archive.Number(value.MaxLonDeg) && archive.Number(value.MaxLatDeg);
};
constexpr auto vertex = [](auto &archive, auto &value) {
  for (size_t i = 0; i < 3; ++i) {
    if (!archive.Number(value.pos[i])) { return false; }
  }
  return archive.Number(value.texture[0]) && archive.Number(value.texture[1]) &&
         archive.Number(value.normWord);
};
constexpr auto vertexLists = [](auto &archive, auto &geometry) {
  return archive.List(geometry.WallCorners, vertex) && archive.List(geometry.RoofCorners, vertex);
};
constexpr auto cluster = [](auto &archive, auto &value, bool directions) {
  for (size_t i = 0; i < 3; ++i) {
    if (!archive.Number(value.SelfCenter[i]) || !archive.Number(value.ParentCenter[i])) {
      return false;
    }
  }
  return archive.Number(value.SelfRadius) && archive.Number(value.ParentRadius) &&
         archive.Number(value.SelfErr) && archive.Number(value.ParentErr) &&
         archive.Number(value.First) && archive.Number(value.Count) &&
         archive.Number(value.Level) &&
         (!directions || (archive.Number(value.NormalAxis) && archive.Number(value.NormalCosine)));
};

constexpr auto surface = [](auto &archive, auto &value) {
  return archive.Number(value.Upper.ReferenceToVariantM) &&
         archive.Number(value.Upper.VariantToReferenceM) &&
         archive.Number(value.ReferenceToVariantLowerM) &&
         archive.Number(value.VariantToReferenceLowerM);
};

template <typename Archive, typename View> bool SelectionView(Archive &archive, View &view) {
  const auto point = [](auto &held, auto &value) {
    return held.Number(value[0]) && held.Number(value[1]) && held.Number(value[2]);
  };
  return archive.Number(view.Eye.LongitudeDeg) && archive.Number(view.Eye.LatitudeDeg) &&
         archive.Maybe(view.EyeEcef, point) && archive.Number(view.Projection.FocalPx) &&
         archive.Number(view.Projection.AllowedErrorPx) && archive.Number(view.EyeRadiusM);
}

bool OriginalIndices(Writer &archive, const BakedTile &tile) {
  const auto write = [&](const std::vector<uint32_t> &indices, const ClusteredMesh &mesh) {
    if (mesh.HasDrawOrder(indices.size()) && ValidClusterRanges(mesh.Clusters, mesh.Index.size())) {
      return archive.Number(uint64_t{0});
    }
    return archive.List(indices, scalar);
  };
  return write(tile.Built.WallRun, tile.Walls) && write(tile.Built.RoofRun, tile.Roofs);
}

bool OriginalIndices(Reader &archive, BakedTile &tile) {
  return archive.List(tile.Built.WallRun, scalar) && archive.List(tile.Built.RoofRun, scalar);
}

template <typename Archive, typename Tile>
bool Product(Archive &archive, Tile &tile, uint32_t version) {
  const auto writeCluster = [version](auto &held, auto &value) {
    return cluster(held, value, version >= kDirectionalVersion);
  };
  if (!OriginalIndices(archive, tile) || !archive.List(tile.Walls.Clusters, writeCluster) ||
      !archive.List(tile.Built.WallColours, scalar) ||
      !archive.List(tile.Built.WallMaterialM, scalar) || !archive.List(tile.Walls.Index, scalar) ||
      !archive.List(tile.Roofs.Clusters, writeCluster) || !archive.List(tile.Roofs.Index, scalar) ||
      !archive.Number(tile.Digest) || !archive.Number(tile.FallbackHeights) ||
      !archive.Maybe(tile.SelectedView,
                     [](auto &held, auto &view) { return SelectionView(held, view); }) ||
      !archive.Maybe(tile.RequestedDetail, scalar) || !archive.Maybe(tile.RequestedCell, scalar) ||
      !archive.Maybe(tile.FootprintBounds, bounds) || !archive.Number(tile.OccupiedCells)) {
    return false;
  }
  for (auto &cell : tile.CellBounds) {
    if (!bounds(archive, cell)) { return false; }
  }
  for (auto &height : tile.CellMaxHeightM) {
    if (!archive.Number(height)) { return false; }
  }
  for (auto &error : tile.CellShellErrorM) {
    if (!archive.Number(error)) { return false; }
  }
  return archive.List(tile.Prints,
                      [version](auto &held, auto &print) {
                        return StructureBinary::FootprintFields(
                            held, print, version >= kCeilingVersion, version >= kPartVersion);
                      }) &&
         archive.List(tile.FootprintDetails, scalar) && archive.List(tile.SeatSpreadM, scalar) &&
         archive.List(tile.AcrossM, scalar) && archive.Number(tile.OsmHeights) &&
         archive.Number(tile.DefaultHeights) && archive.Number(tile.Fronted) &&
         archive.Number(tile.Lumped) && archive.Number(tile.Blocks) &&
         archive.Number(tile.NoGround) && archive.Number(tile.UnsupportedMeshes) &&
         archive.Number(tile.SkippedRings) && archive.Maybe(tile.SurfaceError, surface) &&
         archive.Maybe(tile.SurfaceFailure, scalar);
}

bool MeshValid(const std::vector<StoredVertex> &vertices,
               const std::vector<uint32_t> &indices,
               const ClusteredMesh &mesh) {
  const auto validIndex = [&](uint32_t index) { return index < vertices.size(); };
  if (indices.size() % 3 != 0 || mesh.Index.size() % 3 != 0 ||
      !std::ranges::all_of(indices, validIndex) || !std::ranges::all_of(mesh.Index, validIndex)) {
    return false;
  }
  if (indices.empty() && !mesh.Index.empty() &&
      !ValidClusterRanges(mesh.Clusters, mesh.Index.size())) {
    return false;
  }
  return std::ranges::all_of(mesh.Clusters, [&](const DagCluster &item) {
    return item.First <= mesh.Index.size() && item.Count <= mesh.Index.size() - item.First &&
           item.Count % 3 == 0 && item.SelfRadius >= 0 && item.ParentRadius >= 0 &&
           item.SelfErr >= 0 && item.ParentErr >= 0 && item.NormalAxis <= kPackedNormalMaximum &&
           item.NormalCosine >= 0 && item.NormalCosine <= 1;
  });
}

bool Valid(const BakedTile &tile) {
  const auto detail = [](LevelOfDetail value) { return value <= LevelOfDetail::Skyline; };
  const auto shellError = [](double value) {
    return std::isfinite(value) && (value >= 0.0 || value == -1.0);
  };
  if ((tile.SelectedView && !tile.SelectedView->Contains(tile.SelectedView->Eye,
                                                         tile.SelectedView->Projection,
                                                         tile.SelectedView->EyeEcef)) ||
      (tile.RequestedDetail && !detail(*tile.RequestedDetail)) ||
      (tile.RequestedCell &&
       (*tile.RequestedCell == 0 || *tile.RequestedCell > kStructureCellsPerTile)) ||
      tile.Prints.size() != tile.FootprintDetails.size() ||
      tile.Prints.size() != tile.SeatSpreadM.size() || tile.Prints.size() != tile.AcrossM.size() ||
      !std::ranges::all_of(tile.FootprintDetails, detail) ||
      !std::ranges::all_of(tile.CellShellErrorM, shellError) || tile.OsmHeights < 0 ||
      tile.DefaultHeights < 0 || tile.Fronted < 0 || tile.Lumped < 0 || tile.Blocks < 0 ||
      tile.NoGround < 0 ||
      (tile.SurfaceFailure && (*tile.SurfaceFailure < StructureSurfaceErrorFailure::InvalidSource ||
                               *tile.SurfaceFailure > StructureSurfaceErrorFailure::Cancelled))) {
    return false;
  }
  if (!std::ranges::all_of(tile.Prints, [](const ::outshine::Ground::BuildingFootprint &item) {
        return item.TerrainContacts.Valid() &&
               (!item.TerrainContacts.Parts || item.TerrainCeiling) &&
               (!item.TerrainCeiling ||
                (item.MinimumHeightM == 0.0 && std::isfinite(item.TerrainCeiling->HeightAslM))) &&
               item.Source <= ::outshine::Ground::BuildingHeightSource::Generated &&
               std::isfinite(item.MinimumHeightM) && std::isfinite(item.HeightM) &&
               item.MinimumHeightM < item.HeightM && item.PointCount >= 3 &&
               item.PointCount <= std::numeric_limits<uint32_t>::max() - item.FirstPoint;
      })) {
    return false;
  }
  if (tile.SurfaceError) {
    const auto &error = *tile.SurfaceError;
    if (error.ReferenceToVariantLowerM < 0 || error.VariantToReferenceLowerM < 0 ||
        error.Upper.ReferenceToVariantM < error.ReferenceToVariantLowerM ||
        error.Upper.VariantToReferenceM < error.VariantToReferenceLowerM) {
      return false;
    }
  }
  const bool colourValid =
      tile.Built.WallColours.empty() ||
      (tile.Built.WallColours.size() == tile.Built.WallCorners.size() * 4 &&
       std::ranges::all_of(tile.Built.WallColours, [](float c) { return std::isfinite(c); }));
  const bool materialValid =
      tile.Built.WallMaterialM.empty() ||
      (tile.Built.WallMaterialM.size() == tile.Built.WallCorners.size() * 2 &&
       std::ranges::all_of(tile.Built.WallMaterialM,
                           [](float value) { return std::isfinite(value); }));
  return colourValid && materialValid &&
         MeshValid(tile.Built.WallCorners, tile.Built.WallRun, tile.Walls) &&
         MeshValid(tile.Built.RoofCorners, tile.Built.RoofRun, tile.Roofs);
}

bool KeyValid(std::string_view key) {
  return key.size() == kHashBytes && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

}

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodeStructureArtifact(const BakedTile &tile, std::string_view inputKey) {
  if (!KeyValid(inputKey)) { return std::unexpected(StructureArtifactError::InvalidKey); }
  if (!Valid(tile)) { return std::unexpected(StructureArtifactError::InvalidProduct); }
  const auto palette = tile.RequestedDetail == LevelOfDetail::Shell
                           ? StructureBinary::PositionPalette::Make(tile.Built)
                           : std::nullopt;
  const auto version = palette ? kPositionVersion : kDirectionalVersion;
  Writer writer;
  writer.Bytes.insert(writer.Bytes.end(), kMagic.begin(), kMagic.end());
  if (!writer.Number(version)) { return std::unexpected(writer.Failure); }
  writer.Bytes.insert(writer.Bytes.end(), inputKey.begin(), inputKey.end());
  if (!(palette ? palette->Write(writer, tile.Built) : vertexLists(writer, tile.Built)) ||
      !Product(writer, tile, version) || writer.Bytes.size() > kMostBytes - kHashBytes) {
    return std::unexpected(writer.Failure);
  }
  const auto checksum = Sha256Hex(writer.Bytes.data(), writer.Bytes.size());
  writer.Bytes.insert(writer.Bytes.end(), checksum.begin(), checksum.end());
  return std::move(writer.Bytes);
}

std::optional<BakedTile> DecodeStructureArtifact(std::span<const uint8_t> bytes,
                                                 std::string_view inputKey,
                                                 uint64_t currentSourceKey) {
  const size_t header = kMagic.size() + sizeof(uint32_t) + kHashBytes;
  if (!KeyValid(inputKey) || bytes.size() > kMostBytes || bytes.size() < header + kHashBytes ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    return std::nullopt;
  }
  const auto payload = bytes.first(bytes.size() - kHashBytes);
  const auto checksum = Sha256Hex(payload.data(), payload.size());
  if (!std::equal(checksum.begin(), checksum.end(), bytes.end() - kHashBytes)) {
    return std::nullopt;
  }
  Reader reader(payload.subspan(kMagic.size()));
  uint32_t version = 0;
  if (!reader.Number(version) || version < kPreviousVersion || version > kPositionVersion ||
      !std::equal(inputKey.begin(), inputKey.end(), reader.Bytes.begin())) {
    return std::nullopt;
  }
  reader.Bytes = reader.Bytes.subspan(kHashBytes);
  BakedTile tile;
  const bool positions = version == kPositionVersion
                             ? StructureBinary::PositionPalette::Read(reader, tile.Built)
                             : vertexLists(reader, tile.Built);
  if (!positions || !Product(reader, tile, version) || !reader.Bytes.empty() || !Valid(tile)) {
    return std::nullopt;
  }
  if (tile.SurfaceError) {
    if (currentSourceKey == 0) { return std::nullopt; }
    tile.SurfaceError->Upper.SourceKey = currentSourceKey;
  }
  tile.GeometryAssetKey = inputKey;
  return tile;
}
}
