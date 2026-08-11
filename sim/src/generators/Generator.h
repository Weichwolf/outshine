#ifndef GENERATOR_H
#define GENERATOR_H

#include "Body.h"
#include "Ground.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

/* const and without mutable state, which is what pays for N regions at once with no lock. noexcept
 * is honest because the heap is fixed and a refusal travels through the yield. The region is not a
 * parameter: the ground is of exactly one, says which, and a second one beside it could disagree. */
class Generator {
public:
  virtual ~Generator() = default;
  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  virtual void Occupy(const Ground &ground, Yield &yield) const noexcept = 0;
  /* What stands at one place, with no buffer existing. The answer is a proposal and not a
   * placement: nothing claimed the space. */
  [[nodiscard]] virtual bool At(const Ground &ground, double eastM, double northM,
                                Body *out) const noexcept = 0;
  /* What this generator wants counted beyond the claim, declared once and read by nobody but a
   * telemetry line. */
  virtual Span<const char *const> NoteNames() const noexcept { return Span<const char *const>(); }

protected:
  Generator() = default;
};

} // namespace outshine::Generators
#endif
