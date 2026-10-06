#include "PreparedStructureCodec.h"
#include "math/Units.h"
#include "StructureBinary.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <expected>
#include <optional>
#include <vector>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

namespace outshine::Generators {
namespace {
using StructureBinary::Reader;
using StructureBinary::Writer;
constexpr uint32_t kMagic = 0x31425350;
constexpr uint32_t kVersion = 3;
constexpr auto scalar = [](auto &archive, auto &value) { return archive.Number(value); };
constexpr auto point = [](auto &archive, auto &value) {
  return archive.Number(value.EastM) && archive.Number(value.NorthM);
};
constexpr auto vector = [](auto &archive, auto &value) {
  return archive.Number(value[0]) && archive.Number(value[1]) && archive.Number(value[2]);
};
constexpr auto bounds = [](auto &archive, auto &value) {
  return archive.Number(value.MinLonDeg) && archive.Number(value.MinLatDeg) &&
         archive.Number(value.MaxLonDeg) && archive.Number(value.MaxLatDeg);
};
constexpr auto coverage = [](auto &archive, auto &value) {
  return archive.Number(value.WestDeg) && archive.Number(value.SouthDeg) &&
         archive.Number(value.EastDeg) && archive.Number(value.NorthDeg);
};
constexpr auto ring = [](auto &archive, auto &value) {
  return archive.Number(value.First) && archive.Number(value.Count) &&
         archive.Number(value.Exterior);
};
constexpr auto cell = [](auto &archive, auto &value) {
  return archive.Number(value.Level) && archive.Number(value.X) && archive.Number(value.Y);
};
constexpr auto street = [](auto &archive, auto &value) {
  return archive.Number(value.Known) && archive.Number(value.KerbEm) &&
         archive.Number(value.KerbNm) && archive.Number(value.AlongE) &&
         archive.Number(value.AlongN) && archive.Number(value.ToStreetE) &&
         archive.Number(value.ToStreetN);
};
constexpr auto footprint = [](auto &archive, auto &value) {
  return archive.Number(value.FirstPoint) && archive.Number(value.PointCount) &&
         archive.Number(value.FirstHole) && archive.Number(value.HoleCount) &&
         archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
         archive.Number(value.BaseM) && archive.Number(value.SeatM) &&
         archive.Number(value.FootM) && archive.Number(value.Source) &&
         street(archive, value.Street);
};

bool text(Writer &archive, const std::string &value) {
  if (!archive.Number(static_cast<uint64_t>(value.size()))) { return false; }
  return std::ranges::all_of(value,
                             [&](char c) { return archive.Number(static_cast<uint8_t>(c)); });
}

bool text(Reader &archive, std::string &value) {
  uint64_t count = 0;
  if (!archive.Number(count) || count > archive.Remaining || count > archive.AllocationLeft) {
    return false;
  }
  archive.AllocationLeft -= static_cast<size_t>(count);
  value.resize(static_cast<size_t>(count));
  return archive.Get(std::span(reinterpret_cast<uint8_t *>(value.data()), value.size()));
}

constexpr auto provenance = [](auto &archive, auto &value) {
  return text(archive, value.DatasetId) && text(archive, value.Revision) &&
         archive.List(value.PayloadSha256,
                      [](auto &held, auto &hash) { return text(held, hash); }) &&
         archive.Maybe(value.Cell, cell);
};

bool Origin(Writer &archive, const Data::ProductOrigin &value) {
  return coverage(archive, value.Bounds) && archive.Number(value.Selection) &&
         archive.Number(value.Provenance != nullptr) &&
         (!value.Provenance || provenance(archive, *value.Provenance));
}

bool Origin(Reader &archive, Data::ProductOrigin &value) {
  bool present = false;
  if (!coverage(archive, value.Bounds) || !archive.Number(value.Selection) ||
      !archive.Number(present)) {
    return false;
  }
  if (!present) { return true; }
  if (archive.AllocationLeft < sizeof(Data::SourceProvenance)) { return false; }
  archive.AllocationLeft -= sizeof(Data::SourceProvenance);
  auto data = std::make_shared<Data::SourceProvenance>();
  if (!provenance(archive, *data)) { return false; }
  value.Provenance = std::move(data);
  return true;
}

constexpr auto heightSource = [](auto &archive, auto &value) {
  return archive.Number(value.From) && archive.Number(value.Kind) &&
         archive.Number(value.Tile.Zoom) && archive.Number(value.Tile.X) &&
         archive.Number(value.Tile.Y) &&
         archive.Maybe(value.NativeCell,
                       [](auto &held, auto &native) {
                         return held.Number(native.SouthDeg) && held.Number(native.WestDeg);
                       }) &&
         text(archive, value.SourceId) && text(archive, value.Revision);
};
constexpr auto heightTile = [](auto &archive, auto &value) {
  return archive.Number(value.Zoom) && archive.Number(value.X) && archive.Number(value.Y);
};

constexpr auto layout = [](auto &archive, auto &value) {
  return archive.Number(value.LocalFirst) && archive.Number(value.PointCount) &&
         archive.Number(value.SourceFirst) && archive.Number(value.FirstHole) &&
         archive.Number(value.HoleCount) && archive.Number(value.SourceFirstHole) &&
         archive.Number(value.Cell.Index) && bounds(archive, value.Cell.Footprint) &&
         archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
         archive.Number(value.Pitched) && archive.Maybe(value.WallColour, vector) &&
         archive.Maybe(value.HeightOrigin, scalar) && archive.Number(value.SourceId.Id) &&
         archive.Number(value.SourceId.Kind);
};
constexpr auto prepared = [](auto &archive, auto &value) {
  return layout(archive, value.Layout) && footprint(archive, value.Standing) &&
         bounds(archive, value.Bounds) && archive.Number(value.CornerFirst) &&
         archive.Number(value.BaseAslM) && archive.Number(value.SeatAslM) &&
         archive.Number(value.AreaM2) && archive.Number(value.AcrossM);
};
constexpr auto shape = [](auto &archive, auto &value) {
  return archive.List(value.Ring, point) &&
         archive.List(value.Holes, [](auto &held, auto &hole) { return held.List(hole, point); }) &&
         archive.Number(value.TidiedAway) && archive.List(value.PartyWallEdges, scalar) &&
         archive.Number(value.AreaM2) && point(archive, value.Centre) &&
         point(archive, value.AxisU) && archive.Number(value.HalfUm) &&
         archive.Number(value.HalfVm) && archive.Number(value.Fill) && archive.Number(value.Form) &&
         archive.Number(value.Roof) && archive.Number(value.Storeys) &&
         archive.Number(value.FloorM) && archive.Number(value.FootM) &&
         archive.Number(value.SeatM) && archive.Number(value.SoleM) &&
         archive.Number(value.EavesM) && archive.Number(value.RiseM) &&
         archive.Number(value.BreakFracV) && archive.Number(value.BreakRiseM) &&
         archive.Number(value.PeriodM) && archive.Number(value.BayM) &&
         archive.Number(value.OverhangM) && archive.Number(value.Seed) &&
         archive.Number(value.Ident) && archive.Number(value.WallVariant) &&
         archive.Number(value.FrontEdge);
};
}

class PreparedStructureCodec {
public:
  template <typename Archive, typename Surface>
  static bool SurfaceFields(Archive &archive, Surface &value) {
    using SupportFlag = std::conditional_t<std::is_same_v<Archive, Writer>, const bool, bool>;
    SupportFlag supported = !value.Shapes_.empty();
    if (!archive.Number(supported)) { return false; }
    if (!supported) { return true; }
    return vector(archive, value.Origin_) && vector(archive, value.Axes_.East) &&
           vector(archive, value.Axes_.North) && vector(archive, value.Axes_.Up) &&
           vector(archive, value.Bounds_.Min) && vector(archive, value.Bounds_.Max) &&
           archive.Number(value.MinimumHeightM_) && archive.Maybe(value.WallColour_, vector) &&
           archive.List(value.Shapes_, shape) && archive.List(value.FaceOffsets_, scalar);
  }

  template <typename Archive, typename Tile> static bool TileFields(Archive &archive, Tile &value) {
    return vector(archive, value.AnchorEcef) && archive.Number(value.TileSpanM) &&
           archive.Number(value.Extent) && archive.Number(value.FallbackHeights) &&
           archive.Number(value.SkippedRings) && archive.Number(value.NoGround) &&
           Origin(archive, value.Origin) && archive.List(value.HeightSources, heightSource) &&
           archive.Number(value.HeightRasterDigest) && archive.Number(value.HeightQualified) &&
           archive.Number(value.HeightRequest.Zoom) &&
           archive.Number(value.HeightRequest.Fallback) &&
           archive.List(value.HeightRequest.Tiles, heightTile) &&
           archive.List(value.PointsLatLon, scalar) && archive.List(value.Holes, ring) &&
           archive.List(value.CornerAslM, scalar) && archive.List(value.Structures, prepared) &&
           archive.List(value.Surfaces,
                        [](auto &held, auto &surface) { return SurfaceFields(held, surface); });
  }

  static bool ValidSurface(const BuildingSurface &value) {
    if (value.Shapes_.empty()) { return value.Bounds_.Empty() && value.FaceOffsets_.empty(); }
    if (value.Bounds_.Empty() || value.FaceOffsets_.size() != value.Shapes_.size() + 1 ||
        value.FaceOffsets_[0] != 0) {
      return false;
    }
    size_t faces = 0;
    for (size_t index = 0; index < value.Shapes_.size(); ++index) {
      const auto &part = value.Shapes_[index];
      if (!part.Valid() || part.PartyWallEdges.size() != part.Ring.size() ||
          part.Form > BuildingForm::Spire || part.Roof > RoofKind::Dome || part.Storeys < 1 ||
          part.FloorM <= 0 || part.BayM <= 0 || part.FrontEdge < -1 ||
          (part.FrontEdge >= 0 && std::cmp_greater_equal(part.FrontEdge, part.Ring.size()))) {
        return false;
      }
      faces += part.Ring.size() + 2;
      for (const auto &hole : part.Holes) {
        if (hole.size() < 3) { return false; }
        faces += hole.size();
      }
      if (value.FaceOffsets_[index + 1] != faces) { return false; }
    }
    return true;
  }

  static bool Valid(const PreparedStructureTile &value) {
    if (value.NoGround != 0 || value.SkippedRings != 0 || value.FallbackHeights ||
        value.Extent <= 0 || value.TileSpanM < 0 || value.PointsLatLon.size() % 2 != 0 ||
        value.Surfaces.size() != value.Structures.size() ||
        (value.Origin.Provenance && value.Origin.Provenance->Cell &&
         !value.Origin.Provenance->Cell->Valid())) {
      return false;
    }
    const auto validTile = [](int zoom, auto x, auto y) {
      return zoom >= 0 && zoom <= Data::TileId::MaximumZoom && std::cmp_greater_equal(x, 0) &&
             std::cmp_greater_equal(y, 0) &&
             std::cmp_less(x, uint64_t{1} << static_cast<unsigned>(zoom)) &&
             std::cmp_less(y, uint64_t{1} << static_cast<unsigned>(zoom));
    };
    if (value.HeightRequest.Zoom < 0 || value.HeightRequest.Zoom > Data::TileId::MaximumZoom ||
        (value.HeightQualified && (value.HeightSources.empty() || value.HeightRequest.Fallback))) {
      return false;
    }
    for (const auto &tile : value.HeightRequest.Tiles) {
      if (!validTile(tile.Zoom, tile.X, tile.Y)) { return false; }
    }
    for (const auto &source : value.HeightSources) {
      if (source.From < Data::TileSourceIdentity::Origin::Provider ||
          source.From > Data::TileSourceIdentity::Origin::Shaped ||
          source.Kind != Data::DataKind::Elevation ||
          !validTile(source.Tile.Zoom, source.Tile.X, source.Tile.Y) ||
          (source.NativeCell &&
           (source.NativeCell->SouthDeg < -static_cast<int>((kDegPerHalfTurn / 2)) ||
            source.NativeCell->SouthDeg >= static_cast<int>((kDegPerHalfTurn / 2)) ||
            source.NativeCell->WestDeg < -static_cast<int>(kDegPerHalfTurn) ||
            source.NativeCell->WestDeg >= static_cast<int>(kDegPerHalfTurn)))) {
        return false;
      }
    }
    const auto points = value.PointsLatLon.size() / 2;
    for (const auto &hole : value.Holes) {
      if (hole.First > points || hole.Count > points - hole.First || hole.Count < 3) {
        return false;
      }
    }
    for (size_t index = 0; index < value.Structures.size(); ++index) {
      const auto &entry = value.Structures[index];
      const auto &record = entry.Layout;
      if (record.PointCount < 3 || record.LocalFirst > points ||
          record.PointCount > points - record.LocalFirst || record.FirstHole > value.Holes.size() ||
          record.HoleCount > value.Holes.size() - record.FirstHole ||
          entry.CornerFirst > value.CornerAslM.size() ||
          record.PointCount > value.CornerAslM.size() - entry.CornerFirst ||
          record.Cell.Index == 0 || record.Cell.Index > kStructureCellsPerTile ||
          entry.Standing.PointCount != record.PointCount ||
          entry.Standing.Source > Ground::BuildingHeightSource::Generated || entry.AreaM2 < 0 ||
          entry.AcrossM < 0 || !ValidSurface(value.Surfaces[index])) {
        return false;
      }
    }
    return true;
  }
};

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureTile(const PreparedStructureTile &tile) {
  if (!PreparedStructureCodec::Valid(tile)) {
    return std::unexpected(StructureArtifactError::InvalidProduct);
  }
  Writer output;
  if (!output.Number(kMagic) || !output.Number(kVersion) ||
      !PreparedStructureCodec::TileFields(output, tile)) {
    return std::unexpected(output.Failure);
  }
  return std::move(output.Bytes);
}

std::optional<PreparedStructureTile> DecodePreparedStructureTile(std::span<const uint8_t> bytes,
                                                                 size_t residentBytesMost) {
  if (bytes.size() > kStructureArtifactBytesMost) { return std::nullopt; }
  Reader input(bytes);
  input.AllocationLeft = residentBytesMost;
  uint32_t magic = 0;
  uint32_t version = 0;
  PreparedStructureTile tile;
  if (!input.Number(magic) || magic != kMagic || !input.Number(version) || version != kVersion ||
      !PreparedStructureCodec::TileFields(input, tile) || input.Remaining != 0 ||
      !PreparedStructureCodec::Valid(tile)) {
    return std::nullopt;
  }
  return tile;
}
}
