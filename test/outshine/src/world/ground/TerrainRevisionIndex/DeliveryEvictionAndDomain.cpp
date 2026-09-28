#include "TerrainRevisionIndex.h"
#include "Check.h"
#include <array>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  CHECK(!TerrainRevisionIndex::Create(0), "zero capacity is rejected");
  CHECK(!TerrainRevisionIndex::Create(65537), "excess capacity is rejected");
  auto made = TerrainRevisionIndex::Create(2);
  CHECK(made.has_value(), "bounded index is created");
  if (!made) { return Report(); }
  auto &index = **made;
  const Data::TileId a{.Zoom = 2, .X = 0, .Y = 1};
  const Data::TileId b{.Zoom = 2, .X = 1, .Y = 1};
  const Data::TileId c{.Zoom = 2, .X = 2, .Y = 1};
  const auto original = *index.IssueDeliveryStamp(a);
  const auto neighbour = *index.IssueDeliveryStamp(b);
  const auto payloadBytes = index.PayloadCapacityBytes();
  CHECK(index.CurrentStamp(a) == original, "cached bytes retain their revision");
  CHECK(index.AreCurrent(std::array{original, neighbour}), "coherent dependencies validate");
  CHECK(!index.AreCurrent({}), "empty evidence never certifies");
  const auto replacement = *index.IssueDeliveryStamp(a);
  CHECK(replacement.RegistrationRevision != original.RegistrationRevision,
        "fresh delivery receives a new stamp");
  CHECK(!index.AreCurrent(std::array{original}), "fresh delivery revokes older bytes");
  CHECK(index.AreCurrent(std::array{neighbour}), "another address is unaffected");
  CHECK(!index.AreCurrent(std::array{original, neighbour}),
        "one stale dependency rejects the certificate");
  CHECK(index.CurrentStamp(b) == neighbour, "reads preserve delivery order");
  const auto third = *index.IssueDeliveryStamp(c);
  CHECK(!index.CurrentStamp(b), "oldest delivery is evicted despite recent read");
  CHECK(!index.AreCurrent(std::array{neighbour}), "metadata eviction revokes the certificate");
  CHECK(index.AreCurrent(std::array{replacement, third}), "retained metadata validates");
  const auto returned = *index.IssueDeliveryStamp(b);
  CHECK(returned.RegistrationRevision != neighbour.RegistrationRevision,
        "re-entry never reuses a stamp");
  CHECK(!index.AreCurrent(std::array{replacement}), "re-entry evicts the next oldest delivery");
  CHECK(index.EntryCount() == 2, "metadata stays bounded");
  CHECK(index.PayloadCapacityBytes() == payloadBytes, "updates do not grow reserved storage");
  auto other = TerrainRevisionIndex::Create(2);
  const auto foreign = *(**other).IssueDeliveryStamp(b);
  auto matchedDomain = TerrainRevisionIndex::Create(2);
  const auto matched = *(**matchedDomain).IssueDeliveryStamp(b);
  CHECK(matched.RegistrationRevision == foreign.RegistrationRevision,
        "independent domains reuse numeric revisions");
  CHECK(!(**matchedDomain).AreCurrent(std::array{foreign}),
        "equal numeric revisions cannot cross domains");
  other->reset();
  CHECK(!(**matchedDomain).AreCurrent(std::array{foreign}),
        "destroyed owner cannot certify a replacement");
  CHECK(!index.AreCurrent(std::array{foreign}), "another owner's stamp cannot validate");
  CHECK(!(**matchedDomain).AreCurrent(std::array{returned}),
        "old domain cannot validate in a new owner");
  auto altered = returned;
  altered.Requested = c;
  CHECK(!index.AreCurrent(std::array{altered}), "stamp cannot migrate between addresses");
  altered = returned;
  altered.RegistrationRevision = 0;
  CHECK(!index.AreCurrent(std::array{altered}), "unknown revision is rejected");
  altered = returned;
  altered.Owner.reset();
  CHECK(!index.AreCurrent(std::array{altered}), "missing domain is rejected");
  for (const auto invalid : std::array{Data::TileId{.Zoom = -1},
                                       Data::TileId{.Zoom = 31},
                                       Data::TileId{.Zoom = 2, .X = 4},
                                       Data::TileId{.Zoom = 2, .Y = 4}}) {
    CHECK(!index.IssueDeliveryStamp(invalid), "invalid tile cannot enter the index");
    CHECK(!index.CurrentStamp(invalid), "invalid tile has no metadata");
  }
  CHECK(index.AreCurrent(std::array{returned}), "invalid observations leave valid evidence intact");
  CHECK(
      index.IssueDeliveryStamp({.Zoom = 30, .X = (1U << 30) - 1, .Y = (1U << 30) - 1}).has_value(),
      "maximum zoom and coordinates are valid");
  CHECK(TerrainRevisionIndex::Create(65536).has_value(), "maximum configured capacity is valid");
  {
    auto constrained = TerrainRevisionIndex::Create(1);
    auto &small = **constrained;
    const auto first = *small.IssueDeliveryStamp(a);
    CHECK(small.InspectStamps(std::array{first}) == TerrainRevisionIndex::Validation::Current,
          "matching registration and delivery are current");
    CHECK(small.IssueDeliveryStamp(b).has_value(), "neighbour evicts metadata");
    CHECK(small.InspectStamps(std::array{first}) == TerrainRevisionIndex::Validation::Unknown,
          "metadata eviction loses proof without claiming changed bytes");
    const auto restored = small.RestoreCachedStamp(first);
    CHECK(restored && restored->DeliveryRevision == first.DeliveryRevision &&
              restored->RegistrationRevision != first.RegistrationRevision,
          "cached bytes retain delivery identity with a fresh registration");
    CHECK(small.InspectStamps(std::array{first}) == TerrainRevisionIndex::Validation::Unknown,
          "old registration is uncertified even when bytes are unchanged");
    CHECK(restored && small.AreCurrent(std::array{*restored}), "restored registration is current");
    CHECK(restored && small.RestoreCachedStamp(first) == restored,
          "restoring identical bytes is idempotent while resident");
    const auto changed = *small.IssueDeliveryStamp(a);
    CHECK(changed.DeliveryRevision != first.DeliveryRevision,
          "fresh delivery changes byte identity");
    CHECK(small.InspectStamps(std::array{first}) == TerrainRevisionIndex::Validation::Stale,
          "known changed bytes are stale");
    const auto staleRestore = small.RestoreCachedStamp(first);
    CHECK(!staleRestore && staleRestore.error() == TerrainRevisionIndex::Error::StaleDelivery,
          "old cached bytes cannot overwrite a known newer delivery");
    CHECK(small.AreCurrent(std::array{changed}), "failed restoration preserves newer metadata");
    auto impossible = changed;
    impossible.RegistrationRevision += 100;
    const auto invented = small.RestoreCachedStamp(impossible);
    CHECK(!invented && invented.error() == TerrainRevisionIndex::Error::InvalidStamp,
          "registration never issued by the domain cannot restore metadata");
    impossible = changed;
    impossible.DeliveryRevision = impossible.RegistrationRevision + 1;
    CHECK(!small.RestoreCachedStamp(impossible),
          "delivery cannot postdate its registration counter");
    CHECK(!small.RestoreCachedStamp(foreign),
          "foreign cached bytes cannot register in this domain");
  }
  {
    auto mixed = TerrainRevisionIndex::Create(2);
    const auto unknown = *(**mixed).IssueDeliveryStamp(a);
    const auto stale = *(**mixed).IssueDeliveryStamp(b);
    CHECK((**mixed).IssueDeliveryStamp(c).has_value(), "first dependency becomes unknown");
    CHECK((**mixed).IssueDeliveryStamp(b).has_value(), "second dependency changes");
    CHECK((**mixed).InspectStamps(std::array{unknown, stale}) ==
              TerrainRevisionIndex::Validation::Stale,
          "known conflict takes precedence over unknown evidence");
  }
  return Report();
}
