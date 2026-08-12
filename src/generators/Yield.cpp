#include "Yield.h"

#include <cassert>

namespace outshine::Generators {

Yield::Yield(OccupancySink &space, Span<const char *const> names, Span<Note> notes) noexcept
    : Space_(&space), Notes_(notes) {
  assert(names.Size() == notes.Size());
  for (size_t i = 0; i < names.Size(); i++) Notes_[i] = Note{names[i], 0, 0.0, false};
}

Claim Yield::Place(const Body &body) noexcept {
  /* Taken before the claim, because where this generator's stretch of the shared sink begins is only
   * known at its first success — the yields of a whole set are built before any of them runs. */
  const uint32_t before = Space_->Claims(Claim::Outcome::Placed);
  const Claim claim = Space_->Place(body);
  Claims_[(size_t)claim.Why()]++;
  if (claim.Why() == Claim::Outcome::Placed) {
    if (Range_.Count == 0) Range_.First = before;
    Range_.Count++;
  }
  return claim;
}

} // namespace outshine::Generators
