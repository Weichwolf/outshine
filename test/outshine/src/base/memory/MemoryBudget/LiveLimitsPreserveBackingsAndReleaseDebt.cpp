#include "MemoryBudget.h"
#include "ByteLease.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<uint8_t> bytes(257, 93);
  const auto capacity = bytes.capacity();
  MemoryBudget budget(capacity);
  auto copy = budget;
  auto booking = budget.TryReserve(capacity);
  CHECK(booking, "the complete native capacity is admitted before adoption");
  if (!booking) { return Report(); }
  auto lease = ByteLease::Adopt(bytes, *booking);
  CHECK(lease && lease->Bytes().size() == 257, "the immutable backing adopts its reservation");
  if (!lease) { return Report(); }
  copy.SetMaximum(0);
  const auto overdrawn = budget.Read();
  CHECK(overdrawn.Maximum == 0 && overdrawn.Used == capacity && overdrawn.Available() == 0 &&
            budget.Available() == 0 && !budget.TryReserve(1),
        "a copied owner lowers admission without unsigned room or revoking the live backing");
  CHECK(lease->Bytes().front() == 93 && lease->Bytes().back() == 93,
        "all immutable bytes remain readable while reservations exceed the new limit");
  auto zero = budget.TryReserve(0);
  CHECK(zero && !zero->Resize(1), "zero-byte handles remain valid but cannot acquire new bytes");
  auto alias = lease;
  lease.reset();
  CHECK(budget.Used() == capacity, "every alias preserves the original allocation booking");
  {
    copy.SetMaximum(capacity + 19);
    auto growth = budget.TryReserve(19);
    CHECK(growth && !budget.TryReserve(1), "raising the live limit admits exactly the new room");
    if (growth) {
      budget.SetMaximum(0);
      CHECK(growth->Resize(11) && growth->Bytes() == 11 && !growth->Resize(12),
            "shrinking remains possible in debt while growing is blocked");
      MemoryBudget::Reservation moved;
      CHECK(growth->TransferTo(moved, 11) && moved.Bytes() == 11 && budget.Used() == capacity + 11,
            "same-account ownership transfer neither consumes room nor releases storage early");
      CHECK(moved.Resize(0) && budget.Used() == capacity,
            "release repays debt independently of the current maximum");
    }
    CHECK(alias->Bytes().front() == 93, "budget changes preserve all backing aliases");
  }
  constexpr auto maximum = std::numeric_limits<size_t>::max();
  budget.SetMaximum(maximum);
  auto rest = budget.TryReserve(maximum - capacity);
  CHECK(rest && budget.Used() == maximum && budget.Available() == 0 && !budget.TryReserve(1),
        "live resizing also preserves exact size_t overflow rejection");
  rest.reset();
  alias.reset();
  CHECK(budget.Used() == 0, "the last backing returns its bytes after every runtime limit change");
  Covers(
      "live logical admission, retained immutable bytes and debt release; no physical RAM claim");
  return Report();
}
