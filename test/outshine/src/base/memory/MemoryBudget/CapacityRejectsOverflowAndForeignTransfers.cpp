#include "MemoryBudget.h"
#include "Check.h"
#include <cstddef>
#include <limits>

int main() {
  using outshine::MemoryBudget;
  using namespace outshine::Test;
  const MemoryBudget empty(0);
  auto zero = empty.TryReserve(0);
  CHECK(zero && !zero->Resize(1) && !empty.TryReserve(1),
        "zero capacity accepts no positive bytes");
  MemoryBudget::Reservation unbound;
  CHECK(unbound.Resize(0) && !unbound.Resize(1), "an unbound owner cannot invent a budget");
  constexpr size_t maximum = std::numeric_limits<size_t>::max();
  const MemoryBudget large(maximum);
  auto almost = large.TryReserve(maximum - 1);
  auto last = large.TryReserve(1);
  CHECK(almost && last && large.Used() == maximum, "full size_t range is represented exactly");
  if (!almost || !last) { return Report(); }
  CHECK(!large.TryReserve(1) && !last->Resize(2) && last->Bytes() == 1,
        "addition cannot wrap into an accepted reservation");
  CHECK(last->TransferTo(*almost, 1) && almost->Bytes() == maximum && large.Used() == maximum,
        "transfer at the exact upper boundary does not overflow");
  CHECK(almost->TransferTo(*last, maximum),
        "the complete reservation can transfer without double accounting");
  const MemoryBudget foreign(maximum);
  auto other = foreign.TryReserve(1);
  CHECK(other && !last->TransferTo(*other, 1) && !last->TransferTo(*other, 0),
        "different budgets cannot merge even for an empty transfer");
  CHECK(large.Used() == maximum && foreign.Used() == 1,
        "foreign rejection leaves both budgets intact");
  last.reset();
  almost.reset();
  CHECK(large.Used() == 0 && large.TryReserve(maximum), "destruction restores all capacity");
  Covers("CapacityRejectsOverflowAndForeignTransfers; logical reserved bytes, not physical process "
         "memory");
  return Report();
}
