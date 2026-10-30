#ifndef OUTSHINE_TEST_CANONICALGROUNDTERRAINPARTITIONFIXTURE_H
#define OUTSHINE_TEST_CANONICALGROUNDTERRAINPARTITIONFIXTURE_H
#include "Check.h"
#include "EarthworkDeformation.h"
#include "GroundPortBodies.h"
#include "Geocentric.h"
#include "TangentFrame.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <print>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Test::CanonicalGroundFixture {
struct TerrainPartitionInput {
  std::vector<EarthworkStamp> Contacts;
  std::vector<EastNorth> Points;
  std::vector<double> Heights;
};

inline TerrainPartitionInput PartitionInput(const Generators::GroundPortField &field,
                                            const Generators::GroundPortBodies &base) {
  TerrainPartitionInput result;
  Box bounds;
  for (const auto &body : base.Bodies) {
    result.Contacts.insert(
        result.Contacts.end(), body.TerrainContacts.begin(), body.TerrainContacts.end());
    for (const auto &point : body.Geometry.Points) {
      bounds.Cover(Vec3{{point[0], point[1], point[2]}});
    }
  }
  CHECK(!bounds.Empty() && bounds.Max[0] - bounds.Min[0] < 1024 &&
            bounds.Max[1] - bounds.Min[1] < 1024,
        "the original finite fixtures have an independently bounded one-metre DEM raster extent");
  if (bounds.Empty() || bounds.Max[0] - bounds.Min[0] >= 1024 ||
      bounds.Max[1] - bounds.Min[1] >= 1024) {
    return result;
  }
  const auto frame = TangentFrame::At(base.Anchor);
  const size_t columns = static_cast<size_t>(std::floor(bounds.Max[0] - bounds.Min[0] + 16)) + 1;
  const size_t rows = static_cast<size_t>(std::floor(bounds.Max[1] - bounds.Min[1] + 16)) + 1;
  for (size_t column = 0; column < columns; ++column) {
    const double east = bounds.Min[0] - 8 + static_cast<double>(column);
    for (size_t row = 0; row < rows; ++row) {
      const double north = bounds.Min[1] - 8 + static_cast<double>(row);
      auto geo = GeoFromEcef(frame.ToEcefPosition({.EastM = east, .NorthM = north, .UpM = 0}));
      if (!geo) { continue; }
      const auto height =
          field.Terrain().At({.LongitudeDeg = geo->LongitudeDeg, .LatitudeDeg = geo->LatitudeDeg});
      if (!height) { continue; }
      geo->HeightM = *height;
      result.Heights.push_back(frame.ToLocalPosition(*geo).UpM);
      result.Points.push_back({.EastM = east, .NorthM = north});
    }
  }
  return result;
}

inline void CheckContactMultiset(std::span<const EarthworkStamp> original,
                                 std::span<const EarthworkStamp> partition) {
  std::vector<EarthworkStamp> expected(original.begin(), original.end());
  std::vector<EarthworkStamp> actual(partition.begin(), partition.end());
  const auto key = [](const auto &stamp) {
    return std::tie(
        stamp.CorridorKey, stamp.RingEastNorthM, stamp.PlateauM, stamp.SlopeE, stamp.SlopeN);
  };
  std::ranges::sort(expected, {}, key);
  std::ranges::sort(actual, {}, key);
  CHECK(expected == actual,
        "all owners together retain every entire original terrain contact exactly once, with "
        "unchanged native parameters");
}

inline void CheckTerrainPartition(const Generators::GroundPortField &field,
                                  const Generators::GroundPortBodies &base,
                                  std::span<const EarthworkStamp> partition,
                                  size_t which) {
  auto input = PartitionInput(field, base);
  CheckContactMultiset(input.Contacts, partition);
  CHECK(!input.Points.empty(),
        "actual original DEM values cover the complete body and soft-apron comparison raster");
  if (input.Points.empty()) { return; }
  auto expected = input.Heights;
  EarthworkDeformationJob baseline(input.Contacts, input.Points, expected, 2);
  while (!baseline.Advance(256)) {}
  const auto reference = baseline.Take();
  std::vector<EarthworkStamp> contacts(partition.begin(), partition.end());
  for (const bool reverse : {false, true}) {
    if (reverse) { std::ranges::reverse(contacts); }
    auto actual = input.Heights;
    EarthworkDeformationJob job(contacts, input.Points, actual, 2);
    while (!job.Advance(256)) {}
    const auto result = job.Take();
    CHECK(
        result.Structures == reference.Structures && result.Held == reference.Held,
        "geographic ownership leaves native terrain/structure admission and held states unchanged");
    double maximum = 0;
    for (size_t at = 0; at < actual.size(); ++at) {
      maximum = std::max(maximum, std::abs(actual[at] - expected[at]));
      CHECK_NEAR(actual[at],
                 expected[at],
                 1e-8,
                 "m",
                 "the actual deformation consumer preserves every original DEM raster height "
                 "across owners and publication order");
    }
    std::println("GROUND_TERRAIN_PARTITION field={} samples={} contacts={} reverse={} "
                 "max_delta_m={} structures={} held={}",
                 which,
                 actual.size(),
                 contacts.size(),
                 reverse,
                 maximum,
                 result.Structures,
                 result.Held);
  }
}
}
#endif
