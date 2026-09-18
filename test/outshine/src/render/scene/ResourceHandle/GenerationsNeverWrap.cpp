#include "ResourceHandle.h"
#include "Check.h"
#include <limits>
#include <type_traits>

using namespace outshine::Render;
static_assert(!PieceHandle{});
static_assert(!std::is_convertible_v<uint32_t, PieceHandle>);
static_assert(!std::is_convertible_v<PieceHandle, uint32_t>);

int main() {
  using namespace outshine::Test;
  ResourceSlotState slot{.Generation = std::numeric_limits<uint64_t>::max() - 1, .Occupied = true};
  CHECK(slot.Matches(slot.Generation) && !slot.Matches(0), "occupied slot checks generation");
  CHECK(slot.Release() && !slot.Occupied && slot.Generation == std::numeric_limits<uint64_t>::max(),
        "last available generation may be reused exactly once");
  CHECK(!slot.Release() && slot.Generation == std::numeric_limits<uint64_t>::max(),
        "repeated release consumes no generation");
  slot.Occupied = true;
  CHECK(!slot.Release() && !slot.Occupied &&
            slot.Generation == std::numeric_limits<uint64_t>::max(),
        "exhausted generation retires the slot instead of wrapping into an old identity");
  CHECK(!slot.Matches(std::numeric_limits<uint64_t>::max()) && !slot.Matches(0),
        "retired slot cannot resolve any handle");
  return Report();
}
