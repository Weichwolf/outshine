#ifndef OUTSHINE_BASE_SCENE_LEVELOFDETAIL_H
#define OUTSHINE_BASE_SCENE_LEVELOFDETAIL_H

#include <cstdint>

namespace outshine {

enum class LevelOfDetail : uint8_t {
  Fine,
  Shell,
  Massed,
  Skyline,
};

[[nodiscard]] constexpr LevelOfDetail LevelOfDetailForRung(int rungsCoarser) noexcept {
  if (rungsCoarser <= 0) { return LevelOfDetail::Fine; }
  if (rungsCoarser == 1) { return LevelOfDetail::Shell; }
  if (rungsCoarser == 2) { return LevelOfDetail::Massed; }
  return LevelOfDetail::Skyline;
}

[[nodiscard]] constexpr LevelOfDetail Coarser(LevelOfDetail one, LevelOfDetail two) noexcept {
  return static_cast<uint8_t>(one) > static_cast<uint8_t>(two) ? one : two;
}

static_assert(LevelOfDetailForRung(-1) == LevelOfDetail::Fine);
static_assert(LevelOfDetailForRung(0) == LevelOfDetail::Fine);
static_assert(LevelOfDetailForRung(1) == LevelOfDetail::Shell);
static_assert(LevelOfDetailForRung(2) == LevelOfDetail::Massed);
static_assert(LevelOfDetailForRung(9) == LevelOfDetail::Skyline);
static_assert(Coarser(LevelOfDetail::Fine, LevelOfDetail::Massed) == LevelOfDetail::Massed);
static_assert(Coarser(LevelOfDetail::Skyline, LevelOfDetail::Shell) == LevelOfDetail::Skyline);

}

#endif
