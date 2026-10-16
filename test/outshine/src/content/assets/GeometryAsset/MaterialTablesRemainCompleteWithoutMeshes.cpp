#include "content/GeometryAsset.h"
#include "Check.h"
#include <cstddef>
#include <optional>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::size_t budget = 4096;
  Geometry tables;
  CHECK(tables.addSurface("ground", Material{}), "a terrain material does not require a mesh part");
  CHECK(!tables.wellFormed(), "a material table alone remains unsuitable for direct mesh drawing");
  const auto bytes = EncodeGeometryAsset(tables, budget);
  CHECK(bytes, "a complete material table is nevertheless a valid native game asset");
  auto restored = bytes ? DecodeGeometryAsset(*bytes, budget) : std::nullopt;
  CHECK(restored && restored->surfaces() == 1 && restored->parts() == 0 &&
            restored->surfaceNameOf(0) == "ground",
        "terrain material identity survives a mesh-free native round trip");
  Material missingImage;
  missingImage.BaseColourMap.Image = 0;
  Geometry unresolved;
  CHECK(unresolved.addSurface("unresolved", missingImage), "source assembly can await its image");
  CHECK(!EncodeGeometryAsset(unresolved, budget),
        "unfinished material references cannot enter the cache");
  if (restored) {
    const auto unfinished = restored->addPart("unfinished", MaterialInstance(0));
    CHECK(unfinished && !EncodeGeometryAsset(*restored, budget),
          "a nonempty mesh still requires complete valid geometry");
  }
  return Report();
}
