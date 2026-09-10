#include "OsmStorageUsage.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  using Field = size_t OsmStorageUsage::*;
  constexpr size_t unsignedLimit = std::numeric_limits<uint32_t>::max();
  constexpr size_t signedLimit = std::numeric_limits<int>::max();

  struct Boundary {
    Field Member;
    size_t Limit;
  };

  constexpr std::array<Boundary, 8> boundaries{
      {{&OsmStorageUsage::Features, signedLimit},
       {&OsmStorageUsage::Tiles, signedLimit},
       {&OsmStorageUsage::Rings, unsignedLimit},
       {&OsmStorageUsage::Points, std::min(unsignedLimit, std::numeric_limits<size_t>::max() / 2)},
       {&OsmStorageUsage::Tags, unsignedLimit},
       {&OsmStorageUsage::Values, unsignedLimit},
       {&OsmStorageUsage::Keys, unsignedLimit},
       {&OsmStorageUsage::Strings, unsignedLimit}}};
  for (const auto &boundary : boundaries) {
    OsmStorageUsage usage;
    usage.*boundary.Member = boundary.Limit - 1;
    OsmStorageUsage one;
    one.*boundary.Member = 1;
    CHECK(usage.TryAdd(one) && usage.*boundary.Member == boundary.Limit,
          "last representable pool element accepted");
    CHECK(!usage.TryAdd(one) && usage.*boundary.Member == boundary.Limit,
          "one past capacity rejected without mutation");
    CHECK(usage.TryAdd({}), "zero growth at capacity accepted");
    if (boundary.Limit < std::numeric_limits<size_t>::max()) {
      usage.*boundary.Member = boundary.Limit + 1;
      CHECK(!usage.TryAdd({}), "already invalid usage rejected");
    }
    usage.*boundary.Member = 1;
    one.*boundary.Member = std::numeric_limits<size_t>::max();
    CHECK(!usage.TryAdd(one) && usage.*boundary.Member == 1,
          "size_t overflow cannot wrap past capacity checks");
  }
  OsmStorageUsage usage{.Features = 3, .Strings = unsignedLimit};
  CHECK(!usage.TryAdd({.Features = 2, .Strings = 1}) && usage.Features == 3 &&
            usage.Strings == unsignedLimit,
        "late pool failure preserves every capacity counter");
  constexpr bool constexprResult = [] {
    OsmStorageUsage small;
    return small.TryAdd({.Features = 1, .Rings = 1, .Points = 2});
  }();
  static_assert(constexprResult);
  return Report();
}
