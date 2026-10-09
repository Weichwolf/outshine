#include "OsmStructureDescription.h"
#include "OsmSourceProvenance.h"
#include "OsmSourceCapture.h"
#include "OsmBuildingFacade.h"

#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {

constexpr double kLongitudePeriodDeg = 360.0;

Ground::StructureOpenings OpeningsOf(std::optional<FacadeStyle> facade) {
  if (!facade) { return Ground::StructureOpenings::Unspecified; }
  switch (*facade) {
    case FacadeStyle::Glazing: return Ground::StructureOpenings::Glazed;
    case FacadeStyle::House:
    case FacadeStyle::Terrace:
    case FacadeStyle::Block: return Ground::StructureOpenings::Regular;
    default: return Ground::StructureOpenings::Closed;
  }
}

std::expected<GeographicRing, StructureDescriptionError> PointRing(
    outshine::Ground::StructureFootprints &raw, const StructurePolicy &policy, uint32_t point) {
  const double widthM = policy.PointWidthM;
  if (!std::isfinite(widthM) || widthM <= 0.0) {
    return std::unexpected(StructureDescriptionError::InvalidPointPolicy);
  }
  const size_t pointAt = static_cast<size_t>(point) * 2;
  const auto frame = TangentFrame::At(
      {.LongitudeDeg = raw.LatLon[pointAt + 1], .LatitudeDeg = raw.LatLon[pointAt]});
  const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
  const double half = widthM / 2.0;
  for (const auto corner : std::array<EastNorth, 4>{{{.EastM = -half, .NorthM = -half},
                                                     {.EastM = half, .NorthM = -half},
                                                     {.EastM = half, .NorthM = half},
                                                     {.EastM = -half, .NorthM = half}}}) {
    const auto at = frame.ApproximateGeographicAt(corner);
    raw.LatLon.push_back(at.LatitudeDeg);
    raw.LatLon.push_back(std::remainder(at.LongitudeDeg, kLongitudePeriodDeg));
  }
  return GeographicRing{.First = first, .Count = 4, .Exterior = true};
}

}

std::expected<StructureDescription, StructureDescriptionError>
DescribeStructures(const BuildingFootprints &buildings,
                   const std::shared_ptr<const SourceSnapshot> &source,
                   Data::ProductOrigin origin,
                   StructurePolicy policy) {
  if (source.get() != &buildings.Source()) {
    return std::unexpected(StructureDescriptionError::SourceMismatch);
  }
  policy.PointsMost =
      std::min(policy.PointsMost, static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  if (buildings.Points().size() / 2 > policy.PointsMost) {
    return std::unexpected(StructureDescriptionError::PointBudgetExceeded);
  }
  outshine::Ground::StructureFootprints raw;
  raw.Origin = std::move(origin);
  raw.LatLon.assign(buildings.Points().begin(), buildings.Points().end());
  raw.Rings.assign(buildings.Rings().begin(), buildings.Rings().end());
  raw.Structures.reserve(buildings.Buildings().size());
  for (const auto &building : buildings.Buildings()) {
    const auto height = buildings.Heights(building).Resolve(policy.Heights);
    if (!height) { return std::unexpected(StructureDescriptionError::InvalidHeight); }
    GeographicRing ring;
    uint32_t firstHole = 0;
    uint32_t holeCount = 0;
    if (building.PointIndex) {
      if (policy.PointsMost - raw.LatLon.size() / 2 < 4) {
        return std::unexpected(StructureDescriptionError::PointBudgetExceeded);
      }
      const auto generated = PointRing(raw, policy, *building.PointIndex);
      if (!generated) { return std::unexpected(generated.error()); }
      ring = *generated;
    } else {
      const auto rings = buildings.Rings().subspan(building.FirstRing, building.RingCount);
      if (rings.empty() || !rings.front().Exterior ||
          std::ranges::any_of(rings.subspan(1), [](const auto &hole) { return hole.Exterior; })) {
        return std::unexpected(StructureDescriptionError::UnassignedCourtyard);
      }
      ring = rings.front();
      firstHole = static_cast<uint32_t>(building.FirstRing + 1);
      holeCount = static_cast<uint32_t>(building.RingCount - 1);
    }
    const auto openings = OpeningsOf(ReadBuildingFacade(buildings.Tags(building)));
    raw.Structures.push_back(
        {.First = ring.First,
         .Count = ring.Count,
         .FirstHole = firstHole,
         .HoleCount = holeCount,
         .Height = *height,
         .Source = {.Id = building.Source.Id, .Kind = static_cast<uint8_t>(building.Source.Kind)},
         .Openings = openings});
  }
  std::vector<ElementId> roots;
  roots.reserve(buildings.Buildings().size());
  for (const auto &building : buildings.Buildings()) { roots.push_back(building.Source); }
  auto selected = source->Elements.SelectReferenced(roots);
  if (!selected) { return std::unexpected(StructureDescriptionError::MissingReference); }
  raw.Origin.Provenance = DescribeOsmSource(*source);
  auto closure =
      std::make_shared<const SourceSnapshot>(SourceSnapshot{.Elements = std::move(*selected),
                                                            .Coverage = source->Coverage,
                                                            .SourceBytes = source->SourceBytes,
                                                            .ReadMs = source->ReadMs,
                                                            .ParseMs = source->ParseMs,
                                                            .Chunks = source->Chunks,
                                                            .Cell = source->Cell});
  return StructureDescription{.Footprints = std::move(raw),
                              .Source = std::make_shared<const SourceCapture>(std::move(closure)),
                              .Archive = source};
}

}
