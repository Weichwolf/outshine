#include "Check.h"
#include "FarPanorama.h"
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  FarPanorama product;
  product.Side = 2;
  product.AxialNearM = 700;
  product.AxialOverlapEndM = 900;
  product.MaximumRangeM = 240000;
  product.MaximumTranslationM = 22;
  product.ReferenceExposure = 1;
  product.Lighting.emplace().ToSun = {{0, 1, 0}};
  product.Revision.assign(64, '1');
  const auto texels = FarPanorama::FaceCount * product.Side * product.Side;
  product.Colour.resize(texels * FarPanorama::ColourBytesPerTexel);
  product.Depth.resize(texels * FarPanorama::DepthBytesPerTexel);

  const auto old = EncodeFarPanorama(product);
  CHECK(old.has_value(), "terrain-only cubemap remains cacheable");
  if (old) {
    const auto decoded = DecodeFarPanorama(*old);
    CHECK(decoded && decoded->BakedGroups.empty(), "older product cannot suppress mid groups");
  }

  const std::string first(64, 'a');
  const std::string second(64, 'b');
  product.BakedGroups = {first, second};
  const auto encoded = EncodeFarPanorama(product);
  CHECK(encoded.has_value(), "cubemap can name both baked revisions");
  if (encoded) {
    const auto decoded = DecodeFarPanorama(*encoded);
    CHECK(decoded && decoded->BakedGroups == product.BakedGroups,
          "reloading the cached cubemap preserves exact revision identities");
  }

  product.BakedGroups = {second, first};
  CHECK(!EncodeFarPanorama(product), "noncanonical key ordering rejected");
  product.BakedGroups = {first, first};
  CHECK(!EncodeFarPanorama(product), "duplicate group declarations rejected");
  product.BakedGroups = {first};
  product.AxialOverlapEndM = 0;
  CHECK(!EncodeFarPanorama(product), "substitution requires an overlap domain");
  return Report();
}
