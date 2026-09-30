#include "StructureArtifact.h"
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
constexpr uint32_t kVersion = 3;
constexpr size_t kHashBytes = 64;
constexpr size_t kMostBytes = kStructureArtifactBytesMost;
constexpr std::string_view kMagic = "outshine-structure";

class Writer {
public:
  Writer() = default;

  Writer(const StructureArtifactSink &sink, size_t blockBytes)
      : Sink(&sink), BlockBytes(blockBytes) {}

  bool Flush() {
    if (Bytes.empty()) { return true; }
    if (Sink == nullptr || !(*Sink)(Bytes)) {
      Failure = StructureArtifactError::WriteFailed;
      return false;
    }
    Bytes.clear();
    return true;
  }

  template <typename T> bool Number(const T &value) {
    if constexpr (std::is_enum_v<T>) {
      return Number(static_cast<std::underlying_type_t<T>>(value));
    } else if constexpr (std::is_same_v<T, bool>) {
      return Number(static_cast<uint8_t>(value));
    } else {
      if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
          Failure = StructureArtifactError::InvalidScalar;
          return false;
        }
      }
      if (Sink != nullptr) {
        if (BlockBytes < sizeof(T)) { return false; }
        if (Bytes.size() > BlockBytes - sizeof(T) && !Flush()) { return false; }
      } else if (Bytes.size() > kMostBytes - sizeof(T)) {
        return false;
      }
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
    return std::ranges::all_of(items, [&](const auto &item) { return visit(*this, item); });
  }

  template <typename T, typename Visit> bool Maybe(const std::optional<T> &item, Visit visit) {
    return Number(item.has_value()) && (!item || visit(*this, *item));
  }

  std::vector<uint8_t> Bytes;
  StructureArtifactError Failure = StructureArtifactError::CapacityExceeded;
  const StructureArtifactSink *Sink = nullptr;
  size_t BlockBytes = 0;
};

class Reader {
public:
  explicit Reader(std::span<const uint8_t> bytes) : Bytes(bytes), Remaining(bytes.size()) {}

  Reader(const StructureArtifactSource &source, StructureArtifactReadLimits limits)
      : Remaining(limits.EncodedBytes), AllocationLeft(limits.ResidentBytesMost), Source(&source) {}

  bool Get(std::span<uint8_t> into) {
    if (into.size() > Remaining) { return false; }
    if (Source != nullptr) {
      if (!(*Source)(into)) { return false; }
    } else {
      std::copy_n(Bytes.begin(), into.size(), into.begin());
      Bytes = Bytes.subspan(into.size());
    }
    Remaining -= into.size();
    return true;
  }

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
      std::array<uint8_t, sizeof(T)> encoded{};
      if (!Get(encoded)) { return false; }
      using Word = std::conditional_t<sizeof(T) == 8,
                                      uint64_t,
                                      std::conditional_t<sizeof(T) == 4, uint32_t, uint8_t>>;
      static_assert(sizeof(Word) == sizeof(T));
      Word bits = 0;
      for (size_t i = 0; i < sizeof(T); ++i) {
        bits |= static_cast<Word>(static_cast<Word>(encoded[i]) << (i * 8));
      }
      value = std::bit_cast<T>(bits);
      if constexpr (std::is_floating_point_v<T>) { return std::isfinite(value); }
      return true;
    }
  }

  template <typename T, typename Visit> bool List(std::vector<T> &items, Visit visit) {
    uint64_t count = 0;
    if (!Number(count) || count > Remaining || count > AllocationLeft / sizeof(T)) { return false; }
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
  size_t Remaining = 0;
  size_t AllocationLeft = kMostBytes;
  const StructureArtifactSource *Source = nullptr;
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
         archive.Number(value.FirstHole) && archive.Number(value.HoleCount) &&
         archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
         archive.Number(value.BaseM) && archive.Number(value.SeatM) &&
         archive.Number(value.FootM) && archive.Number(value.Source) &&
         archive.Number(street.Known) && archive.Number(street.KerbEm) &&
         archive.Number(street.KerbNm) && archive.Number(street.AlongE) &&
         archive.Number(street.AlongN) && archive.Number(street.ToStreetE) &&
         archive.Number(street.ToStreetN);
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
               item.MinimumHeightM >= 0.0f &&
               (item.MinimumHeightM == 0.0f || item.MinimumHeightM < item.HeightM) &&
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

bool Text(Writer &writer, std::string_view value) {
  return writer.Number(static_cast<uint64_t>(value.size())) &&
         std::ranges::all_of(value,
                             [&](char byte) { return writer.Number(static_cast<uint8_t>(byte)); });
}

bool Identity(Writer &writer, const Data::TileSourceIdentity &value) {
  return writer.Number(value.From) && writer.Number(value.Kind) && writer.Number(value.Tile.Zoom) &&
         writer.Number(value.Tile.X) && writer.Number(value.Tile.Y) &&
         Text(writer, value.SourceId) && Text(writer, value.Revision);
}

bool HeightInputs(Writer &writer, const Ground::HeightField &heights) {
  if (!writer.Number(heights.CaptureRequest().Zoom) || !writer.Number(heights.Fallback()) ||
      !writer.Number(static_cast<uint64_t>(heights.Blocks().size()))) {
    return false;
  }
  for (const auto &block : heights.Blocks()) {
    if (!writer.Number(block.At.Zoom) || !writer.Number(block.At.X) || !writer.Number(block.At.Y) ||
        !writer.Number(block.Raster.Side) || !writer.Number(block.Raster.Postings) ||
        !writer.Number(block.MissingBoundary) ||
        !writer.Number(static_cast<uint64_t>(block.Sources.size())) ||
        !std::ranges::all_of(block.Sources,
                             [&](const auto &source) { return Identity(writer, source); })) {
      return false;
    }
    const auto nodes =
        block.Terrain
            ? std::span(block.Terrain->Data(),
                        static_cast<size_t>(block.Terrain->Rows()) * block.Terrain->Cols())
            : std::span<const float>(block.Nodes);
    if (!writer.Number(static_cast<uint64_t>(nodes.size())) ||
        !std::ranges::all_of(nodes, [&](float node) { return writer.Number(node); })) {
      return false;
    }
  }
  return true;
}
}

std::expected<void, StructureArtifactError>
WriteStructureProduct(const BakedTile &tile, const StructureArtifactSink &sink, size_t blockBytes) {
  if (!Valid(tile)) { return std::unexpected(StructureArtifactError::InvalidProduct); }
  if (!sink || blockBytes < sizeof(uint64_t) || blockBytes > kMostBytes) {
    return std::unexpected(StructureArtifactError::CapacityExceeded);
  }
  Writer writer(sink, blockBytes);
  if (!Product(writer, tile) || !writer.Flush()) { return std::unexpected(writer.Failure); }
  return {};
}

std::optional<BakedTile> ReadStructureProduct(const StructureArtifactSource &source,
                                              StructureArtifactReadLimits limits,
                                              uint64_t currentSourceKey) {
  if (!source || limits.EncodedBytes == 0 || limits.ResidentBytesMost == 0) { return std::nullopt; }
  Reader reader(source, limits);
  BakedTile tile;
  if (!Product(reader, tile) || reader.Remaining != 0 || !Valid(tile)) { return std::nullopt; }
  if (tile.SurfaceError) {
    if (currentSourceKey == 0) { return std::nullopt; }
    tile.SurfaceError->Upper.SourceKey = currentSourceKey;
  }
  return tile;
}

std::optional<std::string>
StructureArtifactKey(const RawTile &raw,
                     const Ground::HeightField &heights,
                     const std::optional<Data::TileSourceIdentity> &source,
                     std::string_view producerVersion) {
  if (producerVersion.empty()) { return std::nullopt; }
  Writer writer;
  if (raw.Original.Snapshot) {
    const uint64_t original = StructureSourceKey(
        {.Vector = std::nullopt, .HeightSources = {}, .Original = &raw.Original});
    if (!writer.Number(original)) { return std::nullopt; }
  }
  const auto structure = [](auto &archive, const RawTile::Structure &value) {
    return archive.Number(value.LocalFirst) && archive.Number(value.PointCount) &&
           archive.Number(value.SourceFirst) && archive.Number(value.FirstHole) &&
           archive.Number(value.HoleCount) && archive.Number(value.SourceFirstHole) &&
           archive.Number(value.Cell.Index) && bounds(archive, value.Cell.Footprint) &&
           archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
           archive.Number(value.Pitched) && archive.Number(value.HeightOrigin.has_value()) &&
           (!value.HeightOrigin || archive.Number(*value.HeightOrigin)) &&
           archive.Number(value.OriginalId.Kind) && archive.Number(value.OriginalId.Id);
  };
  const auto ring = [](auto &archive, const GeographicRing &value) {
    return archive.Number(value.First) && archive.Number(value.Count) &&
           archive.Number(value.Exterior);
  };
  const auto way = [](auto &archive, const RawTile::Way &value) {
    return archive.Number(value.LocalFirst) && archive.Number(value.PointCount) &&
           archive.Number(value.HalfWidthM);
  };
  if (!Text(writer, kMagic) || !writer.Number(kVersion) || !Text(writer, producerVersion) ||
      !writer.Number(source.has_value()) || (source && !Identity(writer, *source)) ||
      !writer.List(raw.LatLon, scalar) || !writer.List(raw.Structures, structure) ||
      !writer.List(raw.Holes, ring) || !writer.List(raw.Ways, way) ||
      !writer.Maybe(raw.RequestedDetail, scalar) || !writer.Maybe(raw.RequestedCell, scalar) ||
      !writer.Number(raw.TileSpanM) || !writer.Number(raw.Extent) ||
      !writer.Number(raw.ClusterTriangles)) {
    return std::nullopt;
  }
  for (size_t at = 0; at < 3; ++at) {
    if (!writer.Number(raw.AnchorEcef[at])) { return std::nullopt; }
  }
  if (!raw.RequestedDetail &&
      (!writer.Number(raw.Eye.LongitudeDeg) || !writer.Number(raw.Eye.LatitudeDeg) ||
       !writer.Number(raw.FocalPx))) {
    return std::nullopt;
  }
  if (!HeightInputs(writer, heights)) { return std::nullopt; }
  return Sha256Hex(writer.Bytes.data(), writer.Bytes.size());
}

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodeStructureArtifact(const BakedTile &tile, std::string_view inputKey) {
  if (!KeyValid(inputKey)) { return std::unexpected(StructureArtifactError::InvalidKey); }
  if (!Valid(tile)) { return std::unexpected(StructureArtifactError::InvalidProduct); }
  Writer writer;
  writer.Bytes.insert(writer.Bytes.end(), kMagic.begin(), kMagic.end());
  if (!writer.Number(kVersion)) { return std::unexpected(writer.Failure); }
  writer.Bytes.insert(writer.Bytes.end(), inputKey.begin(), inputKey.end());
  if (!Product(writer, tile) || writer.Bytes.size() > kMostBytes - kHashBytes) {
    return std::unexpected(writer.Failure);
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
  reader.Remaining -= kHashBytes;
  BakedTile tile;
  if (!Product(reader, tile) || reader.Remaining != 0 || !Valid(tile)) { return std::nullopt; }
  if (tile.SurfaceError) {
    if (currentSourceKey == 0) { return std::nullopt; }
    tile.SurfaceError->Upper.SourceKey = currentSourceKey;
  }
  return tile;
}
}
