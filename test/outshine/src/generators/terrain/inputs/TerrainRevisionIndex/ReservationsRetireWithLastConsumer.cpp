#include "Check.h"
#include "TerrainRevisionIndex.h"

#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Index = Ground::TerrainRevisionIndex;
  auto created = Index::Create(1);
  CHECK(created.has_value(), "small unreserved source cache is supported");
  if (!created) { return Report(); }
  auto &index = **created;
  const std::array wanted{Data::TileId{.Zoom = 8, .X = 0, .Y = 1},
                          Data::TileId{.Zoom = 8, .X = 1, .Y = 1}};
  auto reserved = index.ReserveFor(wanted);
  CHECK(reserved.has_value(), "resident source set obtains a lifetime reservation");
  if (!reserved) { return Report(); }
  auto snapshot = *reserved;
  reserved->reset();
  const std::array proof{*index.IssueDeliveryStamp(wanted[0]),
                         *index.IssueDeliveryStamp(wanted[1])};
  const size_t bytes = index.PayloadCapacityBytes();
  for (uint32_t x = 0; x < 100; ++x) {
    CHECK(index.IssueDeliveryStamp({.Zoom = 8, .X = x, .Y = 0}).has_value(),
          "unreserved source cache continues accepting deliveries");
  }
  CHECK(index.AreCurrent(proof) && index.EntryCount() == 3,
        "one snapshot retains its contributors while unrelated deliveries cycle");
  snapshot.reset();
  for (uint32_t x = 100; x < 104; ++x) {
    CHECK(index.IssueDeliveryStamp({.Zoom = 8, .X = x, .Y = 0}).has_value(),
          "release permits normal source metadata eviction");
  }
  CHECK(!index.AreCurrent(proof) && index.PayloadCapacityBytes() == bytes,
        "last consumer releases protection without growing metadata history");
  const std::array moved{Data::TileId{.Zoom = 8, .X = 2, .Y = 1},
                         Data::TileId{.Zoom = 8, .X = 3, .Y = 1}};
  const auto replacement = index.ReserveFor(moved);
  CHECK(replacement && index.PayloadCapacityBytes() == bytes,
        "new location reuses the bounded metadata reservation");
  return Report();
}
