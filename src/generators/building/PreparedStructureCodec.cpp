#include "PreparedStructureCodec.h"
#include "math/Units.h"
#include "StructureBinary.h"
#include "PreparedStructurePlan.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
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
constexpr uint32_t kLegacyVersion = 3;
constexpr uint32_t kVersion = 6;
constexpr uint32_t kLegacyIndexVersion = 4;
constexpr uint32_t kIndexVersion = 7;
constexpr uint32_t kBasisVersion = 5;
constexpr uint32_t kLegacyBlockMagic = 0x31435342;
constexpr uint32_t kBlockMagic = 0x32435342;
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
constexpr auto indexedHeightTile = [](auto &archive, auto &value) {
  using Archive = std::remove_reference_t<decltype(archive)>;
  if constexpr (std::is_same_v<Archive, Writer>) {
    return archive.Number(value.Zoom) && archive.Number(static_cast<uint32_t>(value.X)) &&
           archive.Number(static_cast<uint32_t>(value.Y));
  } else {
    uint32_t x = 0;
    uint32_t y = 0;
    if (!archive.Number(value.Zoom) || !archive.Number(x) || !archive.Number(y)) { return false; }
    value.X = static_cast<long>(x);
    value.Y = static_cast<long>(y);
  }
  return true;
};

constexpr auto layout = [](auto &archive, auto &value, bool facadeFields) {
  return archive.Number(value.LocalFirst) && archive.Number(value.PointCount) &&
         archive.Number(value.SourceFirst) && archive.Number(value.FirstHole) &&
         archive.Number(value.HoleCount) && archive.Number(value.SourceFirstHole) &&
         archive.Number(value.Cell.Index) && bounds(archive, value.Cell.Footprint) &&
         archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
         archive.Number(value.Pitched) && archive.Maybe(value.WallColour, vector) &&
         archive.Maybe(value.HeightOrigin, scalar) && archive.Number(value.SourceId.Id) &&
         archive.Number(value.SourceId.Kind) &&
         (!facadeFields || archive.Maybe(value.Facade, scalar));
};
constexpr auto prepared = [](auto &archive, auto &value, bool facadeFields) {
  return layout(archive, value.Layout, facadeFields) && footprint(archive, value.Standing) &&
         bounds(archive, value.Bounds) && archive.Number(value.CornerFirst) &&
         archive.Number(value.BaseAslM) && archive.Number(value.SeatAslM) &&
         archive.Number(value.AreaM2) && archive.Number(value.AcrossM);
};
constexpr auto shape = [](auto &archive, auto &value, bool facadeFields) {
  const bool read =
      archive.List(value.Ring, point) &&
      archive.List(value.Holes, [](auto &held, auto &hole) { return held.List(hole, point); }) &&
      archive.Number(value.TidiedAway) && archive.List(value.PartyWallEdges, scalar) &&
      archive.Number(value.AreaM2) && point(archive, value.Centre) && point(archive, value.AxisU) &&
      archive.Number(value.HalfUm) && archive.Number(value.HalfVm) && archive.Number(value.Fill) &&
      archive.Number(value.Form) && archive.Number(value.Roof) && archive.Number(value.Storeys) &&
      archive.Number(value.FloorM) && archive.Number(value.FootM) && archive.Number(value.SeatM) &&
      archive.Number(value.SoleM) && archive.Number(value.EavesM) && archive.Number(value.RiseM) &&
      archive.Number(value.BreakFracV) && archive.Number(value.BreakRiseM) &&
      archive.Number(value.PeriodM) && archive.Number(value.BayM) &&
      archive.Number(value.OverhangM) && archive.Number(value.Seed) &&
      archive.Number(value.Ident) && archive.Number(value.WallVariant) &&
      archive.Number(value.FrontEdge);
  if (!read) { return false; }
  if (facadeFields) { return archive.Number(value.OpeningStyle); }
  if constexpr (std::is_same_v<std::remove_cvref_t<decltype(archive)>, Reader>) {
    value.OpeningStyle = BuildingFacadeStyle(value.Form);
  }
  return true;
};
}

class PreparedStructureCodec {
public:
  template <typename Archive, typename Surface>
  static bool SurfaceFields(Archive &archive, Surface &value, bool facadeFields = true) {
    if constexpr (std::is_same_v<Archive, Writer>) {
      if (value.Block_) { return SurfaceFields(archive, value.Resident(), facadeFields); }
    }
    using SupportFlag = std::conditional_t<std::is_same_v<Archive, Writer>, const bool, bool>;
    SupportFlag supported = !value.Shapes_.empty();
    if (!archive.Number(supported)) { return false; }
    if (!supported) { return true; }
    return vector(archive, value.Origin_) && vector(archive, value.Axes_.East) &&
           vector(archive, value.Axes_.North) && vector(archive, value.Axes_.Up) &&
           vector(archive, value.Bounds_.Min) && vector(archive, value.Bounds_.Max) &&
           archive.Number(value.MinimumHeightM_) && archive.Maybe(value.WallColour_, vector) &&
           archive.List(value.Shapes_,
                        [facadeFields](auto &held, auto &part) {
                          return shape(held, part, facadeFields);
                        }) &&
           archive.List(value.FaceOffsets_, scalar);
  }

  template <typename Archive, typename Tile, typename Visit>
  static bool HeaderFields(Archive &archive, Tile &value, Visit visitHeightTile) {
    return vector(archive, value.AnchorEcef) && archive.Number(value.TileSpanM) &&
           archive.Number(value.Extent) && archive.Number(value.FallbackHeights) &&
           archive.Number(value.SkippedRings) && archive.Number(value.NoGround) &&
           Origin(archive, value.Origin) && archive.List(value.HeightSources, heightSource) &&
           archive.Number(value.HeightRasterDigest) && archive.Number(value.HeightQualified) &&
           archive.Number(value.HeightRequest.Zoom) &&
           archive.Number(value.HeightRequest.Fallback) &&
           archive.List(value.HeightRequest.Tiles, visitHeightTile);
  }

  template <typename Archive, typename Tile, typename Visit>
  static bool
  MetadataFields(Archive &archive, Tile &value, Visit visitHeightTile, bool facadeFields = true) {
    return HeaderFields(archive, value, visitHeightTile) &&
           archive.List(value.PointsLatLon, scalar) && archive.List(value.Holes, ring) &&
           archive.List(value.CornerAslM, scalar) &&
           archive.List(value.Structures, [facadeFields](auto &held, auto &entry) {
             return prepared(held, entry, facadeFields);
           });
  }

  template <typename Archive, typename Tile>
  static bool TileFields(Archive &archive, Tile &value, bool facadeFields = true) {
    return MetadataFields(archive, value, heightTile, facadeFields) &&
           archive.List(value.Surfaces, [facadeFields](auto &held, auto &surface) {
             return SurfaceFields(held, surface, facadeFields);
           });
  }

  template <typename Archive, typename Surface>
  static bool
  SummaryFields(Archive &archive, Surface &value, BuildingSurface::Selection &selection) {
    using SupportFlag = std::conditional_t<std::is_same_v<Archive, Writer>, const bool, bool>;
    SupportFlag supported = selection.Faces != 0;
    if (!archive.Number(supported)) { return false; }
    if (!supported) { return true; }
    const auto box = [](auto &held, auto &extent) {
      return vector(held, extent.Min) && vector(held, extent.Max);
    };
    return vector(archive, value.Origin_) && vector(archive, value.Axes_.East) &&
           vector(archive, value.Axes_.North) && vector(archive, value.Axes_.Up) &&
           box(archive, value.Bounds_) && archive.Number(value.MinimumHeightM_) &&
           archive.Maybe(value.WallColour_, vector) && archive.Maybe(selection.Envelope, box) &&
           archive.Maybe(selection.ShellErrorM, scalar) && archive.Number(selection.Faces) &&
           archive.Number(selection.Projectable);
  }

  static bool IndexFields(Writer &archive, const PreparedStructureTile &value) {
    if (!MetadataFields(archive, value, indexedHeightTile) ||
        !archive.Number(uint64_t{value.Surfaces.size()})) {
      return false;
    }
    const BuildingMesh mesher;
    BuildingScratch scratch;
    for (size_t index = 0; index < value.Surfaces.size(); ++index) {
      const auto &surface = value.Surfaces[index];
      auto plan = PreparedStructurePlan(value.Structures[index],
                                        value.PointsLatLon,
                                        value.Holes,
                                        std::span(value.CornerAslM)
                                            .subspan(value.Structures[index].CornerFirst,
                                                     value.Structures[index].Layout.PointCount),
                                        value.AnchorEcef);
      plan.Prepared = &surface;
      if (surface.FaceCount() > UINT32_MAX) { return false; }
      BuildingSurface::Selection selection{.Envelope = mesher.SourceEnvelopeBounds(plan, scratch),
                                           .ShellErrorM = mesher.ShellSurfaceErrorM(plan, scratch),
                                           .Faces = static_cast<uint32_t>(surface.FaceCount()),
                                           .Projectable = surface.SupportsProjection()};
      if (!SummaryFields(archive, surface, selection)) { return false; }
    }
    return true;
  }

  static bool IndexFields(Reader &archive, PreparedStructureTile &value, bool facadeFields) {
    return MetadataFields(archive, value, indexedHeightTile, facadeFields) &&
           archive.List(value.Surfaces, [](auto &held, auto &surface) {
             BuildingSurface::Selection selection;
             if (!SummaryFields(held, surface, selection)) { return false; }
             if (selection.Faces > held.AllocationLeft) { return false; }
             held.AllocationLeft -= selection.Faces;
             if (selection.Faces != 0) { surface.Selection_ = selection; }
             return true;
           });
  }

  static bool ValidSurface(const BuildingSurface &value) {
    if (value.Selection_) {
      const auto &selection = *value.Selection_;
      return !value.Bounds_.Empty() && selection.Faces >= 5 &&
             (!selection.Envelope || !selection.Envelope->Empty()) &&
             (!selection.ShellErrorM || *selection.ShellErrorM >= 0);
    }
    if (value.Shapes_.empty()) { return value.Bounds_.Empty() && value.FaceOffsets_.empty(); }
    if (value.Bounds_.Empty() || value.FaceOffsets_.size() != value.Shapes_.size() + 1 ||
        value.FaceOffsets_[0] != 0) {
      return false;
    }
    size_t faces = 0;
    for (size_t index = 0; index < value.Shapes_.size(); ++index) {
      const auto &part = value.Shapes_[index];
      if (!part.Valid() || part.PartyWallEdges.size() != part.Ring.size() ||
          part.Form > BuildingForm::Spire || part.OpeningStyle < FacadeStyle::Outbuilding ||
          part.OpeningStyle > FacadeStyle::Glazing || part.Roof > RoofKind::Dome ||
          part.Storeys < 1 || part.FloorM <= 0 || part.BayM <= 0 || part.FrontEdge < -1 ||
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

  static bool ValidHeightSources(const auto &value) {
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
    if (!std::ranges::all_of(value.HeightRequest.Tiles, [&](const auto &tile) {
          return validTile(tile.Zoom, tile.X, tile.Y);
        })) {
      return false;
    }
    return std::ranges::all_of(value.HeightSources, [&](const auto &source) {
      return source.From >= Data::TileSourceIdentity::Origin::Provider &&
             source.From <= Data::TileSourceIdentity::Origin::Shaped &&
             source.Kind == Data::DataKind::Elevation &&
             validTile(source.Tile.Zoom, source.Tile.X, source.Tile.Y) &&
             (!source.NativeCell ||
              (source.NativeCell->SouthDeg >= -static_cast<int>(kDegPerHalfTurn / 2) &&
               source.NativeCell->SouthDeg < static_cast<int>(kDegPerHalfTurn / 2) &&
               source.NativeCell->WestDeg >= -static_cast<int>(kDegPerHalfTurn) &&
               source.NativeCell->WestDeg < static_cast<int>(kDegPerHalfTurn)));
    });
  }

  static bool BasisFields(auto &archive, auto &value) {
    return HeaderFields(archive, value, indexedHeightTile) &&
           archive.List(value.PointsLatLon, scalar) && archive.List(value.Holes, ring) &&
           archive.List(value.Sources, [](auto &held, auto &source) {
             return held.Number(source.Cell) && held.Number(source.Id.Id) &&
                    held.Number(source.Id.Kind);
           });
  }

  static bool ValidBasis(const PreparedBuildingBasis &value) {
    if (value.NoGround != 0 || value.SkippedRings != 0 || value.FallbackHeights ||
        value.Extent <= 0 || value.TileSpanM < 0 || value.PointsLatLon.size() % 2 != 0 ||
        (value.Origin.Provenance && value.Origin.Provenance->Cell &&
         !value.Origin.Provenance->Cell->Valid()) ||
        !ValidHeightSources(value)) {
      return false;
    }
    const size_t points = value.PointsLatLon.size() / 2;
    return std::ranges::all_of(value.Holes,
                               [points](const auto &hole) {
                                 return hole.First <= points && hole.Count >= 3 &&
                                        hole.Count <= points - hole.First;
                               }) &&
           std::ranges::all_of(value.Sources, [](const auto &source) {
             return source.Cell != 0 && source.Cell <= kStructureCellsPerTile;
           });
  }

  static bool Valid(const PreparedStructureTile &value) {
    if (value.NoGround != 0 || value.SkippedRings != 0 || value.FallbackHeights ||
        value.Extent <= 0 || value.TileSpanM < 0 || value.PointsLatLon.size() % 2 != 0 ||
        value.Surfaces.size() != value.Structures.size() ||
        (value.Origin.Provenance && value.Origin.Provenance->Cell &&
         !value.Origin.Provenance->Cell->Valid())) {
      return false;
    }
    if (!ValidHeightSources(value)) { return false; }
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
          entry.AcrossM < 0 ||
          (record.Facade &&
           (*record.Facade < FacadeStyle::Outbuilding || *record.Facade > FacadeStyle::Glazing)) ||
          !ValidSurface(value.Surfaces[index])) {
        return false;
      }
    }
    return true;
  }
};

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedBuildingBasis(const PreparedBuildingBasis &basis) {
  if (!PreparedStructureCodec::ValidBasis(basis)) {
    return std::unexpected(StructureArtifactError::InvalidProduct);
  }
  Writer output;
  if (!output.Number(kMagic) || !output.Number(kBasisVersion) ||
      !PreparedStructureCodec::BasisFields(output, basis)) {
    return std::unexpected(output.Failure);
  }
  return std::move(output.Bytes);
}

std::optional<PreparedBuildingBasis> DecodePreparedBuildingBasis(std::span<const uint8_t> bytes,
                                                                 size_t residentBytesMost) {
  if (bytes.size() > kStructureArtifactBytesMost) { return std::nullopt; }
  Reader input(bytes);
  input.AllocationLeft = residentBytesMost;
  uint32_t magic = 0;
  uint32_t version = 0;
  PreparedBuildingBasis basis;
  if (!input.Number(magic) || magic != kMagic || !input.Number(version) ||
      version != kBasisVersion || !PreparedStructureCodec::BasisFields(input, basis) ||
      input.Remaining != 0 || !PreparedStructureCodec::ValidBasis(basis)) {
    return std::nullopt;
  }
  return basis;
}

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureTile(const PreparedStructureTile &tile) {
  for (const auto &surface : tile.Surfaces) {
    if (!surface.RequireShapes()) {
      return std::unexpected(StructureArtifactError::InvalidProduct);
    }
  }
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
  if (!input.Number(magic) || magic != kMagic || !input.Number(version) ||
      (version != kVersion && version != kLegacyVersion) ||
      !PreparedStructureCodec::TileFields(input, tile, version == kVersion) ||
      input.Remaining != 0 || !PreparedStructureCodec::Valid(tile)) {
    return std::nullopt;
  }
  return tile;
}

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodePreparedStructureIndex(const PreparedStructureTile &tile) {
  if (!PreparedStructureCodec::Valid(tile)) {
    return std::unexpected(StructureArtifactError::InvalidProduct);
  }
  Writer output;
  if (!output.Number(kMagic) || !output.Number(kIndexVersion) ||
      !PreparedStructureCodec::IndexFields(output, tile)) {
    return std::unexpected(output.Failure);
  }
  return std::move(output.Bytes);
}

std::optional<PreparedStructureTile> DecodePreparedStructureIndex(std::span<const uint8_t> bytes,
                                                                  size_t residentBytesMost) {
  if (bytes.size() > kStructureArtifactBytesMost) { return std::nullopt; }
  Reader input(bytes);
  input.AllocationLeft = residentBytesMost;
  uint32_t magic = 0;
  uint32_t version = 0;
  PreparedStructureTile tile;
  if (!input.Number(magic) || magic != kMagic || !input.Number(version) ||
      (version != kIndexVersion && version != kLegacyIndexVersion) ||
      !PreparedStructureCodec::IndexFields(input, tile, version == kIndexVersion) ||
      input.Remaining != 0 || !PreparedStructureCodec::Valid(tile)) {
    return std::nullopt;
  }
  return tile;
}

std::expected<std::vector<uint8_t>, StructureArtifactError>
EncodeBuildingSurfaceBlock(std::span<const BuildingSurface *const> surfaces) {
  Writer output;
  if (!output.Number(kBlockMagic) || !output.Number(uint64_t{surfaces.size()})) {
    return std::unexpected(output.Failure);
  }
  for (const auto *surface : surfaces) {
    if (surface == nullptr || !surface->RequireShapes() ||
        !PreparedStructureCodec::ValidSurface(*surface) ||
        !PreparedStructureCodec::SurfaceFields(output, *surface)) {
      return std::unexpected(StructureArtifactError::InvalidProduct);
    }
  }
  return std::move(output.Bytes);
}

std::optional<std::vector<BuildingSurface>>
DecodeBuildingSurfaceBlock(std::span<const uint8_t> bytes, size_t residentBytesMost) {
  if (bytes.size() > kStructureArtifactBytesMost) { return std::nullopt; }
  Reader input(bytes);
  input.AllocationLeft = residentBytesMost;
  uint32_t magic = 0;
  std::vector<BuildingSurface> surfaces;
  if (!input.Number(magic) || (magic != kBlockMagic && magic != kLegacyBlockMagic) ||
      !input.List(surfaces,
                  [magic](auto &held, auto &surface) {
                    return PreparedStructureCodec::SurfaceFields(
                               held, surface, magic == kBlockMagic) &&
                           PreparedStructureCodec::ValidSurface(surface);
                  }) ||
      input.Remaining != 0) {
    return std::nullopt;
  }
  return surfaces;
}

bool CompatiblePreparedSurface(const BuildingSurface &header,
                               const BuildingSurface &model) noexcept {
  if (header.MinimumHeightM_ != model.MinimumHeightM_ ||
      header.WallColour_.has_value() != model.WallColour_.has_value()) {
    return false;
  }
  if (header.FaceCount() != model.FaceCount() ||
      header.SupportsProjection() != model.SupportsProjection()) {
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (header.WallColour_ && (*header.WallColour_)[axis] != (*model.WallColour_)[axis]) {
      return false;
    }
    if (header.Origin()[axis] != model.Origin()[axis] ||
        header.Axes().East[axis] != model.Axes().East[axis] ||
        header.Axes().North[axis] != model.Axes().North[axis] ||
        header.Axes().Up[axis] != model.Axes().Up[axis] ||
        header.Bounds().Min[axis] != model.Bounds().Min[axis] ||
        header.Bounds().Max[axis] != model.Bounds().Max[axis]) {
      return false;
    }
  }
  return true;
}
}
