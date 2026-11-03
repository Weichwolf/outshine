#include "ByteLease.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<uint8_t> bytes{1, 2, 3, 4};
  bytes.reserve(64);
  const auto *data = bytes.data();
  const size_t capacity = bytes.capacity();
  const MemoryBudget budget(capacity);
  auto reservation = budget.TryReserve(capacity);
  CHECK(reservation, "native allocation capacity is reserved before adoption");
  if (!reservation) { return Report(); }
  auto lease = ByteLease::Adopt(bytes, *reservation);
  CHECK(lease && lease->Bytes().data() == data && lease->CapacityBytes() == capacity &&
            lease->Bytes().size() == 4 && budget.Used() == capacity,
        "adoption preserves storage and charges capacity rather than logical size");
  if (!lease) { return Report(); }
  CHECK(reservation->Bytes() == 0 && bytes.empty(), "only the backing owns the transferred inputs");
  auto alias = *lease;
  lease.reset();
  CHECK(alias.Bytes().data() == data && budget.Used() == capacity,
        "an immutable alias outlives its first handle without new bytes or booking");
  auto moved = std::move(alias);
  CHECK(moved.Bytes().data() == data && budget.Used() == capacity,
        "moving an alias preserves the single reservation");
  ByteLease replacement;
  replacement = moved;
  moved = {};
  CHECK(std::ranges::equal(replacement.Bytes(), std::vector<uint8_t>{1, 2, 3, 4}) &&
            budget.Used() == capacity,
        "copy assignment shares the original bytes after another owner is released");
  replacement = {};
  CHECK(budget.Used() == 0 && budget.TryReserve(capacity),
        "last backing release restores capacity");
  std::optional<ByteLease> survivor;
  {
    std::vector<uint8_t> transient{7, 8};
    const MemoryBudget local(transient.capacity());
    auto held = local.TryReserve(transient.capacity());
    CHECK(held, "the temporary budget admits its bytes");
    if (held) { survivor = ByteLease::Adopt(transient, *held); }
  }
  CHECK(survivor && survivor->Bytes().front() == 7 && survivor->Bytes().back() == 8,
        "the backing and reservation survive the budget facade");
  survivor.reset();
  Covers("zero-copy adoption, immutable aliases, move/copy and final capacity release; logical "
         "bytes only");
  return Report();
}
