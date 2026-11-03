#include "ByteLease.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<uint8_t> bytes{1, 2, 3};
  bytes.reserve(32);
  const size_t capacity = bytes.capacity();
  const auto *data = bytes.data();
  const MemoryBudget budget(capacity + 1);
  auto reservation = budget.TryReserve(capacity - 1);
  CHECK(reservation, "an intentionally short reservation exists");
  if (!reservation) { return Report(); }
  CHECK(!ByteLease::Adopt(bytes, *reservation) && bytes.data() == data && bytes.size() == 3 &&
            reservation->Bytes() == capacity - 1 && budget.Used() == capacity - 1,
        "underbooked adoption rejects without consuming the source buffer or booking");
  CHECK(reservation->Resize(capacity + 1), "the test can also overbook the same allocation");
  CHECK(!ByteLease::Adopt(bytes, *reservation) && bytes.data() == data &&
            reservation->Bytes() == capacity + 1,
        "the lease requires an exact allocation booking rather than hiding extra workspace");
  CHECK(reservation->Resize(capacity), "the exact capacity can replace the rejected booking");
  auto valid = ByteLease::Adopt(bytes, *reservation);
  CHECK(valid && valid->Bytes().data() == data && budget.Used() == capacity,
        "the preserved owners remain usable for successful adoption");
  valid.reset();
  CHECK(budget.Used() == 0, "rejected attempts never leave phantom charges");
  std::vector<uint8_t> oldBytes{1, 2};
  std::vector<uint8_t> nextBytes{3, 4, 5};
  const MemoryBudget shared(oldBytes.capacity() + nextBytes.capacity());
  auto oldBooking = shared.TryReserve(oldBytes.capacity());
  auto nextBooking = shared.TryReserve(nextBytes.capacity());
  CHECK(oldBooking && nextBooking, "independent blocks reserve the shared capacity");
  if (!oldBooking || !nextBooking) { return Report(); }
  auto oldBlock = ByteBlock::Adopt(oldBytes, *oldBooking);
  auto nextBlock = ByteBlock::Adopt(nextBytes, *nextBooking);
  CHECK(oldBlock && nextBlock, "both standalone blocks adopt without heap ownership wrappers");
  if (oldBlock && nextBlock) {
    const auto nextCapacity = nextBlock->CapacityBytes();
    *oldBlock = std::move(*nextBlock);
    CHECK(oldBlock->Bytes().size() == 3 && oldBlock->Bytes().front() == 3 &&
              shared.Used() == nextCapacity,
          "move assignment replaces the old allocation and releases only its reservation");
    oldBlock.reset();
    CHECK(shared.Used() == 0, "replacement ownership releases once at final destruction");
  }
  const ByteLease empty;
  CHECK(empty.Bytes().empty() && empty.CapacityBytes() == 0, "a default lease holds no resource");
  Covers("reservation boundary, strong failure ownership, exact capacity and empty lease");
  return Report();
}
