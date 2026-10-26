#ifndef OUTSHINE_TEST_HOCKENHEIMCIRCUITFIXTURE_H
#define OUTSHINE_TEST_HOCKENHEIMCIRCUITFIXTURE_H

#include "Earth.h"
#include "Extent.h"
#include "GeographicBounds.h"
#include "SourceProvider.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace outshine::Test::HockenheimFixture {
inline constexpr const char *SourcePath = "test/outshine/data/osm/HockenheimringGrandPrix.osm";
inline constexpr uint64_t RelationId = 284588;
inline constexpr size_t EdgeCount = 267;
inline constexpr size_t NodeCount = 319;
inline constexpr size_t WayCount = 17;
inline constexpr size_t MainWayCount = 16;
inline constexpr double LatitudeDeg = 49.3274;
inline constexpr double LongitudeDeg = 8.5659;
inline constexpr double MinimumReportedBytes = 30000.0;
inline constexpr double TrackWidthM = 12.0;
inline constexpr double FlatHeightM = 100.0;
inline constexpr GeographicBounds Bounds{
    .WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34};
inline constexpr Extent Frame{.WidthPx = 160, .HeightPx = 90};

[[nodiscard]] inline Data::SourceProvider LocalSource() {
  Data::SourceProvider source;
  source.Kind = "osm";
  source.Dataset = "openstreetmap";
  source.Revision = "pin-r1";
  source.Missing = Data::MissingDataPolicy::Fail;
  source.Location = std::filesystem::absolute(SourcePath).string();
  source.Coverage = Bounds;
  return source;
}
}
#endif
