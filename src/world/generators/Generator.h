#ifndef OUTSHINE_WORLD_GENERATORS_GENERATOR_H
#define OUTSHINE_WORLD_GENERATORS_GENERATOR_H

#include "ContactMaterial.h"
#include "Ground.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

template <size_t N>
[[nodiscard]] constexpr bool EveryNoteNamed(const char *const (&names)[N]) {
  for (size_t at = 0; at < N; ++at) {
    if (names[at] == nullptr || names[at][0] == 0) { return false; }
  }
  return true;
}

class Generator {
public:
  virtual ~Generator() = default;
  Generator(const Generator &) = delete;
  Generator &operator=(const Generator &) = delete;

  virtual void Occupy(const Ground &ground, Yield &yield) const noexcept = 0;

  [[nodiscard]] virtual uint32_t Proposes(double areaM2) const noexcept = 0;

  [[nodiscard]] virtual bool At(const Ground &ground, double eastM, double northM,
                                Body *out) const noexcept = 0;

  virtual Span<const char *const> NoteNames() const noexcept { return Span<const char *const>(); }

protected:
  Generator() = default;
};

}
#endif
