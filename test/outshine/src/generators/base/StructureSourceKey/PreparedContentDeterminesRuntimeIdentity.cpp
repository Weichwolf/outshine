#include "StructureSourceKey.h"
#include "Check.h"

#include <optional>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::optional<Data::TileSourceIdentity> vector;
  StructureSourceView inputs{.Vector = vector,
                             .HeightSources = {},
                             .HeightDigest = 17,
                             .PreparedAssetKey = "native-content-a"};
  const auto first = StructureSourceKey(inputs);
  inputs.StreetDigest = 99;
  inputs.TileSpanM = 999;
  CHECK(StructureSourceKey(inputs) == first,
        "a prepared content key already binds source preparation and generation parameters");
  inputs.PreparedAssetKey = "native-content-b";
  CHECK(StructureSourceKey(inputs) != first, "changed native content invalidates runtime details");
  inputs.PreparedAssetKey = "native-content-a";
  inputs.HeightDigest = 18;
  CHECK(StructureSourceKey(inputs) != first, "changed captured terrain invalidates native details");
  inputs.HeightDigest = 17;
  inputs.FallbackHeights = true;
  CHECK(StructureSourceKey(inputs) != first, "fallback content cannot impersonate qualified input");
  return Report();
}
