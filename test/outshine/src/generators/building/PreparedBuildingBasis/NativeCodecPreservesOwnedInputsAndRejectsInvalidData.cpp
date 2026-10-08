#include "PreparedStructureCodec.h"
#include "Check.h"
#include <memory>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  PreparedBuildingBasis original;
  original.TileSpanM = 1000;
  original.PointsLatLon = {47, 9, 47, 9.001, 47.001, 9.001, 47.001, 9};
  original.Sources = {{.Cell = 1, .Id = {.Id = 73, .Kind = 2}},
                      {.Cell = 64, .Id = {.Id = 19, .Kind = 3}}};
  original.Origin.Provenance = std::make_shared<Data::SourceProvenance>(
      Data::SourceProvenance{.DatasetId = "basis", .Revision = "r1", .PayloadSha256 = {}});
  const auto encoded = EncodePreparedBuildingBasis(original);
  CHECK(encoded.has_value(), "native basis encodes without plans or prepared surfaces");
  if (!encoded) { return Report(); }
  const auto decoded = DecodePreparedBuildingBasis(*encoded, 1024 * 1024);
  CHECK(decoded && decoded->PointsLatLon == original.PointsLatLon && decoded->Sources.size() == 2 &&
            decoded->Sources[0].Cell == 1 && decoded->Sources[1].Cell == 64 &&
            decoded->Sources[0].Id == original.Sources[0].Id &&
            decoded->Sources[1].Id == original.Sources[1].Id && decoded->Origin.Provenance &&
            *decoded->Origin.Provenance == *original.Origin.Provenance,
        "cell selection, source identity and owned coordinates survive native delivery");
  CHECK(!DecodePreparedBuildingBasis(*encoded, 1), "decode enforces the residency budget");
  for (size_t prefix = 0; prefix < encoded->size(); ++prefix) {
    CHECK(!DecodePreparedBuildingBasis(std::span(*encoded).first(prefix), 1024 * 1024),
          "partial native products cannot become ready");
  }
  auto corrupt = *encoded;
  corrupt[4] ^= 0x40;
  CHECK(!DecodePreparedBuildingBasis(corrupt, 1024 * 1024), "unknown versions are misses");
  corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!DecodePreparedBuildingBasis(corrupt, 1024 * 1024), "trailing bytes are rejected");
  original.Sources[0].Cell = 0;
  CHECK(!EncodePreparedBuildingBasis(original), "invalid hierarchy references never publish");
  original.Sources[0].Cell = 1;
  original.PointsLatLon[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodePreparedBuildingBasis(original), "nonfinite inputs never publish");
  return Report();
}
