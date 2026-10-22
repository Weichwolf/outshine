#include "Check.h"
#include "PrismFixture.h"
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Osm = Generators::Osm;
  const auto input = UrbanPrismFixture::Product();
  const auto encoded = Osm::EncodeUrbanPrisms(input);
  CHECK(encoded, "ready samples, independent frames and measured deviations can be stored");
  if (!encoded) { return Report(); }
  const auto restored = Osm::DecodeUrbanPrisms(*encoded);
  CHECK(restored && restored->Placements.size() == 2,
        "a decoder with no provider, terrain or generator can restore complete native products");
  if (!restored) { return Report(); }
  CHECK(restored->Geometry.Samples->size() == input.Geometry.Samples->size(),
        "shared sample storage remains shared instead of expanding reused patch slices");
  for (size_t index = 0; index < input.Placements.size(); ++index) {
    CHECK(restored->Placements[index].EcefFromLocal == input.Placements[index].EcefFromLocal &&
              restored->Placements[index].MaxCornerErrorM ==
                  input.Placements[index].MaxCornerErrorM,
          "static double ECEF placement and numerical deviation survive without camera inputs");
  }
  const auto reencoded = Osm::EncodeUrbanPrisms(*restored);
  CHECK(reencoded && *reencoded == *encoded,
        "the canonical little-endian product is bit-preserving");
  const auto empty = Osm::EncodeUrbanPrisms({});
  CHECK(empty && Osm::DecodeUrbanPrisms(*empty), "no selected demand has a finite empty encoding");
  return Report();
}
