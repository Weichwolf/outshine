#include <span>
#include "Yield.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace outshine::Generators {

Yield::Yield(OccupancySink &space,
             std::span<const char *const> names,
             std::span<Note> notes) noexcept
    : Space_(&space), Notes_(notes) {
  assert(names.size() == notes.size());
  for (size_t i = 0; i < names.size(); i++) {
    Notes_[i] = Note{.Name = names[i], .Times = 0, .Peak = 0.0, .Raised = false};
  }
}

Claim Yield::Place(const Body &body) noexcept {
  const uint32_t before = Space_->Claims(Claim::Outcome::Placed);
  const Claim claim = Space_->Place(body);
  Claims_[static_cast<size_t>(claim.Why())]++;
  if (claim.Why() == Claim::Outcome::Placed) {
    if (Range_.Count == 0) { Range_.First = before; }
    Range_.Count++;
  }
  return claim;
}

} // namespace outshine::Generators
