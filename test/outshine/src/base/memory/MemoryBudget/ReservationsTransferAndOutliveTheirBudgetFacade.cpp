#include "MemoryBudget.h"
#include "Check.h"
#include <optional>
#include <utility>

int main() {
  using outshine::MemoryBudget;
  using namespace outshine::Test;
  MemoryBudget budget(16);
  auto first = budget.TryReserve(10);
  auto second = budget.TryReserve(6);
  CHECK(first && second && budget.Used() == 16, "independent owners fill one budget");
  if (!first || !second) { return Report(); }
  CHECK(!first->Resize(11) && first->Bytes() == 10 && budget.Used() == 16,
        "failed growth leaves existing ownership intact");
  CHECK(first->TransferTo(*second, 4), "transfer does not require spare capacity");
  CHECK(first->Bytes() == 6 && second->Bytes() == 10 && budget.Used() == 16,
        "split ownership never double books or releases bytes");
  CHECK(!first->TransferTo(*second, 7) && budget.Used() == 16,
        "a source cannot transfer more than it owns");
  CHECK(!first->TransferTo(*first, 1) && first->TransferTo(*first, 0),
        "self transfer only accepts the empty operation");
  MemoryBudget::Reservation moved(std::move(*second));
  CHECK(second->Bytes() == 0 && moved.Bytes() == 10 && budget.Used() == 16,
        "move construction preserves one owner");
  moved = std::move(*first);
  CHECK(first->Bytes() == 0 && moved.Bytes() == 6 && budget.Used() == 6,
        "move assignment releases the previous destination");
  CHECK(moved.Resize(0) && moved.Resize(16) && budget.Used() == 16,
        "a zero reservation retains its budget and can regrow");
  MemoryBudget::Reservation split;
  CHECK(moved.TransferTo(split, 16) && split.Bytes() == 16 && moved.Bytes() == 0,
        "an empty destination adopts the same budget");
  split = {};
  CHECK(budget.Used() == 0, "last reservation returns capacity");
  std::optional<MemoryBudget::Reservation> survivor;
  {
    const MemoryBudget temporary(8);
    survivor = temporary.TryReserve(8);
  }
  CHECK(survivor && survivor->Resize(3) && survivor->Resize(8),
        "the resource owner remains valid after the budget facade dies");
  survivor.reset();
  Covers("ReservationsTransferAndOutliveTheirBudgetFacade; logical reserved bytes, not physical "
         "process memory");
  return Report();
}
