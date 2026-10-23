#include "Check.h"
#include "UrbanPrismAsset.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  namespace Osm = Generators::Osm;
  const std::string source = Sha256Hex("complete native area product");
  std::array<uint8_t, 32> terrain = Sha256Digest("published terrain", 17);
  std::array<Osm::UrbanPatchRange, 2> patches{{{.Column = 4, .Row = 8, .Columns = 8, .Rows = 8},
                                               {.Column = 12, .Row = 8, .Columns = 4, .Rows = 8}}};
  const auto original = Osm::UrbanPrismRequestKey(source, terrain, patches, 3);
  CHECK(original && original->size() == 64 &&
            original->find_first_not_of("0123456789abcdef") == std::string::npos,
        "complete native dependencies produce a persistent request identity");
  CHECK(Osm::UrbanPrismRequestKey(std::string(source), terrain, patches, 3) == original,
        "equal content and selection reproduce the request independently of object addresses");
  CHECK(Osm::UrbanPrismRequestKey(
            Sha256Hex("changed source recipe or evidence"), terrain, patches, 3) != original,
        "a different complete source product cannot reuse old placed geometry");
  terrain[0] ^= 1;
  CHECK(Osm::UrbanPrismRequestKey(source, terrain, patches, 3) != original,
        "changed published surface content invalidates the dependent prism product");
  terrain[0] ^= 1;
  CHECK(Osm::UrbanPrismRequestKey(source, terrain, patches, 4) != original,
        "different spatial LOD index is a different persisted product");
  std::swap(patches[0], patches[1]);
  CHECK(Osm::UrbanPrismRequestKey(source, terrain, patches, 3) != original,
        "patch order binds the packed sample and placement order");
  std::swap(patches[0], patches[1]);
  ++patches[0].Column;
  CHECK(Osm::UrbanPrismRequestKey(source, terrain, patches, 3) != original,
        "different selected cells cannot collide despite equal sample counts");
  CHECK(!Osm::UrbanPrismRequestKey("unknown", terrain, patches, 3) &&
            !Osm::UrbanPrismRequestKey(std::string(64, 'A'), terrain, patches, 3) &&
            !Osm::UrbanPrismRequestKey(source, terrain, {}, 3),
        "unbound source keys and absent demand are refused before work starts");
  patches[0].Columns = 0;
  CHECK(!Osm::UrbanPrismRequestKey(source, terrain, patches, 3), "empty ranges are invalid");
  patches[0] = {.Column = std::numeric_limits<uint32_t>::max(), .Columns = 1, .Rows = 1};
  CHECK(!Osm::UrbanPrismRequestKey(source, terrain, patches, 3), "range overflow is refused");
  patches[0] = {.Columns = std::numeric_limits<uint32_t>::max(),
                .Rows = std::numeric_limits<uint32_t>::max()};
  CHECK(!Osm::UrbanPrismRequestKey(source, terrain, patches, 3),
        "sample demand is bounded without allocating or generating geometry");
  return Report();
}
