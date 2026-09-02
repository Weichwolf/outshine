#ifndef OUTSHINE_GENERATORS_BASE_MAKING_H
#define OUTSHINE_GENERATORS_BASE_MAKING_H

#include <span>
#include <array>
#include "ContactMaterial.h"
#include "Ground.h"
#include "Yield.h"

namespace outshine::Generators {

template <size_t N>
[[nodiscard]] constexpr bool EveryNoteNamed(const std::array<const char *const, N> &names) {
  for (size_t at = 0; at < N; ++at) {
    if (names[at] == nullptr || names[at][0] == 0) { return false; }
  }
  return true;
}

class Making {
public:
  virtual ~Making() = default;
  Making(const Making &) = delete;
  Making &operator=(const Making &) = delete;

  virtual void Occupy(const Ground &ground, Yield &yield) const noexcept = 0;

  [[nodiscard]] virtual std::span<const char *const> NoteNames() const noexcept { return {}; }

protected:
  Making() = default;
};

} // namespace outshine::Generators
#endif
