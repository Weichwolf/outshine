#include "OsmStringIndex.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Generators::Osm;

void Relocate() {
  OsmStringIndex index;
  std::vector<std::string> pool;
  constexpr uint32_t entries = 2048;
  for (uint32_t at = 0; at < entries; ++at) {
    CHECK(index.Intern(pool, "k" + std::to_string(at)) == at,
          "native pool IDs follow first appearance through repeated growth");
  }
  OsmStringIndex restored;
  auto decodedPool = pool;
  CHECK(restored.Intern(decodedPool, "k7") == 7 && decodedPool == pool &&
            restored.CapacityBytes() <= OsmStringIndex::PeakBytes(decodedPool.size()),
        "an unindexed decoded pool rebuilds without reserving for an absent extra entry");
  const auto bytes = index.CapacityBytes();
  for (uint32_t at = 0; at < entries; ++at) {
    CHECK(index.Intern(pool, "k" + std::to_string(at)) == at,
          "relocated short strings keep the same native identity");
  }
  CHECK(pool.size() == entries && index.CapacityBytes() == bytes,
        "duplicate queries allocate neither extra pool entries nor a larger index");
  auto copied = index;
  auto copiedPool = pool;
  CHECK(copied.Intern(copiedPool, "k7") == 7 && copied.Intern(copiedPool, "other") == entries,
        "copied pool/index pairs own their references independently");
  auto moved = std::move(copied);
  CHECK(moved.Intern(copiedPool, "other") == entries && pool.size() == entries,
        "moving an index preserves IDs without mutating the original pool");
  CHECK(bytes <= OsmStringIndex::PeakBytes(entries),
        "the slot peak accounts for the actual retained index capacity");
}

void RebuildAndBinaryText() {
  OsmStringIndex index;
  std::vector<std::string> pool{"existing", "existing"};
  CHECK(index.Intern(pool, "existing") == 0,
        "a decoded pool without an index reuses the first existing entry");
  CHECK(index.Intern(pool, "") == 2 && index.Intern(pool, "") == 2,
        "empty native strings are interned without an empty-slot ambiguity");
  const std::string binary("a\0b", 3);
  CHECK(index.Intern(pool, binary) == 3 && index.Intern(pool, "a") == 4 &&
            index.Intern(pool, binary) == 3,
        "binary strings retain their full length and distinct identity");
  const std::string_view part(pool[0].data() + 1, pool[0].size() - 1);
  CHECK(index.Intern(pool, part) == 5 && pool.back() == "xisting",
        "an inserted substring survives relocation of its source pool");
  index.Clear();
  CHECK(index.CapacityBytes() == 0,
        "clearing the index releases old slots before a new pool lifetime is admitted");
  pool.clear();
  CHECK(index.Intern(pool, "restart") == 0 && pool.size() == 1,
        "clearing both owners starts a consistent new ID space");
  CHECK(OsmStringIndex::PeakBytes(std::numeric_limits<size_t>::max()) ==
            std::numeric_limits<size_t>::max(),
        "an unrepresentable preflight count saturates instead of wrapping");
}
}

int main() {
  Relocate();
  RebuildAndBinaryText();
  return outshine::Test::Report();
}
