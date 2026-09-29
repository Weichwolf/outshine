#include "StructureArtifact.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace outshine::Generators {
namespace {
static_assert(sizeof(size_t) == sizeof(uint64_t));
constexpr uint32_t kVersion = 1;
constexpr size_t kHashBytes = 64;
constexpr size_t kMostBytes = size_t{64} * 1024 * 1024;
constexpr std::string_view kMagic = "outshine-structure";

class Writer {
public:
  template <typename T> bool Number(const T &value) {
    if constexpr (std::is_enum_v<T>) {
      return Number(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
      return Number(static_cast<uint8_t>(value));
    } else {
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) { return false; }
      }
      if (Bytes.size() > kMostBytes - sizeof(T)) { return false; }
      using Word = std::conditional_t<sizeof(T) == 8,
                                      uint64_t,
                                      std::conditional_t<sizeof(T) == 4, uint32_t, uint8_t>>;
      static_assert(sizeof(Word) == sizeof(T));
      const auto bits = std::bit_cast<Word>(value);
      for (size_t i = 0; i < sizeof(T); ++i) {
        Bytes.push_back(static_cast<uint8_t>(bits >> (i * 8)));
      }
      return true;
    }
  }

  template <typename T, typename Visit> bool List(const std::vector<T> &items, Visit visit) {
    if (!Number(static_cast<uint64_t>(items.size()))) { return false; }
    for (const auto &item : items) {
      if (!visit(*this, item)) { return false; }
    }
    return true;
  }

  template <typename T, typename Visit> bool Maybe(const std::optional<T> &item, Visit visit) {
    return Number(item.has_value()) && (!item || visit(*this, *item));
  }

  std::vector<uint8_t> Bytes;
};

class Reader {
public:
  explicit Reader(std::span<const uint8_t> bytes) : Bytes(bytes) {}

  template <typename T> bool Number(T &value) {
    if constexpr (std::is_enum_v<T>) {
      std::underlying_type_t<T> underlying{};
      if (!Number(underlying)) { return false; }
      value = static_cast<T>(underlying);
      return true;
    } else if constexpr (std::is_same_v<T, bool>) {
      uint8_t underlying = 0;
      if (!Number(underlying) || underlying > 1) { return false; }
      value = underlying != 0;
      return true;
    } else {
      if (Bytes.size() < sizeof(T)) { return false; }
      using Word = std::conditional_t<sizeof(T) == 8,
                                      uint64_t,
                                      std::conditional_t<sizeof(T) == 4, uint32_t, uint8_t>>;
      static_assert(sizeof(Word) == sizeof(T));
      Word bits = 0;
      for (size_t i = 0; i < sizeof(T); ++i) {
        bits |= static_cast<Word>(static_cast<Word>(Bytes[i]) << (i * 8));
      }
      value = std::bit_cast<T>(bits);
      Bytes = Bytes.subspan(sizeof(T));
      if constexpr (std::is_floating_point_v<T>) { return std::isfinite(value); }
      return true;
    }
  }

  template <typename T, typename Visit> bool List(std::vector<T> &items, Visit visit) {
    uint64_t count = 0;
    if (!Number(count) || count > Bytes.size() || count > AllocationLeft / sizeof(T)) {
      return false;
    }
    AllocationLeft -= static_cast<size_t>(count) * sizeof(T);
    items.resize(static_cast<size_t>(count));
    for (auto &item : items) {
      if (!visit(*this, item)) { return false; }
    }
    return true;
  }

  template <typename T, typename Visit> bool Maybe(std::optional<T> &item, Visit visit) {
    bool present = false;
    if (!Number(present)) { return false; }
    if (!present) {
      item.reset();
      return true;
    }
    item.emplace();
    return visit(*this, *item);
  }

  std::span<const uint8_t> Bytes;
  size_t AllocationLeft = kMostBytes;
};

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
constexpr auto cluster = [](auto &archive, auto &value) {
  for (size_t i = 0; i < 3; ++i) {
    if (!archive.Number(value.SelfCenter[i]) || !archive.Number(value.ParentCenter[i])) {
      return false;
    }
  }
  return archive.Number(value.SelfRadius) && archive.Number(value.ParentRadius) &&
         archive.Number(value.SelfErr) && archive.Number(value.ParentErr) &&
         archive.Number(value.First) && archive.Number(value.Count) && archive.Number(value.Level);
};
constexpr auto footprint = [](auto &archive, auto &value) {
  auto &street = value.Street;
  return archive.Number(value.FirstPoint) && archive.Number(value.PointCount) &&
         archive.Number(value.HeightM) && archive.Number(value.BaseM) &&
         archive.Number(value.SeatM) && archive.Number(value.FootM) &&
         archive.Number(value.Source) && archive.Number(street.Known) &&
         archive.Number(street.KerbEm) && archive.Number(street.KerbNm) &&
         archive.Number(street.AlongE) && archive.Number(street.AlongN) &&
         archive.Number(street.ToStreetE) && archive.Number(street.ToStreetN);
};
constexpr auto surface = [](auto &archive, auto &value) {
  return archive.Number(value.Upper.ReferenceToVariantM) &&
         archive.Number(value.Upper.VariantToReferenceM) &&
         archive.Number(value.ReferenceToVariantLowerM) &&
         archive.Number(value.VariantToReferenceLowerM);
};

template <typename Archive, typename Tile> bool Product(Archive &archive, Tile &tile) {
  if (!archive.List(tile.Built.WallCorners, vertex) ||
      !archive.List(tile.Built.RoofCorners, vertex) || !archive.List(tile.Built.WallRun, scalar) ||
      !archive.List(tile.Built.RoofRun, scalar) || !archive.List(tile.Walls.Clusters, cluster) ||
      !archive.List(tile.Walls.Index, scalar) || !archive.List(tile.Roofs.Clusters, cluster) ||
      !archive.List(tile.Roofs.Index, scalar) || !archive.Number(tile.Digest) ||
      !archive.Number(tile.FallbackHeights) || !archive.Maybe(tile.RequestedDetail, scalar) ||
      !archive.Maybe(tile.RequestedCell, scalar) || !archive.Maybe(tile.FootprintBounds, bounds) ||
      !archive.Number(tile.OccupiedCells)) {
    return false;
  }
  for (auto &cell : tile.CellBounds) {
    if (!bounds(archive, cell)) { return false; }
  }
  for (auto &height : tile.CellMaxHeightM) {
    if (!archive.Number(height)) { return false; }
  }
  return archive.List(tile.Prints, footprint) && archive.List(tile.FootprintDetails, scalar) &&
         archive.List(tile.SeatSpreadM, scalar) && archive.List(tile.AcrossM, scalar) &&
         archive.Number(tile.OsmHeights) && archive.Number(tile.DefaultHeights) &&
         archive.Number(tile.Fronted) && archive.Number(tile.Lumped) &&
         archive.Number(tile.Blocks) && archive.Number(tile.NoGround) &&
         archive.Number(tile.UnsupportedMeshes) && archive.Number(tile.SkippedRings) &&
         archive.Maybe(tile.SurfaceError, surface) && archive.Maybe(tile.SurfaceFailure, scalar);
}

bool MeshValid(const std::vector<StoredVertex> &vertices,
               const std::vector<uint32_t> &indices,
               const ClusteredMesh &mesh) {
  const auto validIndex = [&](uint32_t index) { return index < vertices.size(); };
  if (indices.size() % 3 != 0 || mesh.Index.size() % 3 != 0 ||
      !std::ranges::all_of(indices, validIndex) || !std::ranges::all_of(mesh.Index, validIndex)) {
    return false;
  }
  return std::ranges::all_of(mesh.Clusters, [&](const DagCluster &item) {
    return item.First <= mesh.Index.size() && item.Count <= mesh.Index.size() - item.First &&
           item.Count % 3 == 0 && item.SelfRadius >= 0 && item.ParentRadius >= 0 &&
           item.SelfErr >= 0 && item.ParentErr >= 0;
  });
}

bool Valid(const BakedTile &tile) {
  const auto detail = [](LevelOfDetail value) { return value <= LevelOfDetail::Skyline; };
  if ((tile.RequestedDetail && !detail(*tile.RequestedDetail)) ||
      (tile.RequestedCell &&
       (*tile.RequestedCell == 0 || *tile.RequestedCell > kStructureCellsPerTile)) ||
      tile.Prints.size() != tile.FootprintDetails.size() ||
      tile.Prints.size() != tile.SeatSpreadM.size() || tile.Prints.size() != tile.AcrossM.size() ||
      !std::ranges::all_of(tile.FootprintDetails, detail) || tile.OsmHeights < 0 ||
      tile.DefaultHeights < 0 || tile.Fronted < 0 || tile.Lumped < 0 || tile.Blocks < 0 ||
      tile.NoGround < 0 ||
      (tile.SurfaceFailure && (*tile.SurfaceFailure < StructureSurfaceErrorFailure::InvalidSource ||
                               *tile.SurfaceFailure > StructureSurfaceErrorFailure::Cancelled))) {
    return false;
  }
  if (!std::ranges::all_of(tile.Prints, [](const Ground::BuildingField::Footprint &item) {
        return item.Source <= Ground::BuildingField::HeightSource::Default &&
               item.PointCount >= 3 &&
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
  return MeshValid(tile.Built.WallCorners, tile.Built.WallRun, tile.Walls) &&
         MeshValid(tile.Built.RoofCorners, tile.Built.RoofRun, tile.Roofs);
}

bool KeyValid(std::string_view key) {
  return key.size() == kHashBytes && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}
}

std::optional<std::vector<uint8_t>> EncodeStructureArtifact(const BakedTile &tile,
                                                            std::string_view inputKey) {
  if (!KeyValid(inputKey) || !Valid(tile)) { return std::nullopt; }
  Writer writer;
  writer.Bytes.insert(writer.Bytes.end(), kMagic.begin(), kMagic.end());
  if (!writer.Number(kVersion)) { return std::nullopt; }
  writer.Bytes.insert(writer.Bytes.end(), inputKey.begin(), inputKey.end());
  if (!Product(writer, tile) || writer.Bytes.size() > kMostBytes - kHashBytes) {
    return std::nullopt;
  }
  const auto checksum = Sha256Hex(writer.Bytes.data(), writer.Bytes.size());
  writer.Bytes.insert(writer.Bytes.end(), checksum.begin(), checksum.end());
  return std::move(writer.Bytes);
}

std::optional<BakedTile> DecodeStructureArtifact(std::span<const uint8_t> bytes,
                                                 std::string_view inputKey,
                                                 uint64_t currentSourceKey) {
  const size_t header = kMagic.size() + sizeof(kVersion) + kHashBytes;
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
  if (!reader.Number(version) || version != kVersion ||
      !std::equal(inputKey.begin(), inputKey.end(), reader.Bytes.begin())) {
    return std::nullopt;
  }
  reader.Bytes = reader.Bytes.subspan(kHashBytes);
  BakedTile tile;
  if (!Product(reader, tile) || !reader.Bytes.empty() || !Valid(tile)) { return std::nullopt; }
  if (tile.SurfaceError) {
    if (currentSourceKey == 0) { return std::nullopt; }
    tile.SurfaceError->Upper.SourceKey = currentSourceKey;
  }
  return tile;
}
}
