#ifndef OUTSHINE_GENERATORS_GENERATOR_H
#define OUTSHINE_GENERATORS_GENERATOR_H

#include "Body.h"
#include "Ground.h"
#include "Span.h"
#include "Yield.h"

namespace outshine::Generators {

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
