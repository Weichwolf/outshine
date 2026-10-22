#include "Check.h"
#include "PrismFixture.h"
#include <cstddef>
#include <limits>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Osm = Generators::Osm;
  auto product = UrbanPrismFixture::Product();
  const auto encoded = Osm::EncodeUrbanPrisms(product);
  CHECK(encoded, "the independent fixture produces a valid stored product");
  if (!encoded) { return Report(); }
  for (size_t length = 0; length < encoded->size(); ++length) {
    CHECK(!Osm::DecodeUrbanPrisms(std::span(*encoded).first(length)),
          "every truncated record is refused rather than partially restored");
  }
  CHECK(!Osm::EncodeUrbanPrisms(product, encoded->size() - 1) &&
            !Osm::DecodeUrbanPrisms(*encoded, encoded->size() - 1),
        "both cache directions enforce the caller's byte budget");
  auto corrupt = *encoded;
  corrupt.push_back(0);
  CHECK(!Osm::DecodeUrbanPrisms(corrupt), "trailing bytes cannot hide an inconsistent extent");
  corrupt = *encoded;
  corrupt[0] ^= 1;
  CHECK(!Osm::DecodeUrbanPrisms(corrupt), "an unknown format cannot enter native geometry");
  corrupt = *encoded;
  for (size_t index = 8; index < 12; ++index) { corrupt[index] = 255; }
  CHECK(!Osm::DecodeUrbanPrisms(corrupt), "hostile counts are rejected before sample allocation");
  corrupt = *encoded;
  corrupt[16] = 0;
  corrupt[17] = 0;
  corrupt[18] = 192;
  corrupt[19] = 127;
  CHECK(!Osm::DecodeUrbanPrisms(corrupt),
        "a stored NaN in a referenced sample fails the shared native validation");
  product.Geometry.Patches.front().FirstSample = 5;
  CHECK(!Osm::EncodeUrbanPrisms(product), "encoding shares the renderer's CPU range validation");
  product = UrbanPrismFixture::Product();
  product.Placements.pop_back();
  CHECK(!Osm::EncodeUrbanPrisms(product), "each stored patch requires one static placement");
  product = UrbanPrismFixture::Product();
  product.Placements.front().EcefFromLocal.At(0, 0) = 0;
  CHECK(!Osm::EncodeUrbanPrisms(product), "singular placement cannot become a ray-query asset");
  product = UrbanPrismFixture::Product();
  product.Placements.front().MaxCornerErrorM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!Osm::EncodeUrbanPrisms(product), "invalid approximation metadata cannot be serialized");
  return Report();
}
