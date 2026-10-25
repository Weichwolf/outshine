#include "Check.h"
#include "TerrainRevisionIndex.h"

#include <array>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Index = Ground::TerrainRevisionIndex;
  auto created = Index::Create(2);
  CHECK(created.has_value(), "small metadata budget is supported");
  if (!created) { return Report(); }
  auto &index = **created;
  const std::array retained{*index.IssueDeliveryStamp({.Zoom = 4, .X = 0, .Y = 0}),
                            *index.IssueDeliveryStamp({.Zoom = 4, .X = 0, .Y = 1})};
  const size_t initialBytes = index.PayloadCapacityBytes();
  std::vector<Data::TileId> planned;
  for (uint32_t y = 1; y <= 6; ++y) {
    for (uint32_t x = 1; x <= 6; ++x) { planned.push_back({.Zoom = 4, .X = x, .Y = y}); }
  }
  planned.push_back(planned.front());
  planned.push_back(retained.front().Requested);
  auto reservation = index.ReserveFor(planned);
  CHECK(reservation.has_value(), "reserve active source dependencies");
  const size_t preparedBytes = index.PayloadCapacityBytes();
  CHECK(preparedBytes > initialBytes && index.AreCurrent(retained),
        "growth retains exact registration and delivery identities");
  std::vector<Index::Stamp> arrivals;
  for (uint32_t y = 1; y <= 6; ++y) {
    for (uint32_t x = 1; x <= 6; ++x) {
      arrivals.push_back(*index.IssueDeliveryStamp({.Zoom = 4, .X = x, .Y = y}));
    }
  }
  CHECK(index.EntryCount() == 38 && index.AreCurrent(retained) && index.AreCurrent(arrivals),
        "all contributors remain certified while the complete working set arrives");
  CHECK(index.ReserveFor(planned).has_value() &&
            index.PayloadCapacityBytes() <= preparedBytes + sizeof(Index::Reservation),
        "overlapping and repeated demand does not enlarge the metadata reservation");
  const std::array invalid{Data::TileId{.Zoom = 4, .X = 16, .Y = 0}};
  const auto rejected = index.ReserveFor(invalid);
  CHECK(!rejected && rejected.error() == Index::Error::InvalidTile && index.AreCurrent(arrivals),
        "invalid requests preserve resident proofs");
  std::vector<Data::TileId> excessive;
  for (uint32_t y = 0; y < 256; ++y) {
    for (uint32_t x = 0; x < 256; ++x) { excessive.push_back({.Zoom = 8, .X = x, .Y = y}); }
  }
  const auto overBudget = index.ReserveFor(excessive);
  CHECK(!overBudget && overBudget.error() == Index::Error::InvalidCapacity &&
            index.PayloadCapacityBytes() == preparedBytes && index.EntryCount() == 38 &&
            index.AreCurrent(retained),
        "the hard source metadata ceiling refuses growth without evicting active evidence");
  return Report();
}
