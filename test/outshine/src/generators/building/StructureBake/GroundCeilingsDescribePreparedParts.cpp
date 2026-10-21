#include "BuildingMesh.h"
#include "BuildingShape.h"
#include "Check.h"
#include "Geodesy.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructureTile.h"
#include "Sha256.h"
#include "StructureArtifact.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Fixture {
  RawTile Raw;
  std::shared_ptr<const Ground::HeightField> Heights;
};

Fixture Inputs() {
  Fixture fixture;
  const auto at = Ground::HeightField::SpotOf({.LongitudeDeg = 9.003, .LatitudeDeg = 47.002}, 18);
  Ground::HeightField::Block block;
  block.At = at;
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 110, 120, 130};
  fixture.Heights = Ground::HeightField::Of(at.Zoom, {block});
  const auto position = [&](double x, double y) {
    return Ground::TileFracToGeo(
        {.X = static_cast<double>(at.X) + x, .Y = static_cast<double>(at.Y) + y}, at.Zoom);
  };
  const auto northwest = position(0, 0);
  const auto southeast = position(1, 1);
  const Ground::GeoBounds bounds{.MinLonDeg = northwest.LongitudeDeg,
                                 .MinLatDeg = southeast.LatitudeDeg,
                                 .MaxLonDeg = southeast.LongitudeDeg,
                                 .MaxLatDeg = northwest.LatitudeDeg};
  GeoToEcef(position(0.5, 0.5), fixture.Raw.AnchorEcef);
  fixture.Raw.TileSpanM = 200;
  for (uint32_t index = 0; index < 3; ++index) {
    const double x = index == 0 ? 0.1 : 0.6;
    const double y = index == 2 ? 0.6 : 0.1;
    const double width = index == 0 ? 0.4 : 0.3;
    const uint32_t first = static_cast<uint32_t>(fixture.Raw.LatLon.size() / 2u);
    for (const auto &point : std::array{position(x, y),
                                        position(x + width, y),
                                        position(x + width, y + 0.3),
                                        position(x, y + 0.3)}) {
      fixture.Raw.LatLon.push_back(point.LatitudeDeg);
      fixture.Raw.LatLon.push_back(point.LongitudeDeg);
    }
    const auto ring = std::span(fixture.Raw.LatLon).subspan(static_cast<size_t>(first) * 2u, 8);
    const auto cell = StructureCellOf(bounds, ring);
    CHECK(cell.has_value(), "source belongs to a canonical spatial cell");
    if (!cell) { return {}; }
    fixture.Raw.Structures.push_back(
        {.LocalFirst = first,
         .PointCount = 4,
         .SourceFirst = first,
         .Cell = *cell,
         .HeightM = 12,
         .MinimumHeightM = index == 1 ? -3.0 : (index == 2 ? 3.0 : 0.0),
         .Pitched = 0});
  }
  return fixture;
}

void CheckPhysicalPlane(const PreparedStructureTile &base) {
  const auto &contact = base.Structures.front().Standing.TerrainCeiling;
  CHECK(contact.has_value(), "ordinary grounded model supplies a terrain ceiling");
  if (!contact) { return; }
  CHECK(contact->OriginPoint == base.Structures.front().Layout.LocalFirst,
        "the model's source frame owns the ceiling origin");
  CHECK(contact->HeightAslM > base.Structures.front().Standing.SeatM + 0.25,
        "the sloped model ceiling is distinct from the old mean-height stamp");
  const size_t point = static_cast<size_t>(contact->OriginPoint) * 2u;
  Vec3 origin;
  GeoToEcef({.LongitudeDeg = base.PointsLatLon[point + 1u],
             .LatitudeDeg = base.PointsLatLon[point],
             .HeightM = contact->HeightAslM},
            origin);
  const auto &surface = base.Surfaces.front();
  const auto &axes = surface.Axes();
  double least = std::numeric_limits<double>::infinity();
  size_t contactIndex = 0;
  for (const auto &part : surface.Shapes()) {
    if (!part.OnGround()) { continue; }
    CHECK(contactIndex < base.Structures.front().Standing.TerrainContacts.PartsView().size(),
          "each standing part has its own stored contact");
    if (contactIndex >= base.Structures.front().Standing.TerrainContacts.PartsView().size()) {
      return;
    }
    const auto &piece =
        base.Structures.front().Standing.TerrainContacts.PartsView()[contactIndex++];
    CHECK(piece.Ring == part.Ring && piece.Holes == part.Holes &&
              std::abs(piece.HeightAslM -
                       (base.Structures.front().BaseAslM + part.SeatM - kBuildingPlinthM)) < 1e-9,
          "stored contacts preserve the actual spatial ownership and physical standing plane");
    for (const auto &corner : part.Ring) {
      const Vec3 floor = base.AnchorEcef + surface.Origin() + axes.East * corner.EastM +
                         axes.North * corner.NorthM + axes.Up * part.SeatM;
      const double clearance = Dot(floor - origin, axes.Up);
      CHECK(clearance >= kBuildingPlinthM - 1e-6,
            "the physical terrain plane leaves every grounded part's recipe plinth clear");
      least = std::min(least, clearance);
    }
  }
  CHECK(std::abs(least - kBuildingPlinthM) < 1e-6,
        "the lowest actual ground floor determines the conservative ceiling");
  CHECK(contactIndex == base.Structures.front().Standing.TerrainContacts.PartsView().size() &&
            base.Structures.front().Standing.TerrainContacts.Valid() &&
            base.Structures.front().Standing.HeapBytes() > 0,
        "metadata contains exactly the standing parts and accounts for their retained payload");
  CHECK(!base.Structures[1].Standing.TerrainCeiling && !base.Structures[2].Standing.TerrainCeiling,
        "basement and elevated recipes retain their existing contact treatment");
}
}

int main() {
  auto fixture = Inputs();
  if (!fixture.Heights) { return Report(); }
  const auto base = PrepareStructureTile(fixture.Raw, *fixture.Heights);
  CHECK(base && base->Structures.size() == 3, "all recipe classes prepare from the sloping DEM");
  if (!base || base->Structures.size() != 3) { return Report(); }
  CheckPhysicalPlane(*base);
  const auto bytes = EncodePreparedStructureIndex(*base);
  const auto header =
      bytes ? DecodePreparedStructureIndex(*bytes, size_t{8} * 1024 * 1024) : std::nullopt;
  CHECK(header &&
            header->Structures.front().Standing.TerrainContacts ==
                base->Structures.front().Standing.TerrainContacts &&
            header->Surfaces.front().PreparedSelection() &&
            header->Structures.front().Standing.TerrainCeiling ==
                base->Structures.front().Standing.TerrainCeiling,
        "source-frame contacts survive a metadata-only native cache restart");
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Shell;
  const auto product = BakePreparedStructures(*base, view, mesher, *scratch);
  CHECK(product && product->Prints.size() == 3, "render products carry the same source contacts");
  if (!product) { return Report(); }
  CHECK(product->Prints.front().TerrainContacts.Parts ==
            base->Structures.front().Standing.TerrainContacts.Parts,
        "LOD products share immutable contact polygons instead of copying them");
  const auto key = Sha256Hex("ground-contact-asset");
  const auto artifact = EncodeStructureArtifact(*product, key);
  const auto restored = artifact ? DecodeStructureArtifact(*artifact, key, 1) : std::nullopt;
  CHECK(restored && restored->Prints == product->Prints,
        "LOD cache hits retain the contact without rebuilding the source model");
  auto invalid = *base;
  invalid.Structures.front().Standing.TerrainCeiling->OriginPoint = UINT32_MAX;
  CHECK(!EncodePreparedStructureIndex(invalid), "dangling contact origins cannot publish");
  invalid = *base;
  invalid.Structures.front().Standing.TerrainCeiling->HeightAslM =
      std::numeric_limits<double>::infinity();
  CHECK(!EncodePreparedStructureIndex(invalid), "nonfinite contact heights cannot publish");
  invalid = *base;
  auto parts = *invalid.Structures.front().Standing.TerrainContacts.Parts;
  parts.front().Ring.front().EastM = std::numeric_limits<double>::infinity();
  invalid.Structures.front().Standing.TerrainContacts.Parts =
      std::make_shared<const std::vector<Ground::BuildingTerrainPart>>(std::move(parts));
  CHECK(!EncodePreparedStructureIndex(invalid), "nonfinite contact polygons cannot publish");
  return Report();
}
