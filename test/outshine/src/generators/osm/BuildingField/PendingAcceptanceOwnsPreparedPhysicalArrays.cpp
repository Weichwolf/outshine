#include "BuildingField.h"
#include "Check.h"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {
constexpr auto kChangedSpreadM = 77;
constexpr auto kChangedAcrossM = 88;
constexpr auto kChangedHeightM = 99;
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Field = Generators::Osm::BuildingField;
  auto geometry = std::make_shared<Ground::BuildingGeometry>();
  geometry->OwnCoordinates({0, 0, 0, 1, 1, 1}, {});
  const std::weak_ptr<const Ground::BuildingGeometry> coordinates = geometry;
  auto contacts = std::make_shared<const std::vector<Ground::BuildingTerrainPart>>(
      std::vector<Ground::BuildingTerrainPart>{{.HeightAslM = 12,
                                                .Ring = {{.EastM = 0, .NorthM = 0},
                                                         {.EastM = 1, .NorthM = 0},
                                                         {.EastM = 1, .NorthM = 1}},
                                                .Holes = {}}});
  const std::weak_ptr<const std::vector<Ground::BuildingTerrainPart>> parts = contacts;
  std::array<Ground::BuildingFootprint, 1> prints{{{.PointCount = 3,
                                                    .HeightM = 12,
                                                    .Street = {},
                                                    .TerrainCeiling = {},
                                                    .TerrainContacts = {.Parts = contacts}}}};
  std::array<double, 1> spread{2};
  std::array<double, 1> across{9};
  Field field;
  field.AnchorAt({{0, 0, 0}});
  Field::Baked baked{
      .Coordinates = geometry, .Prints = prints, .SeatSpreadM = spread, .AcrossM = across};
  field.Take(7);
  auto pending = field.PrepareAcceptance(7, baked);
  geometry.reset();
  contacts.reset();
  baked.Coordinates.reset();
  prints.front().HeightM = kChangedHeightM;
  prints.front().TerrainContacts = {};
  spread.front() = kChangedSpreadM;
  across.front() = kChangedAcrossM;
  CHECK(field.FootprintCount() == 0 && !coordinates.expired() && !parts.expired(),
        "pending acceptance owns physical buffers, contacts and geometry without publishing them");
  field.CommitAcceptance(std::move(pending), baked);
  CHECK(field.OfTile(7).front().HeightM == 12 && field.ProductAt(0).SeatSpreadM.front() == 2 &&
            field.ProductAt(0).AcrossM.front() == 9 &&
            field.OfTile(7).front().TerrainContacts.Valid(),
        "noexcept commit consumes the complete product captured before source buffers changed");
  CHECK(field.GeometryOfFootprint(0) != nullptr && field.GeometryOfFootprint(1) == nullptr,
        "global geometry lookup resolves only accepted footprint indices");
  field.ResetDerived();
  CHECK(coordinates.expired() && parts.expired(),
        "retiring the final accepted owner releases captured geometry and contacts");
  const auto before = field.Revision();
  const auto token = field.ReservationOwner();
  auto abandonedGeometry = std::make_shared<Ground::BuildingGeometry>();
  const std::weak_ptr<const Ground::BuildingGeometry> abandonedOwner = abandonedGeometry;
  baked.Coordinates = abandonedGeometry;
  std::optional<Field::PendingAcceptance> abandoned{field.PrepareAcceptance(9, baked)};
  abandonedGeometry.reset();
  baked.Coordinates.reset();
  CHECK(abandoned && !abandonedOwner.expired() && field.FootprintCount() == 0 &&
            field.Revision() == before && field.ReservationOwner() == token,
        "a prepared unpublished product owns its inputs without altering revision or reservations");
  abandoned.reset();
  CHECK(abandonedOwner.expired() && field.AcceptedTiles().empty() && field.Revision() == before,
        "abandoning preparation releases its inputs and publishes no physical or source state");
  return Report();
}
